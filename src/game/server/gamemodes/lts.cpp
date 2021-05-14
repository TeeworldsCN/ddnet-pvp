/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include "lts.h"
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/weapons.h>

CGameControllerLTS::CGameControllerLTS() :
	IGameController()
{
	m_pGameType = "LTS";
	m_GameFlags = IGF_TEAMS | IGF_SURVIVAL;
}

// event
void CGameControllerLTS::OnCharacterSpawn(class CCharacter *pChr)
{
	pChr->IncreaseHealth(10);

	// give start equipment
	pChr->GiveWeapon(WEAPON_GUN, WEAPON_TYPE_PISTOL, 10);
	pChr->GiveWeapon(WEAPON_HAMMER, WEAPON_TYPE_HAMMER, -1);
	pChr->GiveWeapon(WEAPON_SHOTGUN, WEAPON_TYPE_SHOTGUN, 10);
	pChr->GiveWeapon(WEAPON_GRENADE, WEAPON_TYPE_GRENADE, 10);
	pChr->GiveWeapon(WEAPON_LASER, WEAPON_TYPE_LASER, 5);
}

bool CGameControllerLTS::OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number)
{
	// bypass pickups
	if(Index >= ENTITY_ARMOR_1 && Index <= ENTITY_WEAPON_LASER)
		return true;
	return false;
}

// game
bool CGameControllerLTS::DoWincheckRound()
{
	int Count[2] = {0};
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(IsPlayerInRoom(i) && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS &&
			(!GameServer()->m_apPlayers[i]->m_RespawnDisabled ||
				(GameServer()->m_apPlayers[i]->GetCharacter() && GameServer()->m_apPlayers[i]->GetCharacter()->IsAlive())))
			++Count[GameServer()->m_apPlayers[i]->GetTeam()];
	}

	if(Count[TEAM_RED] + Count[TEAM_BLUE] == 0 || (m_GameInfo.m_TimeLimit > 0 && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60))
	{
		++m_aTeamscore[TEAM_BLUE];
		++m_aTeamscore[TEAM_RED];
		EndRound();
		return true;
	}
	else if(Count[TEAM_RED] == 0)
	{
		++m_aTeamscore[TEAM_BLUE];
		EndRound();
		return true;
	}
	else if(Count[TEAM_BLUE] == 0)
	{
		++m_aTeamscore[TEAM_RED];
		EndRound();
		return true;
	}
	return false;
}
