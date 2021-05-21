#include "catch.h"

#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/weapons.h>

#include <game/server/entities/dumbentity.h>

CGameControllerCatch::CGameControllerCatch()
{
	m_pGameType = "Catch";

	m_GameFlags = IGF_MARK_SURVIVAL;
	INSTANCE_CONFIG_INT(&m_WinnerBonus, "winner_bonus", 100, 0, 2000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "amount of points given to winner")
	INSTANCE_CONFIG_INT(&m_MinimumPlayers, "minimum_players", 5, 1, MAX_CLIENTS, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "how many players required to trigger match end")
}

void CGameControllerCatch::Catch(CPlayer *pVictim)
{
	CPlayer *TopPlayer = nullptr;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(i);
		if(pPlayer && pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive() && (!TopPlayer || pPlayer->m_Score > TopPlayer->m_Score))
			TopPlayer = pPlayer;
	}

	// catch by top player
	if(TopPlayer)
	{
		vec2 Pos = m_aCharPath[TopPlayer->GetCID()].PrevPoint(m_aNumCaught[TopPlayer->GetCID()]);
		Catch(pVictim, TopPlayer, Pos);
	}
}

void CGameControllerCatch::Catch(CPlayer *pVictim, CPlayer *pBy, vec2 Pos)
{
	if(m_aCaughtBy[pVictim->GetCID()] != -1)
		return;

	if(m_apHearts[pVictim->GetCID()])
	{
		m_apHearts[pVictim->GetCID()]->Destroy();
		m_apHearts[pVictim->GetCID()] = nullptr;
		m_aHeartKillTick[pVictim->GetCID()] = -1;
	}

	m_apHearts[pVictim->GetCID()] = new CDumbEntity(GameWorld(), CDumbEntity::TYPE_HEART, Pos);
	m_aHeartID[pVictim->GetCID()] = m_aNumCaught[pBy->GetCID()];
	m_aNumCaught[pBy->GetCID()]++;

	m_aCaughtBy[pVictim->GetCID()] = pBy->GetCID();
}

void CGameControllerCatch::Release(CPlayer *pPlayer, bool IsKillRelease)
{
	if(m_aCaughtBy[pPlayer->GetCID()] == -1)
		return;

	int By = m_aCaughtBy[pPlayer->GetCID()];

	m_aNumCaught[By]--;
	m_aCaughtBy[pPlayer->GetCID()] = -1;

	if(IsKillRelease)
	{
		// do heart kill animation
		m_aHeartKillTick[pPlayer->GetCID()] = Server()->Tick() + (m_aHeartID[pPlayer->GetCID()] + 1) * 2;
	}
	else
	{
		// remove last heart
		int LastHeartID = m_aNumCaught[By];

		// find the last heart
		int WhoHasLastHeart = pPlayer->GetCID();
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_aCaughtBy[i] != By)
				continue;

			if(m_aHeartID[i] == LastHeartID)
			{
				WhoHasLastHeart = i;
				break;
			}
		}

		m_apHearts[WhoHasLastHeart]->Destroy();
		m_apHearts[WhoHasLastHeart] = m_apHearts[pPlayer->GetCID()];
		m_aHeartID[WhoHasLastHeart] = m_aHeartID[pPlayer->GetCID()];
		m_apHearts[pPlayer->GetCID()] = nullptr;
		m_aHeartID[pPlayer->GetCID()] = -1;
	}

	pPlayer->m_RespawnDisabled = false;
	pPlayer->m_RespawnTick = Server()->Tick() + Server()->TickSpeed();
	pPlayer->Respawn();
}

void CGameControllerCatch::OnInit()
{
	mem_zero(m_apHearts, sizeof(m_apHearts));
	mem_zero(m_aCharMoveDist, sizeof(m_aCharMoveDist));
	mem_zero(m_aNumCaught, sizeof(m_aNumCaught));

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aCaughtBy[i] = -1;
		m_aHeartKillTick[i] = -1;
	}
}

