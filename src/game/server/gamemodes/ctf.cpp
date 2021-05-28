/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include <game/mapitems.h>

#include "ctf.h"
#include <game/server/entities/character.h>
#include <game/server/entities/flag.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/weapons.h>

CGameControllerCTF::CGameControllerCTF() :
	IGameController()
{
	m_pGameType = "CTF";
	m_GameFlags = IGF_TEAMS | IGF_FLAGS | IGF_SUDDENDEATH;
}

void CGameControllerCTF::OnInit()
{
	// game
	m_apFlags[0] = 0;
	m_apFlags[1] = 0;
}

void CGameControllerCTF::OnCharacterSpawn(CCharacter *pChr)
{
	pChr->IncreaseHealth(10);

	pChr->GiveWeapon(WEAPON_GUN, WEAPON_ID_PISTOL, 10);
	pChr->GiveWeapon(WEAPON_HAMMER, WEAPON_ID_HAMMER, -1);
}

// balancing
bool CGameControllerCTF::CanBeMovedOnBalance(CPlayer *pPlayer) const
{
	CCharacter *Character = pPlayer->GetCharacter();
	if(Character)
	{
		for(int fi = 0; fi < 2; fi++)
		{
			CFlag *F = m_apFlags[fi];
			if(F && F->GetCarrier() == Character)
				return false;
		}
	}
	return true;
}

// event
int CGameControllerCTF::OnCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, int WeaponID)
{
	int HadFlag = DEATH_NORMAL;

	// drop flags
	for(int i = 0; i < 2; i++)
	{
		CFlag *F = m_apFlags[i];
		if(F && pKiller && pKiller->GetCharacter() && F->GetCarrier() == pKiller->GetCharacter())
			HadFlag |= DEATH_KILLER_HAS_FLAG;
		if(F && F->GetCarrier() == pVictim)
		{
			SendGameMsg(GAMEMSG_CTF_DROP, -1);
			F->Drop();

			if(pKiller && pKiller->GetTeam() != pVictim->GetPlayer()->GetTeam())
				pKiller->m_Score++;

			HadFlag |= DEATH_VICTIM_HAS_FLAG;
		}
	}

	return HadFlag;
}

void CGameControllerCTF::OnFlagReset(CFlag *pFlag)
{
	SendGameMsg(GAMEMSG_CTF_RETURN, -1);
}

bool CGameControllerCTF::OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number)
{
	int Team = -1;
	if(Index == ENTITY_FLAGSTAND_RED)
		Team = TEAM_RED;
	if(Index == ENTITY_FLAGSTAND_BLUE)
		Team = TEAM_BLUE;
	if(Team == -1 || m_apFlags[Team])
		return false;

	CFlag *F = new CFlag(GameWorld(), Team, Pos);
	m_apFlags[Team] = F;
	return true;
}

// game
void CGameControllerCTF::DoWincheckMatch()
{
	// check score win condition
	if((m_GameInfo.m_ScoreLimit > 0 && (m_aTeamscore[TEAM_RED] >= m_GameInfo.m_ScoreLimit || m_aTeamscore[TEAM_BLUE] >= m_GameInfo.m_ScoreLimit)) ||
		(m_GameInfo.m_TimeLimit > 0 && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60))
	{
		if(m_SuddenDeath)
		{
			if(m_aTeamscore[TEAM_RED] / 100 != m_aTeamscore[TEAM_BLUE] / 100)
				EndMatch();
		}
		else
		{
			if(m_aTeamscore[TEAM_RED] != m_aTeamscore[TEAM_BLUE])
				EndMatch();
			else
				m_SuddenDeath = 1;
		}
	}
}

bool CGameControllerCTF::GetFlagState(SFlagState *pState)
{
	pState->m_RedFlagDroppedTick = 0;
	if(m_apFlags[TEAM_RED])
	{
		if(m_apFlags[TEAM_RED]->IsAtStand())
			pState->m_RedFlagCarrier = FLAG_ATSTAND;
		else if(m_apFlags[TEAM_RED]->GetCarrier() && m_apFlags[TEAM_RED]->GetCarrier()->GetPlayer())
			pState->m_RedFlagCarrier = m_apFlags[TEAM_RED]->GetCarrier()->GetPlayer()->GetCID();
		else
		{
			pState->m_RedFlagCarrier = FLAG_TAKEN;
			pState->m_RedFlagDroppedTick = m_apFlags[TEAM_RED]->GetDropTick();
		}
	}
	else
		pState->m_RedFlagCarrier = FLAG_MISSING;

	pState->m_BlueFlagDroppedTick = 0;
	if(m_apFlags[TEAM_BLUE])
	{
		if(m_apFlags[TEAM_BLUE]->IsAtStand())
			pState->m_BlueFlagCarrier = FLAG_ATSTAND;
		else if(m_apFlags[TEAM_BLUE]->GetCarrier() && m_apFlags[TEAM_BLUE]->GetCarrier()->GetPlayer())
			pState->m_BlueFlagCarrier = m_apFlags[TEAM_BLUE]->GetCarrier()->GetPlayer()->GetCID();
		else
		{
			pState->m_BlueFlagCarrier = FLAG_TAKEN;
			pState->m_BlueFlagDroppedTick = m_apFlags[TEAM_BLUE]->GetDropTick();
		}
	}
	else
		pState->m_BlueFlagCarrier = FLAG_MISSING;

	return true;
}

