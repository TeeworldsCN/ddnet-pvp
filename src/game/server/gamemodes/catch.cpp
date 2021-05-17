#include "catch.h"

#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/weapons.h>

CGameControllerCatch::CGameControllerCatch()
{
	m_pGameType = "Catch";

	// Force win condition off
	INSTANCE_COMMAND_REMOVE("timelimit")
	INSTANCE_COMMAND_REMOVE("scorelimit")
	INSTANCE_COMMAND_REMOVE("roundlimit")
	m_Scorelimit = 0;
	m_Timelimit = 0;
	m_Roundlimit = 0;

	// Disable kill
	INSTANCE_COMMAND_REMOVE("kill_delay")
	m_KillDelay = -1;

	m_GameFlags = IGF_MARK_SURVIVAL;
	INSTANCE_CONFIG_INT(&m_WinnerBonus, "winner_bonus", 100, 0, 2000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "amount of points given to winner")
}

void CGameControllerCatch::Catch(CPlayer *pVictim, CPlayer *pBy)
{
	m_aCatchedBy[pVictim->GetCID()] = pBy->GetCID();
	pVictim->CancelSpawn();
}

void CGameControllerCatch::Release(CPlayer *pPlayer)
{
	m_aCatchedBy[pPlayer->GetCID()] = -1;
	pPlayer->m_RespawnDisabled = false;
	pPlayer->Respawn();
	// spawn now
	pPlayer->TryRespawn();
}

void CGameControllerCatch::OnWorldReset()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aCatchedBy[i] = -1;
}

void CGameControllerCatch::OnPlayerJoin(CPlayer *pPlayer)
{
	// don't do anything during warmup
	if(IsWarmup())
		return;

	CPlayer *TopPlayer = nullptr;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(i);
		if(pPlayer && (!TopPlayer || pPlayer->m_Score > TopPlayer->m_Score))
			TopPlayer = pPlayer;
	}

	// catch by top player
	if(TopPlayer)
		Catch(pPlayer, TopPlayer);
}

void CGameControllerCatch::OnCharacterSpawn(CCharacter *pChr)
{
	// standard dm equipments
	pChr->IncreaseHealth(10);

	pChr->GiveWeapon(WEAPON_GUN, WEAPON_ID_PISTOL, 10);
	pChr->GiveWeapon(WEAPON_HAMMER, WEAPON_ID_HAMMER, -1);
}

int CGameControllerCatch::OnCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, int Weapon)
{
	// don't do anything during warmup
	if(IsWarmup())
		return DEATH_NORMAL;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(i);
		if(pPlayer && m_aCatchedBy[pPlayer->GetCID()] == pVictim->GetPlayer()->GetCID())
		{
			Release(pPlayer);
		}
	}

	// this also covers suicide
	if(!pKiller->GetCharacter() || !pKiller->GetCharacter()->IsAlive())
	{
		return DEATH_NORMAL;
	}

	Catch(pVictim->GetPlayer(), pKiller);

	return DEATH_NORMAL;
}

bool CGameControllerCatch::CanDeadPlayerFollow(const CPlayer *pSpectator, const CPlayer *pTarget)
{
	return m_aCatchedBy[pSpectator->GetCID()] == pTarget->GetCID();
}

void CGameControllerCatch::DoWincheckMatch()
{
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
		EndMatch();
	}
	else if(AlivePlayerCount == 1) // 1 winner
	{
		pAlivePlayer->m_Score += m_WinnerBonus;
		EndMatch();
	}
}