void CGameControllerCatch::OnKill(CPlayer *pPlayer)
{
	int ClientID = pPlayer->GetCID();

	if(m_aNumCaught[ClientID] > 0)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			CPlayer *pPlayer = GetPlayerIfInRoom(i);
			if(pPlayer && m_aCaughtBy[pPlayer->GetCID()] == ClientID)
				Release(pPlayer, true);
		}

		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "'%s' released.", Server()->ClientName(ClientID));
		SendChatTarget(-1, aBuf);
	}
}

void CGameControllerCatch::OnPreTick()
{
	if(!IsRunning())
		return;

	float PointDist = 50.0f;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(i);
		if(pPlayer && pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive())
		{
			CCharacter *pChar = pPlayer->GetCharacter();
			vec2 DeltaPos = pChar->GetPos() - m_aLastPosition[i];
			m_aLastPosition[i] = pChar->GetPos();

			vec2 LastPoint = m_aCharPath[i].LatestPoint();
			vec2 Dir = normalize(pChar->GetPos() - LastPoint);

			if(fabs(length(DeltaPos)) < 1e-6)
			{
				m_aCharInertia[i] = m_aCharInertia[i] / 1.05f;
				m_aCharMoveDist[i] += m_aCharInertia[i];
				Dir = Dir * 0.15f;
			}
			else
			{
				m_aCharInertia[i] = length(DeltaPos);
				m_aCharMoveDist[i] += length(DeltaPos);
			}

			int Iteration = 0;
			while(m_aCharMoveDist[i] > PointDist)
			{
				m_aCharMoveDist[i] -= PointDist;
				Iteration++;
				vec2 Point = LastPoint + Dir * PointDist * Iteration;
				m_aCharPath[i].RecordPoint(Point);
			}
		}
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apHearts[i])
		{
			if(m_aHeartKillTick[i] != -1 && m_aHeartKillTick[i] < Server()->Tick())
			{
				GameWorld()->CreateSound(m_apHearts[i]->GetPos(), SOUND_PLAYER_DIE);
				GameWorld()->CreateDeath(m_apHearts[i]->GetPos(), i);
				m_apHearts[i]->Destroy();
				m_apHearts[i] = nullptr;
				m_aHeartID[i] = -1;
				m_aHeartKillTick[i] = -1;
				continue;
			}

			int CaughtBy = m_aCaughtBy[i];
			CPlayer *pPlayer = GetPlayerIfInRoom(CaughtBy);
			if(pPlayer && pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive())
			{
				float Interp = m_aCharMoveDist[CaughtBy] / PointDist;
				vec2 Point = m_aCharPath[CaughtBy].PrevPoint(m_aHeartID[i]);
				vec2 PrevPoint = m_aCharPath[CaughtBy].PrevPoint(m_aHeartID[i] + 1);
				vec2 TargetPos = mix(PrevPoint, Point, Interp);
				float Rate = 1.0f - (m_aHeartID[i] / (32.0f + 16.0f));
				vec2 Pos = mix(m_apHearts[i]->m_Pos, TargetPos, (25.0f * Rate * Rate / (float)Server()->TickSpeed()));
				m_apHearts[i]->MoveTo(Pos);
			}
		}
	}
}

void CGameControllerCatch::OnWorldReset()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aCaughtBy[i] = -1;
		m_aNumCaught[i] = 0;
		m_aHeartKillTick[i] = -1;
		if(m_apHearts[i])
		{
			m_apHearts[i]->Destroy();
			m_apHearts[i] = nullptr;
		}
	}
}

void CGameControllerCatch::OnPlayerJoin(CPlayer *pPlayer)
{
	// don't do anything during warmup
	if(IsWarmup())
		return;

	Catch(pPlayer);
}

void CGameControllerCatch::OnPlayerLeave(CPlayer *pPlayer)
{
	Release(pPlayer, false);
}

