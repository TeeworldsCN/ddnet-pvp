#include "block-down.h"

#include <game/server/ModCore/BlockDown/player.h>
#include <game/server/entities/character.h>
#include <game/server/weapons.h>

CGameControllerBD::CGameControllerBD() :
	IGameController()
{
	m_pGameType = "BlockDown";
	m_GameFlags = IGF_TEAMS | IGF_SUDDENDEATH;
	m_GameState = IGS_WARMUP_GAME;
}

void CGameControllerBD::OnCharacterSpawn(CCharacter *pChr)
{
	pChr->GetPlayer()->m_WontDie = true;
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

void CGameControllerBD::StartRoundBD()
{
	SendChatTarget(-1, "Run! Zombies are coming!");
	// If we passed warmup and still not enough player, do infinite timer
	if(IsWarmup() && !HasEnoughPlayers())
	{
		SetGameState(IGS_WARMUP_GAME, TIMER_INFINITE);
		return;
	}

	ResetGame();
	CheckGameInfo(false);

	if(IsRoundTimer())
		m_GameStartTick = Server()->Tick();

	++m_RoundCount;
	m_GameInfo.m_MatchCurrent = m_RoundCount + 1;

	for(int i = 0; i < MAX_CLIENTS; ++i)
		UpdateGameInfo(i);

	if(HasEnoughPlayers())
	{
		SetGameState(IGS_START_COUNTDOWN);
		OnGameStart(true);
	}
	else
		SetGameState(IGS_WARMUP_GAME, TIMER_INFINITE);
}

void CGameControllerBD::OnPreTick()
{
	NumZombies();
	NumPlayers();
	NumHumans();
	//char aBuf[128];
	//str_format(aBuf, sizeof(aBuf), "Zomb: %i | Human: %i | All: %i", NumZombies(), NumHumans(), NumPlayers());
	//SendBroadcast(aBuf, -1);
	// if(m_aTeamSize[TEAM_RED] + m_aTeamSize[TEAM_BLUE] >= 2)
	CGameControllerBD::DoWincheckMatch();
}

int CGameControllerBD::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	if(pKiller->GetTeam() == TEAM_RED)
	{
		pKiller->m_Score++;
	}
	if(pVictim->GetPlayer()->GetTeam() == TEAM_BLUE)
	{
		m_aTeamSize[TEAM_BLUE]--;
		m_aTeamSize[TEAM_RED]++;
		pVictim->GetPlayer()->m_Team = TEAM_RED;
		//DoTeamChange(pPlayer, TEAM_RED, false);
	}
	CGameControllerBD::DoWincheckMatch();	
	pVictim->GetPlayer()->m_RespawnTick = 50;

	return DEATH_NORMAL;
}

void CGameControllerBD::DoWincheckMatch()
{
	char aBuf[64];
	//if(IsTeamplay())
	//{
		if((Server()->Tick() - m_GameStartTick) >= (m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60) && RoundOver != 1)
		{
			RoundOver = 1;
			str_format(aBuf, sizeof(aBuf), "Humans win this round");
			GameServer()->SendChatTarget(-1, aBuf);
			m_aTeamscore[TEAM_BLUE] = 1000;
			EndMatch();
			ResetMatch();
			DoTeamBalance();
			CGameControllerBD::StartRound();
		}
		else if(NumPlayers() - NumZombies() == 0 && (m_aTeamSize[TEAM_BLUE] + m_aTeamSize[TEAM_RED]) >= 2 && RoundOver != 1)
		{
			RoundOver = 1;
			str_format(aBuf, sizeof(aBuf), "Zombies win this round in: %i", (Server()->Tick() - m_GameStartTick) / 50);
			GameServer()->SendChatTarget(-1, aBuf);
			EndMatch();
			ResetGame();
			DoTeamBalance();
			CGameControllerBD::StartRound();
		}
//	}
}

int CGameControllerBD::NumZombies()
{
	int NumZombies = m_aTeamSize[TEAM_RED];
	/*for (int i = 0; i < (m_aTeamSize[TEAM_RED]); i++)
	{
		if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() == TEAM_RED)
			NumZombies++;
	}*/
	return NumZombies;
}

int CGameControllerBD::NumHumans()
{
	int NumHumans = m_aTeamSize[TEAM_BLUE];
	/*for (int i = 0; i  < (m_aTeamSize[TEAM_BLUE]); i++)
	{
		if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() == TEAM_BLUE)
			NumHumans++;
	}*/
	return NumHumans;
}

int CGameControllerBD::NumPlayers()
{
	int NumPlayers = 0;
	for (int i = 0; i < (m_aTeamSize[TEAM_BLUE] + m_aTeamSize[TEAM_RED]); i++)
	{
		if (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			NumPlayers++;
	}
	return NumPlayers;
}