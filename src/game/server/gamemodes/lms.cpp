/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include "lms.h"
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/weapons.h>

CGameControllerLMS::CGameControllerLMS() :
	IGameController()
{
	m_pGameType = "LMS";
	m_GameFlags = IGF_SURVIVAL | IGF_ROUND_TIMER_ROUND | IGF_SUDDENDEATH;

	INSTANCE_CONFIG_INT(&m_SpawnArmor, "spawn_armor", 5, 0, 10, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Maximum room size (from 2 to 64)")
}

// event
void CGameControllerLMS::OnWorldReset()
{
	// only spawn topscore players on suddon death round
	if(m_SuddenDeath)
	{
		int Topscore = 0;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CPlayer *pPlayer = GetPlayerIfInRoom(i);
			if(pPlayer)
				if(pPlayer->m_Score > Topscore)
					Topscore = pPlayer->m_Score;
		}

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CPlayer *pPlayer = GetPlayerIfInRoom(i);
			if(pPlayer && pPlayer->m_Score != Topscore)
				pPlayer->CancelSpawn();
		}
	}
}

void CGameControllerLMS::OnCharacterSpawn(CCharacter *pChr)
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

bool CGameControllerLMS::OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number)
{
	// bypass pickups
	if(Index >= ENTITY_ARMOR_1 && Index <= ENTITY_WEAPON_LASER)
		return true;
	return false;
}

// game
void CGameControllerLMS::DoWincheckRound()
{
	// check for time based win
	if(!m_SuddenDeath && m_GameInfo.m_TimeLimit > 0 && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			CPlayer *pPlayer = GetPlayerIfInRoom(i);
			if(pPlayer && pPlayer->GetTeam() != TEAM_SPECTATORS &&
				(!pPlayer->m_RespawnDisabled ||
					(pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive())))
				pPlayer->m_Score++;
		}

		EndRound();
	}
	else
	{
		// check for survival win
		CPlayer *pAlivePlayer = 0;
		int AlivePlayerCount = 0;
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			CPlayer *pPlayer = GetPlayerIfInRoom(i);
			if(pPlayer && pPlayer->GetTeam() != TEAM_SPECTATORS &&
				(!pPlayer->m_RespawnDisabled ||
					(pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive())))
			{
				++AlivePlayerCount;
				pAlivePlayer = pPlayer;
			}
		}

		if(AlivePlayerCount == 0) // no winner
		{
			EndRound();
		}
		else if(AlivePlayerCount == 1) // 1 winner
		{
			pAlivePlayer->m_Score++;
			EndRound();
		}
	}
}