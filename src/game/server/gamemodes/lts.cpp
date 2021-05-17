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
	m_GameFlags = IGF_TEAMS | IGF_SURVIVAL | IGF_ROUND_TIMER_ROUND;

	INSTANCE_CONFIG_INT(&m_SpawnArmor, "spawn_armor", 5, 0, 10, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Maximum room size (from 2 to 64)")
}

// event
void CGameControllerLTS::OnCharacterSpawn(class CCharacter *pChr)
{
	pChr->IncreaseHealth(10);
	pChr->IncreaseArmor(m_SpawnArmor);

	// give start equipment
	pChr->GiveWeapon(WEAPON_GUN, WEAPON_ID_PISTOL, 10);
	pChr->GiveWeapon(WEAPON_HAMMER, WEAPON_ID_HAMMER, -1);
	pChr->GiveWeapon(WEAPON_SHOTGUN, WEAPON_ID_SHOTGUN, 10);
	pChr->GiveWeapon(WEAPON_GRENADE, WEAPON_ID_GRENADE, 10);
	pChr->GiveWeapon(WEAPON_LASER, WEAPON_ID_LASER, 5);
}

bool CGameControllerLTS::OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number)
{
	// bypass pickups
	if(Index >= ENTITY_ARMOR_1 && Index <= ENTITY_WEAPON_LASER)
		return true;
	return false;
}

// game
void CGameControllerLTS::DoWincheckRound()
{
	int Count[2] = {0};
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(i);
		if(pPlayer && pPlayer->GetTeam() != TEAM_SPECTATORS &&
			(!pPlayer->m_RespawnDisabled ||
				(pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive())))
			++Count[pPlayer->GetTeam()];
	}

	if(Count[TEAM_RED] + Count[TEAM_BLUE] == 0 || (m_GameInfo.m_TimeLimit > 0 && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60))
	{
		++m_aTeamscore[TEAM_BLUE];
		++m_aTeamscore[TEAM_RED];
		EndRound();
	}
	else if(Count[TEAM_RED] == 0)
	{
		++m_aTeamscore[TEAM_BLUE];
		EndRound();
	}
	else if(Count[TEAM_BLUE] == 0)
	{
		++m_aTeamscore[TEAM_RED];
		EndRound();
	}
}
