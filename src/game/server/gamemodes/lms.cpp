/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include "lms.h"
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CGameControllerLMS::CGameControllerLMS() :
	IGameController()
{
	m_pGameType = "LMS";
	m_GameFlags = IGF_SURVIVAL;
}

// event
void CGameControllerLMS::OnCharacterSpawn(CCharacter *pChr)
{
	IGameController::OnCharacterSpawn(pChr);

	// give start equipment
	pChr->GiveWeapon(WEAPON_SHOTGUN, 10);
	pChr->GiveWeapon(WEAPON_GRENADE, 10);
	pChr->GiveWeapon(WEAPON_LASER, 5);

	// prevent respawn
	pChr->GetPlayer()->m_RespawnDisabled = GetStartRespawnState();
}

bool CGameControllerLMS::OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number)
{
	// bypass pickups
	if(Index >= ENTITY_ARMOR_1 && Index <= ENTITY_WEAPON_LASER)
		return true;
	return false;
}

// game
bool CGameControllerLMS::DoWincheckRound()
{
	// check for time based win
	if(m_GameInfo.m_TimeLimit > 0 && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS &&
				(!GameServer()->m_apPlayers[i]->m_RespawnDisabled ||
					(GameServer()->m_apPlayers[i]->GetCharacter() && GameServer()->m_apPlayers[i]->GetCharacter()->IsAlive())))
				GameServer()->m_apPlayers[i]->m_Score++;
		}

		EndRound();
		return true;
	}
	else
	{
		// check for survival win
		CPlayer *pAlivePlayer = 0;
		int AlivePlayerCount = 0;
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS &&
				(!GameServer()->m_apPlayers[i]->m_RespawnDisabled ||
					(GameServer()->m_apPlayers[i]->GetCharacter() && GameServer()->m_apPlayers[i]->GetCharacter()->IsAlive())))
			{
				++AlivePlayerCount;
				pAlivePlayer = GameServer()->m_apPlayers[i];
			}
		}

		if(AlivePlayerCount == 0) // no winner
		{
			EndRound();
			return true;
		}
		else if(AlivePlayerCount == 1) // 1 winner
		{
			pAlivePlayer->m_Score++;
			EndRound();
			return true;
		}
	}

	return false;
}
