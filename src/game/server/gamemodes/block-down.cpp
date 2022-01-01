#include "block-down.h"

#include <game/server/ModCore/BlockDown/player.h>
#include <game/server/entities/character.h>
#include <game/server/weapons.h>

CGameControllerBD::CGameControllerBD() :
	IGameController()
{
	m_pGameType = "BlockDown";
	m_GameFlags = IGF_TEAMS | IGF_SUDDENDEATH;
}

void CGameControllerBD::OnCharacterSpawn(CCharacter *pChr)
{
	if(pChr->GetPlayer()->GetTeam() == TEAM_RED)
	{
		pChr->GiveWeapon(WEAPON_HAMMER, WEAPON_ID_INFHAMMER, -1);
	}
	else if(pChr->GetPlayer()->GetTeam() == TEAM_BLUE)
	{
		pChr->GiveWeapon(WEAPON_LASER, WEAPON_ID_FREEZELASER, 10);
	}
	pChr->IncreaseHealth(10);
	pChr->GiveWeapon(WEAPON_GUN, WEAPON_ID_USELESSGUN, -1);
}

int CGameControllerBD::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	if(pVictim->GetPlayer()->GetTeam() == TEAM_BLUE)
		DoTeamChange(VictimCID, TEAM_RED, false);
	
	pVictim->GetPlayer()->m_RespawnTick = 50;

	return DEATH_NORMAL;
}

void CGameControllerBD::DoWincheckMatch()
{
	char aBuf[64];
	if(IsTeamplay())
	{
		if(((m_GameInfo.m_TimeLimit > 0 && !IsRoundTimer() && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60)))
		{
			str_format(aBuf, sizeof(aBuf), "Humans win this round");
			GameServer()->SendChatTarget(-1, aBuf);
			m_aTeamscore[TEAM_BLUE] = 1000;
			EndMatch();
		}
		else if(m_aTeamSize[TEAM_BLUE] == 0)
		{
			m_aTeamscore[TEAM_RED] = 1000;
			str_format(aBuf, sizeof(aBuf), "Zombies win this round in: %i", Server()->Tick() - m_GameStartTick);
			GameServer()->SendChatTarget(-1, aBuf);
			EndMatch();
		}
	}
}