// general
void CGameControllerCTF::OnPostTick()
{
	if(GameWorld()->m_ResetRequested || GameWorld()->m_Paused)
		return;

	for(int fi = 0; fi < 2; fi++)
	{
		CFlag *F = m_apFlags[fi];

		if(!F)
			continue;

		//
		if(F->GetCarrier())
		{
			if(m_apFlags[fi ^ 1] && m_apFlags[fi ^ 1]->IsAtStand())
			{
				if(distance(F->GetPos(), m_apFlags[fi ^ 1]->GetPos()) < CFlag::ms_PhysSize + CCharacter::ms_PhysSize)
				{
					// CAPTURE! \o/
					m_aTeamscore[fi ^ 1] += 100;
					F->GetCarrier()->GetPlayer()->m_Score += 5;
					float Diff = Server()->Tick() - F->GetGrabTick();

					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "flag_capture player='%d:%s' team=%d time=%.2f",
						F->GetCarrier()->GetPlayer()->GetCID(),
						Server()->ClientName(F->GetCarrier()->GetPlayer()->GetCID()),
						F->GetCarrier()->GetPlayer()->GetTeam(),
						Diff / (float)Server()->TickSpeed());
					GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

					int DiffTick = (int)Diff;
					int CarrierCID = F->GetCarrier()->GetPlayer()->GetCID();
					SendGameMsg(GAMEMSG_CTF_CAPTURE, -1, &fi, &CarrierCID, &DiffTick);
					for(int i = 0; i < 2; i++)
						m_apFlags[i]->Reset();
					// do a win check(capture could trigger win condition)
					DoWincheckMatch();
					if(IsEndMatch())
						return;
				}
			}
		}
		else
		{
			CCharacter *apCloseCCharacters[MAX_CLIENTS];
			int Num = GameWorld()->FindEntities(F->GetPos(), CFlag::ms_PhysSize, (CEntity **)apCloseCCharacters, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for(int i = 0; i < Num; i++)
			{
				if(!apCloseCCharacters[i]->IsAlive() || apCloseCCharacters[i]->GetPlayer()->GetTeam() == TEAM_SPECTATORS || GameServer()->Collision()->IntersectLine(F->GetPos(), apCloseCCharacters[i]->GetPos(), NULL, NULL))
					continue;

				if(apCloseCCharacters[i]->GetPlayer()->GetTeam() == F->GetTeam())
				{
					// return the flag
					if(!F->IsAtStand())
					{
						CCharacter *pChr = apCloseCCharacters[i];
						pChr->GetPlayer()->m_Score += 1;

						char aBuf[256];
						str_format(aBuf, sizeof(aBuf), "flag_return player='%d:%s' team=%d",
							pChr->GetPlayer()->GetCID(),
							Server()->ClientName(pChr->GetPlayer()->GetCID()),
							pChr->GetPlayer()->GetTeam());
						GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
						SendGameMsg(GAMEMSG_CTF_RETURN, -1);
						F->Reset();
					}
				}
				else
				{
					// take the flag
					if(F->IsAtStand())
						m_aTeamscore[fi ^ 1]++;

					F->Grab(apCloseCCharacters[i]);

					F->GetCarrier()->GetPlayer()->m_Score += 1;

					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "flag_grab player='%d:%s' team=%d",
						F->GetCarrier()->GetPlayer()->GetCID(),
						Server()->ClientName(F->GetCarrier()->GetPlayer()->GetCID()),
						F->GetCarrier()->GetPlayer()->GetTeam());
					GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
					SendGameMsg(GAMEMSG_CTF_GRAB, -1, &fi);
					break;
				}
			}
		}
	}
	// do a win check(grabbing flags could trigger win condition)
	DoWincheckMatch();
}