bool CGameControllerCatch::OnPlayerTryRespawn(CPlayer *pPlayer, vec2 Pos)
{
	if(m_aCaughtBy[pPlayer->GetCID()] == -1)
		return true;

	int By = m_aCaughtBy[pPlayer->GetCID()];
	CPlayer *pKiller = GetPlayerIfInRoom(By);

	if(!pKiller || !pKiller->GetCharacter() || !pKiller->GetCharacter()->IsAlive())
	{
		m_aCaughtBy[pPlayer->GetCID()] = -1;
		return true;
	}

	pPlayer->CancelSpawn();
	return false;
}

void CGameControllerCatch::OnPlayerChangeTeam(CPlayer *pPlayer, int FromTeam, int ToTeam)
{
	if(ToTeam == TEAM_SPECTATORS)
		Release(pPlayer, false);
	else
		Catch(pPlayer);
}

void CGameControllerCatch::OnCharacterSpawn(CCharacter *pChr)
{
	// standard dm equipments
	pChr->IncreaseHealth(10);

	pChr->GiveWeapon(WEAPON_GUN, WEAPON_ID_PISTOL, 10);
	pChr->GiveWeapon(WEAPON_HAMMER, WEAPON_ID_HAMMER, -1);

	int ClientID = pChr->GetPlayer()->GetCID();
	m_aLastPosition[ClientID] = pChr->GetPos();
	m_aCharMoveDist[ClientID] = 0;
	m_aCharPath[ClientID].Init(pChr->GetPos());
	m_aCharInertia[ClientID] = 20.0f;
}

int CGameControllerCatch::OnCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, int Weapon)
{
	// don't do anything during warmup
	if(IsWarmup())
		return DEATH_NORMAL;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(i);
		if(pPlayer && m_aCaughtBy[pPlayer->GetCID()] == pVictim->GetPlayer()->GetCID())
		{
			Release(pPlayer, true);
		}
	}

	// allow respawn, only check caught status on respawn (handles mutual kills)
	pVictim->GetPlayer()->m_RespawnDisabled = false;
	pVictim->GetPlayer()->m_RespawnTick = Server()->Tick() + Server()->TickSpeed();
	pVictim->GetPlayer()->Respawn();

	// this also covers suicide
	if(!pKiller->GetCharacter() || !pKiller->GetCharacter()->IsAlive())
	{
		return DEATH_NORMAL;
	}

	Catch(pVictim->GetPlayer(), pKiller, pVictim->GetPos());

	return DEATH_NORMAL;
}

bool CGameControllerCatch::CanDeadPlayerFollow(const CPlayer *pSpectator, const CPlayer *pTarget)
{
	return m_aCaughtBy[pSpectator->GetCID()] == pTarget->GetCID();
}

void CGameControllerCatch::DoWincheckMatch()
{
	CPlayer *pAlivePlayer = 0;
	int AlivePlayerCount = 0;
	int TotalPlayerCount = 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(i);
		if(pPlayer && pPlayer->GetTeam() != TEAM_SPECTATORS)
		{
			TotalPlayerCount++;

			if(!pPlayer->m_RespawnDisabled || (pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive()))
			{
				++AlivePlayerCount;
				pAlivePlayer = pPlayer;
			}
		}
	}

	if(AlivePlayerCount == 1) // 1 winner
	{
		if(TotalPlayerCount >= m_MinimumPlayers)
		{
			pAlivePlayer->m_Score += m_WinnerBonus;
			EndMatch();
		}
		else
		{
			for(int i = 0; i < MAX_CLIENTS; ++i)
			{
				CPlayer *pPlayer = GetPlayerIfInRoom(i);
				if(pPlayer && m_aCaughtBy[pPlayer->GetCID()] == pAlivePlayer->GetCID())
					Release(pPlayer, true);
			}
		}
	}

	// still counts scorelimit
	IGameController::DoWincheckMatch();
}

template<>
CGameControllerInstagib<CGameControllerCatch>::CGameControllerInstagib()
{
	m_pGameType = "zCatch";
	RegisterConfig();
}
