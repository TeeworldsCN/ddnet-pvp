/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/teams.h>

#include <game/generated/protocol.h>
#include <game/generated/server_data.h>

#include "entities/character.h"
#include "entities/pickup.h"
#include "gamecontext.h"
#include "gamecontroller.h"
#include "player.h"

#include "entities/door.h"
#include "entities/dragger.h"
#include "entities/gun.h"
#include "entities/light.h"
#include "entities/plasma.h"
#include "entities/projectile.h"
#include "weapons.h"
#include <game/layers.h>

#include <engine/server/server.h>
#include <game/localization.h>

// MYTODO: clean up these static methods
static void ConchainUpdateCountdown(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() >= 1)
	{
		IGameController *pThis = static_cast<IGameController *>(pUserData);
		if(pThis->GetGameStateTimer() > pThis->m_Countdown * pThis->Server()->TickSpeed())
			pThis->DoCountdown(pThis->m_Countdown);
	}
}

static void ConchainTryStartWarmup(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	IGameController *pThis = static_cast<IGameController *>(pUserData);
	pThis->TryStartWarmup(true);
}

static void ConchainVoteUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() >= 1)
	{
		IGameController *pThis = static_cast<IGameController *>(pUserData);
		pThis->GameServer()->Teams()->UpdateVotes();
	}
}

static void ConchainGameInfoUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() >= 1)
	{
		IGameController *pThis = static_cast<IGameController *>(pUserData);
		pThis->CheckGameInfo();
	}
}

static void ConSwapTeams(IConsole::IResult *pResult, void *pUserData)
{
	IGameController *pSelf = (IGameController *)pUserData;
	pSelf->SwapTeams();
}

static void ConShuffleTeams(IConsole::IResult *pResult, void *pUserData)
{
	IGameController *pSelf = (IGameController *)pUserData;
	pSelf->ShuffleTeams();
}

static void ConPause(IConsole::IResult *pResult, void *pUserData)
{
	IGameController *pSelf = (IGameController *)pUserData;

	if(pResult->NumArguments())
		pSelf->DoPause(clamp(pResult->GetInteger(0), -1, 1000));
	else
		pSelf->DoPause(pSelf->IsGamePaused() ? 0 : IGameController::TIMER_INFINITE);
}

static void ConRestart(IConsole::IResult *pResult, void *pUserData)
{
	IGameController *pSelf = (IGameController *)pUserData;

	int Seconds = pResult->NumArguments() ? clamp(pResult->GetInteger(0), -1, 1000) : 0;
	if(Seconds < 0)
		pSelf->AbortWarmup();
	else
		pSelf->DoWarmup(Seconds);
}

static void ConSay(IConsole::IResult *pResult, void *pUserData)
{
	IGameController *pSelf = (IGameController *)pUserData;
	pSelf->SendChatTarget(-1, pResult->GetString(0));
}

static void ConBroadcast(IConsole::IResult *pResult, void *pUserData)
{
	IGameController *pSelf = (IGameController *)pUserData;
	pSelf->SendBroadcast(pResult->GetString(0), -1);
}

static void ConSetTeamAll(IConsole::IResult *pResult, void *pUserData)
{
	IGameController *pSelf = (IGameController *)pUserData;
	int Team = clamp(pResult->GetInteger(0), -1, 1);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "All players were moved to the %s", pSelf->GetTeamName(Team));
	pSelf->SendChatTarget(-1, aBuf);

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = pSelf->GetPlayerIfInRoom(i);
		if(pPlayer)
			pSelf->DoTeamChange(pPlayer, Team, false);
	}
}

static void ConAddVote(IConsole::IResult *pResult, void *pUserData)
{
	IGameController *pSelf = (IGameController *)pUserData;
	const char *pDescription = pResult->GetString(0);
	const char *pCommand = pResult->GetString(1);

	if(pSelf->m_NumVoteOptions == MAX_VOTE_OPTIONS)
	{
		pSelf->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "maximum number of room vote options reached");
		return;
	}

	// check for valid option
	if(!pSelf->InstanceConsole()->LineIsValid(pCommand) || str_length(pCommand) >= VOTE_CMD_LENGTH)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid command '%s'", pCommand);
		pSelf->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}
	while(*pDescription == ' ')
		pDescription++;
	if(str_length(pDescription) >= VOTE_DESC_LENGTH || *pDescription == 0)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid option '%s'", pDescription);
		pSelf->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	// check for duplicate entry
	CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
	while(pOption)
	{
		if(str_comp_nocase(pDescription, pOption->m_aDescription + sizeof("☐ ")) == 0)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "option '%s' already exists", pDescription);
			pSelf->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}
		pOption = pOption->m_pNext;
	}

	// add the option
	++pSelf->m_NumVoteOptions;
	int Len = str_length(pCommand);

	pOption = (CVoteOptionServer *)pSelf->m_pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
	pOption->m_pNext = 0;
	pOption->m_pPrev = pSelf->m_pVoteOptionLast;
	if(pOption->m_pPrev)
		pOption->m_pPrev->m_pNext = pOption;
	pSelf->m_pVoteOptionLast = pOption;
	if(!pSelf->m_pVoteOptionFirst)
		pSelf->m_pVoteOptionFirst = pOption;

	if(str_comp(pCommand, "info") == 0)
		str_format(pOption->m_aDescription, sizeof(pOption->m_aDescription), "%s", pDescription);
	else
		str_format(pOption->m_aDescription, sizeof(pOption->m_aDescription), "☐ %s", pDescription);
	mem_copy(pOption->m_aCommand, pCommand, Len + 1);
}

static void ConRemoveVote(IConsole::IResult *pResult, void *pUserData)
{
	IGameController *pSelf = (IGameController *)pUserData;
	const char *pDescription = pResult->GetString(0);

	// check for valid option
	CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
	while(pOption)
	{
		if(str_comp_nocase(pDescription, pOption->m_aDescription) == 0)
			break;
		pOption = pOption->m_pNext;
	}
	if(!pOption)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "option '%s' does not exist", pDescription);
		pSelf->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	// remove the option
	--pSelf->m_NumVoteOptions;

	CHeap *pVoteOptionHeap = new CHeap();
	CVoteOptionServer *pVoteOptionFirst = 0;
	CVoteOptionServer *pVoteOptionLast = 0;
	int NumVoteOptions = pSelf->m_NumVoteOptions;
	for(CVoteOptionServer *pSrc = pSelf->m_pVoteOptionFirst; pSrc; pSrc = pSrc->m_pNext)
	{
		if(pSrc == pOption)
			continue;

		// copy option
		int Len = str_length(pSrc->m_aCommand);
		CVoteOptionServer *pDst = (CVoteOptionServer *)pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
		pDst->m_pNext = 0;
		pDst->m_pPrev = pVoteOptionLast;
		if(pDst->m_pPrev)
			pDst->m_pPrev->m_pNext = pDst;
		pVoteOptionLast = pDst;
		if(!pVoteOptionFirst)
			pVoteOptionFirst = pDst;

		str_copy(pDst->m_aDescription, pSrc->m_aDescription, sizeof(pDst->m_aDescription));
		mem_copy(pDst->m_aCommand, pSrc->m_aCommand, Len + 1);
	}

	// clean up
	delete pSelf->m_pVoteOptionHeap;
	pSelf->m_pVoteOptionHeap = pVoteOptionHeap;
	pSelf->m_pVoteOptionFirst = pVoteOptionFirst;
	pSelf->m_pVoteOptionLast = pVoteOptionLast;
	pSelf->m_NumVoteOptions = NumVoteOptions;
	pSelf->m_ResendVotes = true;
}

static void ConHelp(IConsole::IResult *pResult, void *pUserData)
{
	IGameController *pSelf = (IGameController *)pUserData;
	int ClientID = pResult->m_ClientID;

	if(pResult->NumArguments() == 0)
		pSelf->GameServer()->Console()->ExecuteLine("help", ClientID, false); // call main help for info
	else
	{
		const char *pArg = pResult->GetString(0);
		const IConsole::CCommandInfo *pCmdInfo =
			pSelf->InstanceConsole()->GetCommandInfo(pArg, CFGFLAG_CHAT | CFGFLAG_INSTANCE, false);
		if(pCmdInfo)
		{
			if(pCmdInfo->m_pParams)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "Usage: setting %s %s", pCmdInfo->m_pName, pCmdInfo->m_pParams);
				pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help", aBuf);
			}

			if(pCmdInfo->m_pHelp)
				pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help", pCmdInfo->m_pHelp);
		}
		else
			pSelf->InstanceConsole()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"help",
				"Command is either unknown or you have given a blank command without any parameters.");
	}
}

static void ConKick(IConsole::IResult *pResult, void *pUserData)
{
	IGameController *pSelf = (IGameController *)pUserData;
	int VictimID = pResult->GetInteger(0);

	if(pSelf->GameWorld()->Team() == 0)
		pSelf->GameServer()->Console()->ExecuteLine(pResult->GetString(1));
	else
	{
		if(pSelf->GameServer()->Teams()->SetForcePlayerTeam(VictimID, 0, CGameTeams::TEAM_REASON_FORCE, nullptr))
		{
			pSelf->GameServer()->SendChatLocalized(VictimID, "You have been moved to room %d", 0);
			pSelf->GameServer()->Teams()->SetClientInvited(pSelf->GameWorld()->Team(), VictimID, false);
		}
		else
			pSelf->GameServer()->Console()->ExecuteLine(pResult->GetString(1));
	}
}

static void ConServerCommand(IConsole::IResult *pResult, void *pUserData)
{
	IGameController *pSelf = (IGameController *)pUserData;
	pSelf->GameServer()->Console()->ExecuteLine(pResult->GetString(0));
}

static void ConChangeMap(IConsole::IResult *pResult, void *pUserData)
{
	IGameController *pSelf = (IGameController *)pUserData;
	int Room = pSelf->GameWorld()->Team();
	CGameTeams *pTeams = pSelf->GameServer()->Teams();
	const char *pMapName = pResult->GetString(0);
	int MapIndex = pTeams->GetMapIndex(pResult->GetString(0));
	if(MapIndex == 0)
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "Cannot find map '%s'", pMapName);
		pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "instance", aBuf);
		return;
	}

	if(pSelf->m_MapIndex == MapIndex)
		return;
	pSelf->m_MapIndex = MapIndex;
	pTeams->ReloadGameInstance(Room);
}

static void ConChangeGameType(IConsole::IResult *pResult, void *pUserData)
{
	IGameController *pSelf = (IGameController *)pUserData;
	int Room = pSelf->GameWorld()->Team();
	CGameTeams *pTeams = pSelf->GameServer()->Teams();
	if(!pTeams->RecreateGameInstance(Room, pResult->GetString(0)))
	{
		pSelf->InstanceConsole()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"instance",
			"Please provide a valid gametype.");
	}
}

int IGameController::MakeGameFlag(int GameFlag)
{
	int Flags = 0;
	if(GameFlag & IGF_TEAMS)
		Flags |= GAMEFLAG_TEAMS;
	if(GameFlag & IGF_FLAGS)
		Flags |= GAMEFLAG_FLAGS;
	return Flags;
}

int IGameController::MakeGameFlagSixUp(int GameFlag)
{
	// TODO: add race support?
	int Flags = 0;
	if(GameFlag & IGF_TEAMS)
		Flags |= protocol7::GAMEFLAG_TEAMS;
	if(GameFlag & IGF_FLAGS)
		Flags |= protocol7::GAMEFLAG_FLAGS;
	if(GameFlag & (IGF_SURVIVAL | IGF_MARK_SURVIVAL))
		Flags |= protocol7::GAMEFLAG_SURVIVAL;
	return Flags;
}

IGameController::IGameController()
{
	m_Started = false;
	m_pGameServer = nullptr;
	m_pConfig = nullptr;
	m_pServer = nullptr;
	m_pWorld = nullptr;
	m_pInstanceConsole = new CConsole(CFGFLAG_INSTANCE);
	m_MapIndex = 0;

	// balancing
	m_aTeamSize[TEAM_RED] = 0;
	m_aTeamSize[TEAM_BLUE] = 0;
	m_UnbalancedTick = TBALANCE_OK;

	// game
	m_GameState = IGS_GAME_RUNNING;
	m_GameStateTimer = TIMER_INFINITE;
	m_GameStartTick = 0;
	m_RoundCount = 0;
	m_SuddenDeath = 0;

	// info
	m_GameFlags = 0;
	m_pGameType = "unknown";
	m_GameInfo.m_MatchCurrent = m_RoundCount + 1;
	m_GameInfo.m_MatchNum = 0;
	m_GameInfo.m_ScoreLimit = 0;
	m_GameInfo.m_TimeLimit = 0;
	m_DDNetInfoFlag = GAMEINFOFLAG_ALLOW_EYE_WHEEL | GAMEINFOFLAG_ALLOW_HOOK_COLL | GAMEINFOFLAG_PREDICT_DDRACE_TILES;
	m_DDNetInfoFlag2 = 0;

	// vote
	m_VotePos = 0;
	m_VoteCreator = -1;
	m_VoteEnforcer = -1;
	m_VoteCloseTime = 0;
	m_VoteUpdate = false;
	m_aVoteDescription[0] = 0;
	m_aSixupVoteDescription[0] = 0;
	m_aVoteCommand[0] = 0;
	m_aVoteReason[0] = 0;
	m_VoteEnforce = 0;
	m_VoteWillPass = false;
	m_VoteType = VOTE_TYPE_UNKNOWN;
	m_VoteVictim = -1;
	m_pVoteOptionHeap = new CHeap();
	m_pVoteOptionFirst = nullptr;
	m_pVoteOptionLast = nullptr;
	m_NumVoteOptions = 0;
	m_ResendVotes = false;
	m_NumPlayerNotReady = 0;

	// fake client broadcast
	mem_zero(m_aFakeClientBroadcast, sizeof(m_aFakeClientBroadcast));

	m_pInstanceConsole->RegisterPrintCallback(IConsole::OUTPUT_LEVEL_STANDARD, InstanceConsolePrint, this);

	INSTANCE_CONFIG_INT(&m_Warmup, "warmup", 10, 0, 1000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Number of seconds to do warmup before match starts");
	INSTANCE_CONFIG_INT(&m_Countdown, "countdown", 0, -1000, 1000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Number of seconds to freeze the game in a countdown before match starts, (-: for survival, +: for all")
	INSTANCE_CONFIG_INT(&m_Teamdamage, "teamdamage", 0, 0, 2, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Team damage (1 = half damage, 2 = full damage)")
	INSTANCE_CONFIG_INT(&m_MatchSwap, "match_swap", 1, 0, 2, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Swap teams between matches (2 = shuffle team)")
	INSTANCE_CONFIG_INT(&m_Powerups, "powerups", 1, 0, 1, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Allow powerups like ninja")
	INSTANCE_CONFIG_INT(&m_Scorelimit, "scorelimit", 20, 0, 1000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Score limit (0 disables)")
	INSTANCE_CONFIG_INT(&m_Timelimit, "timelimit", 0, 0, 1000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Time limit in minutes (0 disables)")
	INSTANCE_CONFIG_INT(&m_Roundlimit, "roundlimit", 0, 0, 1000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Round limit for game with rounds (0 disables)")
	INSTANCE_CONFIG_INT(&m_TeambalanceTime, "teambalance_time", 1, 0, 1000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "How many minutes to wait before autobalancing teams")
	INSTANCE_CONFIG_INT(&m_KillDelay, "kill_delay", 1, -1, 9999, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "The minimum time in seconds between kills (-1 = disable kill)")
	INSTANCE_CONFIG_INT(&m_PlayerSlots, "player_slots", MAX_CLIENTS, 2, MAX_CLIENTS, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Maximum room size (from 2 to 64)")
	INSTANCE_CONFIG_INT(&m_PlayerReadyMode, "player_ready_mode", 0, 0, 3, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "1 = players can ready to start the game on warmup 2 = players can unready to pause the game, 3 = both")
	INSTANCE_CONFIG_INT(&m_ResetOnMatchEnd, "reset_on_match_end", 0, 0, 1, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Whether to reset to the initial state (warmup or wait for ready) after a match.")
	INSTANCE_CONFIG_INT(&m_PausePerMatch, "pause_per_match", 0, 0, 10, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Number of pause (using ready state) allowed per player per match")
	INSTANCE_CONFIG_INT(&m_MinimumPlayers, "minimum_players", 1, 0, MAX_CLIENTS, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Number of players required to start a match, (0 = the game can't start)")

	m_pInstanceConsole->Chain("scorelimit", ConchainGameInfoUpdate, this);
	m_pInstanceConsole->Chain("timelimit", ConchainGameInfoUpdate, this);
	m_pInstanceConsole->Chain("roundlimit", ConchainGameInfoUpdate, this);
	m_pInstanceConsole->Chain("player_slots", ConchainVoteUpdate, this);
	m_pInstanceConsole->Chain("minimum_players", ConchainTryStartWarmup, this);
	m_pInstanceConsole->Chain("countdown", ConchainUpdateCountdown, this);

	m_pInstanceConsole->Register("say", "?r[message]", CFGFLAG_INSTANCE, ConSay, this, "Say in chat in this room");
	m_pInstanceConsole->Register("broadcast", "?r[message]", CFGFLAG_INSTANCE, ConBroadcast, this, "Broadcast message in this room");

	m_pInstanceConsole->Register("shuffle_teams", "", CFGFLAG_CHAT | CFGFLAG_INSTANCE, ConShuffleTeams, this, "Shuffle the current teams");
	m_pInstanceConsole->Register("swap_teams", "", CFGFLAG_CHAT | CFGFLAG_INSTANCE, ConSwapTeams, this, "Swap the current teams");
	m_pInstanceConsole->Register("map", "?r[name]", CFGFLAG_CHAT | CFGFLAG_INSTANCE, ConChangeMap, this, "Change map");
	m_pInstanceConsole->Register("gametype", "?r[gametype]", CFGFLAG_CHAT | CFGFLAG_INSTANCE, ConChangeGameType, this, "Change gametype");
	m_pInstanceConsole->Register("pause", "?i[seconds]", CFGFLAG_CHAT | CFGFLAG_INSTANCE, ConPause, this, "Pause/unpause game");
	m_pInstanceConsole->Register("restart", "?i[seconds]", CFGFLAG_CHAT | CFGFLAG_INSTANCE, ConRestart, this, "Restart in x seconds (0 = abort)");
	m_pInstanceConsole->Register("set_team_all", "i[team-id]", CFGFLAG_INSTANCE, ConSetTeamAll, this, "Set team of all players to team");
	m_pInstanceConsole->Register("help", "?r[command]", CFGFLAG_CHAT | CFGFLAG_INSTANCE | CFGFLAG_NO_CONSENT, ConHelp, this, "Shows help to command, general help if left blank");
	m_pInstanceConsole->Register("info", "", CFGFLAG_CHAT | CFGFLAG_INSTANCE | CFGFLAG_NO_CONSENT, ConHelp, this, "Shows help to command, general help if left blank");

	// vote commands
	m_pInstanceConsole->Register("add_vote", "s[name] r[command]", CFGFLAG_INSTANCE, ConAddVote, this, "Add a voting option");
	m_pInstanceConsole->Register("remove_vote", "r[name]", CFGFLAG_INSTANCE, ConRemoveVote, this, "remove a voting option");

	// vote helper
	m_pInstanceConsole->Register("vote_kick", "i[id] r[real-command]", CFGFLAG_INSTANCE, ConKick, this, "Helper command for vote kicking, it is not recommended to call this directly");
	m_pInstanceConsole->Register("server_command", "r[real-command]", CFGFLAG_INSTANCE, ConServerCommand, this, "Pass the command to GameServer to process, it is not recommended to call this directly");
}

IGameController::~IGameController()
{
	delete m_pInstanceConsole;
	for(auto pInt : m_IntConfigStore)
		delete pInt;

	delete m_pVoteOptionHeap;
}

void IGameController::StartController()
{
	m_Started = true;

	// game
	m_RoundCount = 0;
	m_SuddenDeath = 0;

	m_GameStartTick = Server()->Tick();
	m_GameInfo.m_ScoreLimit = m_Scorelimit;
	m_GameInfo.m_TimeLimit = m_Timelimit;
	m_GameInfo.m_MatchNum = m_Roundlimit;
	m_GameInfo.m_MatchCurrent = m_RoundCount + 1;
	for(int i = 0; i < MAX_CLIENTS; ++i)
		UpdateGameInfo(i);

	// default to wait for more players
	SetGameState(IGS_WARMUP_GAME, TIMER_INFINITE);

	TryStartWarmup();

	OnControllerStart();
}

bool IGameController::GetPlayersReadyState(int WithoutID)
{
	m_NumPlayerNotReady = 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(i == WithoutID)
			continue;

		CPlayer *pPlayer = GetPlayerIfInRoom(i);
		if(pPlayer && pPlayer->GetTeam() != TEAM_SPECTATORS && !pPlayer->m_IsReadyToPlay)
			m_NumPlayerNotReady++;
	}
	return m_NumPlayerNotReady == 0;
}

void IGameController::SetPlayersReadyState(bool ReadyState)
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(i);
		if(pPlayer && GameServer()->IsClientPlayer(i) && (ReadyState || !pPlayer->m_DeadSpecMode))
			pPlayer->m_IsReadyToPlay = ReadyState;
	}
	GetPlayersReadyState();
}

// to be called when a player changes state, spectates or disconnects
void IGameController::CheckReadyStates(int WithoutID)
{
	if(m_PlayerReadyMode)
	{
		bool AllReady = GetPlayersReadyState(WithoutID);
		switch(m_GameState)
		{
		case IGS_WARMUP_USER:
			// all players are ready -> start actual warmup
			if(AllReady && m_GameStateTimer == TIMER_INFINITE)
				SetGameState(IGS_WARMUP_USER, m_Warmup);
			break;
		case IGS_GAME_PAUSED:
			// all players are ready -> unpause the game
			if(AllReady)
				SetGameState(IGS_GAME_PAUSED, 0);
			break;
		case IGS_WARMUP_GAME:
		case IGS_GAME_RUNNING:
		case IGS_START_COUNTDOWN:
		case IGS_END_MATCH:
		case IGS_END_ROUND:
			break;
		}
	}
}

// balancing
void IGameController::CheckTeamBalance()
{
	if(!IsTeamplay() || !m_TeambalanceTime)
	{
		m_UnbalancedTick = TBALANCE_OK;
		return;
	}

	// check if teams are unbalanced
	char aBuf[256];
	if(AreTeamsUnbalanced())
	{
		str_format(aBuf, sizeof(aBuf), "Teams are NOT balanced (red=%d blue=%d)", m_aTeamSize[TEAM_RED], m_aTeamSize[TEAM_BLUE]);
		if(m_UnbalancedTick <= TBALANCE_OK)
			m_UnbalancedTick = Server()->Tick();
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "Teams are balanced (red=%d blue=%d)", m_aTeamSize[TEAM_RED], m_aTeamSize[TEAM_BLUE]);
		m_UnbalancedTick = TBALANCE_OK;
	}
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
}

void IGameController::DoTeamBalance()
{
	if(!IsTeamplay() || !m_TeambalanceTime || !AreTeamsUnbalanced())
		return;

	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", "Balancing teams");

	float aTeamScore[2] = {0};
	float aPlayerScore[MAX_CLIENTS] = {0.0f};

	// gather stats
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(i);
		if(pPlayer && pPlayer->GetTeam() != TEAM_SPECTATORS)
		{
			aPlayerScore[i] = pPlayer->m_Score * Server()->TickSpeed() * 60.0f /
					  (Server()->Tick() - pPlayer->m_ScoreStartTick);
			aTeamScore[pPlayer->GetTeam()] += aPlayerScore[i];
		}
	}

	int BiggerTeam = (m_aTeamSize[TEAM_RED] > m_aTeamSize[TEAM_BLUE]) ? TEAM_RED : TEAM_BLUE;
	int NumBalance = absolute(m_aTeamSize[TEAM_RED] - m_aTeamSize[TEAM_BLUE]) / 2;

	// balance teams
	do
	{
		CPlayer *pPlayer = 0;
		float ScoreDiff = aTeamScore[BiggerTeam];
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CPlayer *pRoomPlayer = GetPlayerIfInRoom(i);
			if(!pRoomPlayer || !CanBeMovedOnBalance(pRoomPlayer))
				continue;

			// remember the player whom would cause lowest score-difference
			if(pRoomPlayer->GetTeam() == BiggerTeam &&
				(!pPlayer || absolute((aTeamScore[BiggerTeam ^ 1] + aPlayerScore[i]) - (aTeamScore[BiggerTeam] - aPlayerScore[i])) < ScoreDiff))
			{
				pPlayer = pRoomPlayer;
				ScoreDiff = absolute((aTeamScore[BiggerTeam ^ 1] + aPlayerScore[i]) - (aTeamScore[BiggerTeam] - aPlayerScore[i]));
			}
		}

		// move the player to the other team
		if(pPlayer)
		{
			int Temp = pPlayer->m_LastActionTick;
			DoTeamChange(pPlayer, BiggerTeam ^ 1);
			pPlayer->m_LastActionTick = Temp;
			pPlayer->Respawn();
			int Team = pPlayer->GetTeam();
			SendGameMsg(GAMEMSG_TEAM_BALANCE_VICTIM, pPlayer->GetCID(), &Team);
		}
	} while(--NumBalance);

	m_UnbalancedTick = TBALANCE_OK;
	SendGameMsg(GAMEMSG_TEAM_BALANCE, -1);
}

// event
int IGameController::OnInternalCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, int Weapon)
{
	int DeathFlag = OnCharacterDeath(pVictim, pKiller, Weapon);

	if(!(DeathFlag & DEATH_KEEP_SOLO))
		GameServer()->Teams()->m_Core.SetSolo(pVictim->GetPlayer()->GetCID(), false);

	if(!(DeathFlag & DEATH_SKIP_SCORE))
	{
		// do scoreing
		if(!pKiller || Weapon == WEAPON_GAME)
			return 0;
		if(pKiller == pVictim->GetPlayer())
			pVictim->GetPlayer()->m_Score--; // suicide or world
		else
		{
			if(IsTeamplay() && pVictim->GetPlayer()->GetTeam() == pKiller->GetTeam())
				pKiller->m_Score--; // teamkill
			else
				pKiller->m_Score++; // normal kill
		}
	}

	if(!(DeathFlag & DEATH_NO_SUICIDE_PANATY) && Weapon == WEAPON_SELF)
		pVictim->GetPlayer()->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() * 3.0f;

	// update spectator modes for dead players
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(i);
		if(pPlayer && pPlayer->m_DeadSpecMode)
			pPlayer->UpdateDeadSpecMode();
	}

	return DeathFlag;
}

void IGameController::OnInternalCharacterSpawn(CCharacter *pChr)
{
	// check respawn state
	pChr->GetPlayer()->m_RespawnDisabled = GetStartRespawnState();
	OnCharacterSpawn(pChr);
}

bool IGameController::OnInternalCharacterTile(CCharacter *pChr, int MapIndex)
{
	if(OnCharacterTile(pChr, MapIndex))
		return true;

	if(!pChr->IsAlive())
		return false;

	CPlayer *pPlayer = pChr->GetPlayer();
	int ClientID = pPlayer->GetCID();

	int m_TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
	int m_TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);

	// solo part
	if(((m_TileIndex == TILE_SOLO_ENABLE) || (m_TileFIndex == TILE_SOLO_ENABLE)) && !GameServer()->Teams()->m_Core.GetSolo(ClientID))
	{
		GameServer()->SendChatLocalized(ClientID, "You are now in a solo part");
		pChr->SetSolo(true);
	}
	else if(((m_TileIndex == TILE_SOLO_DISABLE) || (m_TileFIndex == TILE_SOLO_DISABLE)) && GameServer()->Teams()->m_Core.GetSolo(ClientID))
	{
		GameServer()->SendChatLocalized(ClientID, "You are now out of the solo part");
		pChr->SetSolo(false);
	}

	return false;
}

void IGameController::OnInternalEntity(int Index, vec2 Pos, int Layer, int Flags, int MegaMapIndex, int Number)
{
	if(m_MapIndex > 0 && MegaMapIndex != m_MapIndex)
		return;

	if(Index < 0 || OnEntity(Index, Pos, Layer, Flags, Number))
		return;

	int Type = -1;
	int SubType = 0;

	int x, y;
	x = (Pos.x - 16.0f) / 32.0f;
	y = (Pos.y - 16.0f) / 32.0f;
	int sides[8];
	sides[0] = GameServer()->Collision()->Entity(x, y + 1, Layer);
	sides[1] = GameServer()->Collision()->Entity(x + 1, y + 1, Layer);
	sides[2] = GameServer()->Collision()->Entity(x + 1, y, Layer);
	sides[3] = GameServer()->Collision()->Entity(x + 1, y - 1, Layer);
	sides[4] = GameServer()->Collision()->Entity(x, y - 1, Layer);
	sides[5] = GameServer()->Collision()->Entity(x - 1, y - 1, Layer);
	sides[6] = GameServer()->Collision()->Entity(x - 1, y, Layer);
	sides[7] = GameServer()->Collision()->Entity(x - 1, y + 1, Layer);

	if(Index >= ENTITY_SPAWN && Index <= ENTITY_SPAWN_BLUE)
	{
		int Type = Index - ENTITY_SPAWN;
		m_aaSpawnPoints[Type][m_aNumSpawnPoints[Type]] = Pos;
		m_aNumSpawnPoints[Type] = minimum(m_aNumSpawnPoints[Type] + 1, (int)(sizeof(m_aaSpawnPoints[0]) / sizeof(m_aaSpawnPoints[0][0])));
	}

	else if(Index == ENTITY_DOOR)
	{
		for(int i = 0; i < 8; i++)
		{
			if(sides[i] >= ENTITY_LASER_SHORT && sides[i] <= ENTITY_LASER_LONG)
			{
				new CDoor(
					GameWorld(), //GameWorld
					Pos, //Pos
					pi / 4 * i, //Rotation
					32 * 3 + 32 * (sides[i] - ENTITY_LASER_SHORT) * 3, //Length
					Number //Number
				);
			}
		}
	}
	else if(Index == ENTITY_CRAZY_SHOTGUN_EX)
	{
		int Dir;
		if(!Flags)
			Dir = 0;
		else if(Flags == ROTATION_90)
			Dir = 1;
		else if(Flags == ROTATION_180)
			Dir = 2;
		else
			Dir = 3;
		float Deg = Dir * (pi / 2);
		// MYTODO: add back ddnet freeze bullet
		// CProjectile *bullet = new CProjectile(
		// 	GameWorld(),
		// 	WEAPON_SHOTGUN, //Type
		// 	-1, //Owner
		// 	Pos, //Pos
		// 	vec2(sin(Deg), cos(Deg)), //Dir
		// 	-2, //Span
		// 	0, //Damage
		// 	true, //Explosive
		// 	0, //Force
		// 	(g_Config.m_SvShotgunBulletSound) ? SOUND_GRENADE_EXPLODE : -1, //SoundImpact
		// 	true, //Freeze
		// 	Layer,
		// 	Number);
		// bullet->SetBouncing(2 - (Dir % 2));
	}
	else if(Index == ENTITY_CRAZY_SHOTGUN)
	{
		int Dir;
		if(!Flags)
			Dir = 0;
		else if(Flags == (TILEFLAG_ROTATE))
			Dir = 1;
		else if(Flags == (TILEFLAG_VFLIP | TILEFLAG_HFLIP))
			Dir = 2;
		else
			Dir = 3;
		float Deg = Dir * (pi / 2);
		// CProjectile *bullet = new CProjectile(
		// 	GameWorld(),
		// 	WEAPON_SHOTGUN, //Type
		// 	-1, //Owner
		// 	Pos, //Pos
		// 	vec2(sin(Deg), cos(Deg)), //Dir
		// 	-2, //Span
		// 	0, //Damage
		// 	false, //Explosive
		// 	0,
		// 	SOUND_GRENADE_EXPLODE,
		// 	true, //Freeze
		// 	Layer,
		// 	Number);
		// bullet->SetBouncing(2 - (Dir % 2));
	}

	if(Index == ENTITY_ARMOR_1)
	{
		Type = POWERUP_ARMOR;
	}
	else if(Index == ENTITY_HEALTH_1)
	{
		Type = POWERUP_HEALTH;
	}
	else if(Index == ENTITY_WEAPON_SHOTGUN)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_SHOTGUN;
	}
	else if(Index == ENTITY_WEAPON_GRENADE)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_GRENADE;
	}
	else if(Index == ENTITY_WEAPON_LASER)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_LASER;
	}
	else if(Index == ENTITY_POWERUP_NINJA)
	{
		Type = POWERUP_NINJA;
		SubType = WEAPON_NINJA;
	}
	else if(Index >= ENTITY_LASER_FAST_CCW && Index <= ENTITY_LASER_FAST_CW)
	{
		int sides2[8];
		sides2[0] = GameServer()->Collision()->Entity(x, y + 2, Layer);
		sides2[1] = GameServer()->Collision()->Entity(x + 2, y + 2, Layer);
		sides2[2] = GameServer()->Collision()->Entity(x + 2, y, Layer);
		sides2[3] = GameServer()->Collision()->Entity(x + 2, y - 2, Layer);
		sides2[4] = GameServer()->Collision()->Entity(x, y - 2, Layer);
		sides2[5] = GameServer()->Collision()->Entity(x - 2, y - 2, Layer);
		sides2[6] = GameServer()->Collision()->Entity(x - 2, y, Layer);
		sides2[7] = GameServer()->Collision()->Entity(x - 2, y + 2, Layer);

		float AngularSpeed = 0.0f;
		int Ind = Index - ENTITY_LASER_STOP;
		int M;
		if(Ind < 0)
		{
			Ind = -Ind;
			M = 1;
		}
		else if(Ind == 0)
			M = 0;
		else
			M = -1;

		if(Ind == 0)
			AngularSpeed = 0.0f;
		else if(Ind == 1)
			AngularSpeed = pi / 360;
		else if(Ind == 2)
			AngularSpeed = pi / 180;
		else if(Ind == 3)
			AngularSpeed = pi / 90;
		AngularSpeed *= M;

		for(int i = 0; i < 8; i++)
		{
			if(sides[i] >= ENTITY_LASER_SHORT && sides[i] <= ENTITY_LASER_LONG)
			{
				CLight *Lgt = new CLight(GameWorld(), Pos, pi / 4 * i, 32 * 3 + 32 * (sides[i] - ENTITY_LASER_SHORT) * 3, Layer, Number);
				Lgt->m_AngularSpeed = AngularSpeed;
				if(sides2[i] >= ENTITY_LASER_C_SLOW && sides2[i] <= ENTITY_LASER_C_FAST)
				{
					Lgt->m_Speed = 1 + (sides2[i] - ENTITY_LASER_C_SLOW) * 2;
					Lgt->m_CurveLength = Lgt->m_Length;
				}
				else if(sides2[i] >= ENTITY_LASER_O_SLOW && sides2[i] <= ENTITY_LASER_O_FAST)
				{
					Lgt->m_Speed = 1 + (sides2[i] - ENTITY_LASER_O_SLOW) * 2;
					Lgt->m_CurveLength = 0;
				}
				else
					Lgt->m_CurveLength = Lgt->m_Length;
			}
		}
	}
	else if(Index >= ENTITY_DRAGGER_WEAK && Index <= ENTITY_DRAGGER_STRONG)
	{
		new CDragger(GameWorld(), Pos, Index - ENTITY_DRAGGER_WEAK + 1, false, Layer, Number);
	}
	else if(Index >= ENTITY_DRAGGER_WEAK_NW && Index <= ENTITY_DRAGGER_STRONG_NW)
	{
		new CDragger(GameWorld(), Pos, Index - ENTITY_DRAGGER_WEAK_NW + 1, true, Layer, Number);
	}
	else if(Index == ENTITY_PLASMAE)
	{
		new CGun(GameWorld(), Pos, false, true, Layer, Number);
	}
	else if(Index == ENTITY_PLASMAF)
	{
		new CGun(GameWorld(), Pos, true, false, Layer, Number);
	}
	else if(Index == ENTITY_PLASMA)
	{
		new CGun(GameWorld(), Pos, true, true, Layer, Number);
	}
	else if(Index == ENTITY_PLASMAU)
	{
		new CGun(GameWorld(), Pos, false, false, Layer, Number);
	}

	if(Type != -1)
	{
		CPickup *pPickup = new CPickup(GameWorld(), Type, SubType);
		pPickup->m_Pos = Pos;
	}
}

void IGameController::OnKill(CPlayer *pPlayer)
{
	if(m_KillDelay == -1)
		return;
	if(pPlayer->m_LastKill && pPlayer->m_LastKill + Server()->TickSpeed() * m_KillDelay > Server()->Tick())
		return;
	if(pPlayer->IsPaused())
		return;

	CCharacter *pChr = pPlayer->GetCharacter();
	if(!pChr)
		return;

	pPlayer->m_LastKill = Server()->Tick();
	pPlayer->KillCharacter(WEAPON_SELF);
}

int IGameController::OnPickup(CPickup *pPickup, CCharacter *pChar, SPickupSound *pSound)
{
	int Type = pPickup->GetType();
	int Subtype = pPickup->GetSubtype();
	pSound->m_Global = false;
	switch(pPickup->GetType())
	{
	case POWERUP_HEALTH:
		if(pChar->IncreaseHealth(1))
		{
			pSound->m_Sound = SOUND_PICKUP_HEALTH;
			return g_pData->m_aPickups[Type].m_Respawntime * Server()->TickSpeed();
		}
		break;

	case POWERUP_ARMOR:
		if(pChar->IncreaseArmor(1))
		{
			pSound->m_Sound = SOUND_PICKUP_ARMOR;
			return g_pData->m_aPickups[Type].m_Respawntime * Server()->TickSpeed();
		}

		break;

	case POWERUP_WEAPON:
		if(Subtype >= 0 && Subtype < NUM_WEAPON_SLOTS)
		{
			int WeaponID = -1;
			int Ammo = g_pData->m_Weapons.m_aId[Subtype].m_Maxammo;
			switch(Subtype)
			{
			case WEAPON_HAMMER:
				WeaponID = WEAPON_ID_HAMMER;
				break;
			case WEAPON_GUN:
				WeaponID = WEAPON_ID_PISTOL;
				break;
			case WEAPON_SHOTGUN:
				WeaponID = WEAPON_ID_SHOTGUN;
				break;
			case WEAPON_GRENADE:
				WeaponID = WEAPON_ID_GRENADE;
				break;
			case WEAPON_LASER:
				WeaponID = WEAPON_ID_LASER;
				break;
			}
			if(WeaponID < 0)
				return -2;

			if(pChar->GiveWeapon(Subtype, WeaponID, Ammo))
			{
				if(Subtype == WEAPON_GRENADE)
					pSound->m_Sound = SOUND_PICKUP_GRENADE;
				else if(Subtype == WEAPON_SHOTGUN)
					pSound->m_Sound = SOUND_PICKUP_SHOTGUN;
				else if(Subtype == WEAPON_LASER)
					pSound->m_Sound = SOUND_PICKUP_SHOTGUN;

				if(pChar->GetPlayer())
					GameServer()->SendWeaponPickup(pChar->GetPlayer()->GetCID(), Subtype);

				return g_pData->m_aPickups[Type].m_Respawntime * Server()->TickSpeed();
			}
		}
		break;

	case POWERUP_NINJA:
	{
		// activate ninja on target player
		pChar->SetPowerUpWeapon(WEAPON_ID_NINJA, -1);
		pSound->m_Sound = SOUND_PICKUP_NINJA;
		pSound->m_Global = true;

		// loop through all players, setting their emotes
		CCharacter *pC = static_cast<CCharacter *>(GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER));
		for(; pC; pC = (CCharacter *)pC->TypeNext())
		{
			if(pC != pChar)
				pC->SetEmote(EMOTE_SURPRISE, Server()->Tick() + Server()->TickSpeed());
		}

		pChar->SetEmote(EMOTE_ANGRY, Server()->Tick() + 1200 * Server()->TickSpeed() / 1000);
		if(pChar->GetPlayer())
			GameServer()->SendWeaponPickup(pChar->GetPlayer()->GetCID(), Subtype);

		return g_pData->m_aPickups[Type].m_Respawntime * Server()->TickSpeed();
		break;
	}
	default:
		return -2;
	};

	return -1;
}

void IGameController::OnInternalPlayerJoin(CPlayer *pPlayer, int Type)
{
	int ClientID = pPlayer->GetCID();
	pPlayer->GameReset();
	pPlayer->m_IsReadyToPlay = !IsPlayerReadyMode();
	pPlayer->m_RespawnDisabled = GetStartRespawnState();
	pPlayer->m_Vote = 0;
	pPlayer->m_VotePos = 0;
	pPlayer->m_PauseCount = 0;

	// clear vote options for joining player
	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, pPlayer->GetCID());
	pPlayer->m_SendVoteIndex = 0;

	if(GameServer()->m_VoteCloseTime > 0)
		GameServer()->SendVoteSet(ClientID);
	else
		SendVoteSet(ClientID);

	// update game info first
	UpdateGameInfo(ClientID);

	// change team second, and don't update team if it is a reload
	if(Type != INSTANCE_CONNECTION_RELOAD || pPlayer->GetTeam() != TEAM_SPECTATORS)
		pPlayer->SetTeam(GetStartTeam());

	if(pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		int Team = pPlayer->GetTeam();
		++m_aTeamSize[Team];
		dbg_msg("game", "team size increased to %d, team='%d', ddrteam='%d'", m_aTeamSize[Team], Team, GameWorld()->Team());
		m_UnbalancedTick = TBALANCE_CHECK;
	}

	// sixup: update team info for fake spectators
	pPlayer->SendCurrentTeamInfo();

	pPlayer->Respawn();

	m_aFakeClientBroadcast[ClientID].m_LastGameState = -1;
	m_aFakeClientBroadcast[ClientID].m_LastTimer = -1;
	m_aFakeClientBroadcast[ClientID].m_LastPlayerNotReady = -1;
	m_aFakeClientBroadcast[ClientID].m_NextBroadcastTick = -1;
	m_aFakeClientBroadcast[ClientID].m_LastDeadSpec = false;
	if(Type == INSTANCE_CONNECTION_SERVER)
		m_aFakeClientBroadcast[ClientID].m_DisableUntil = Server()->Tick() + Server()->TickSpeed() * 3;
	else
		m_aFakeClientBroadcast[ClientID].m_DisableUntil = -1;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "ddrteam_join player='%d:%s' team=%d ddrteam='%d'", ClientID, Server()->ClientName(ClientID), pPlayer->GetTeam(), GameWorld()->Team());
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	if(Type != INSTANCE_CONNECTION_RELOAD)
	{
		if(Type == INSTANCE_CONNECTION_SERVER)
		{
			if(g_Config.m_SvRoom == 0)
				str_format(aBuf, sizeof(aBuf), "'%s' entered and joined the %s", Server()->ClientName(ClientID), GetTeamName(pPlayer->GetTeam()));
			else
				str_format(aBuf, sizeof(aBuf), "'%s' entered and joined the %s in %s room %d", Server()->ClientName(ClientID), GetTeamName(pPlayer->GetTeam()), m_pGameType, GameWorld()->Team());

			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, -1);
		}
		else if(Type == INSTANCE_CONNECTION_CREATE)
		{
			str_format(aBuf, sizeof(aBuf), "'%s' created %s room %d and joined the %s", Server()->ClientName(ClientID), m_pGameType, GameWorld()->Team(), GetTeamName(pPlayer->GetTeam()));
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, -1);
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "'%s' entered the room and joined the %s", Server()->ClientName(ClientID), GetTeamName(pPlayer->GetTeam()));
			SendChatTarget(-1, aBuf);
		}
	}

	GetPlayersReadyState(-1);
	OnPlayerJoin(pPlayer);
	TryStartWarmup();

	m_VoteUpdate = true;
}

void IGameController::OnInternalPlayerLeave(CPlayer *pPlayer, int Type)
{
	int ClientID = pPlayer->GetCID();

	if(GameServer()->Server()->ClientIngame(ClientID))
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "ddrteam_leave player='%d:%s' ddrteam='%d'", ClientID, Server()->ClientName(ClientID), GameWorld()->Team());
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

		if(Type == INSTANCE_CONNECTION_NORMAL)
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "'%s' has left the room", Server()->ClientName(ClientID));
			for(int i = 0; i < MAX_CLIENTS; i++)
				if(GetPlayerIfInRoom(i) && i != pPlayer->GetCID())
					GameServer()->SendChatTarget(i, aBuf);
		}
		else if(Type == INSTANCE_CONNECTION_FORCED)
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "'%s' has left the room (kicked)", Server()->ClientName(ClientID));
			for(int i = 0; i < MAX_CLIENTS; i++)
				if(GetPlayerIfInRoom(i) && i != pPlayer->GetCID())
					GameServer()->SendChatTarget(i, aBuf);
		}
	}

	if(pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		--m_aTeamSize[pPlayer->GetTeam()];
		dbg_msg("game", "team size decreased to %d, team='%d', ddrteam='%d'", m_aTeamSize[pPlayer->GetTeam()], pPlayer->GetTeam(), GameWorld()->Team());
		m_UnbalancedTick = TBALANCE_CHECK;
	}

	CheckReadyStates(ClientID);
	OnPlayerLeave(pPlayer);

	if(m_VoteVictim == ClientID && (GameWorld()->Team() > 0 || Type == INSTANCE_CONNECTION_SERVER))
	{
		EndVote(true);
	}
	m_VoteUpdate = true;
	TryStartWarmup(true);
}

void IGameController::OnReset()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(i);
		if(pPlayer)
		{
			pPlayer->m_RespawnDisabled = false;
			pPlayer->Respawn();
			pPlayer->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() / 2;
			if(m_RoundCount == 0)
			{
				pPlayer->m_Score = 0;
				pPlayer->m_ScoreStartTick = Server()->Tick();
			}
		}
	}

	OnWorldReset();
	dbg_msg("game", "world cleared, ddrteam='%d'", GameWorld()->Team());
}

// game
void IGameController::DoWincheckMatch()
{
	if(IsTeamplay())
	{
		// check score win condition
		if((m_GameInfo.m_ScoreLimit > 0 && (m_aTeamscore[TEAM_RED] >= m_GameInfo.m_ScoreLimit || m_aTeamscore[TEAM_BLUE] >= m_GameInfo.m_ScoreLimit)) ||
			(m_GameInfo.m_TimeLimit > 0 && !IsRoundTimer() && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60) ||
			(m_GameInfo.m_MatchNum > 0 && m_GameInfo.m_MatchCurrent >= m_GameInfo.m_MatchNum))
		{
			if(m_aTeamscore[TEAM_RED] != m_aTeamscore[TEAM_BLUE] || !UseSuddenDeath())
				EndMatch();
			else
				m_SuddenDeath = 1;
		}
	}
	else
	{
		// gather some stats
		int Topscore = 0;
		int TopscoreCount = 0;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CPlayer *pPlayer = GetPlayerIfInRoom(i);
			if(pPlayer)
			{
				if(pPlayer->m_Score > Topscore)
				{
					Topscore = pPlayer->m_Score;
					TopscoreCount = 1;
				}
				else if(pPlayer->m_Score == Topscore)
					TopscoreCount++;
			}
		}

		// check score win condition
		if((m_GameInfo.m_ScoreLimit > 0 && Topscore >= m_GameInfo.m_ScoreLimit) ||
			(m_GameInfo.m_TimeLimit > 0 && !IsRoundTimer() && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60) ||
			(m_GameInfo.m_MatchNum > 0 && m_GameInfo.m_MatchCurrent >= m_GameInfo.m_MatchNum))
		{
			if(TopscoreCount == 1 || !UseSuddenDeath())
				EndMatch();
			else
				m_SuddenDeath = 1;
		}
	}
}

void IGameController::ResetGame()
{
	// reset the game
	GameWorld()->m_ResetRequested = true;

	SetGameState(IGS_GAME_RUNNING);

	// do team-balancing
	DoTeamBalance();
}

void IGameController::SetGameState(EGameState GameState, int Timer)
{
	// change game state
	switch(GameState)
	{
	case IGS_WARMUP_GAME:
		// game based warmup is only possible when game or any warmup is running
		if(m_GameState == IGS_GAME_RUNNING || m_GameState == IGS_WARMUP_GAME || m_GameState == IGS_WARMUP_USER)
		{
			if(Timer == TIMER_INFINITE)
			{
				// run warmup till there're enough players
				m_GameState = GameState;
				m_GameStateTimer = TIMER_INFINITE;
				m_SuddenDeath = 0;

				// enable respawning in survival when activating warmup
				if(IsSurvival())
				{
					for(int i = 0; i < MAX_CLIENTS; ++i)
					{
						CPlayer *pPlayer = GetPlayerIfInRoom(i);
						if(pPlayer)
							pPlayer->m_RespawnDisabled = false;
					}
				}
			}
			else if(Timer == 0)
			{
				// start new match
				StartMatch();
			}
		}
		break;
	case IGS_WARMUP_USER:
		// user based warmup is only possible when the game or any warmup is running
		if(m_GameState == IGS_GAME_RUNNING || m_GameState == IGS_GAME_PAUSED || m_GameState == IGS_WARMUP_GAME || m_GameState == IGS_WARMUP_USER)
		{
			if(Timer != 0)
			{
				// start warmup
				if(Timer < 0)
				{
					m_GameState = GameState;
					m_GameStateTimer = TIMER_INFINITE;
					if(m_PlayerReadyMode & 1)
					{
						// run warmup till all players are ready
						SetPlayersReadyState(false);
					}
				}
				else if(Timer > 0)
				{
					// user warmup called, mark all player ready
					SetPlayersReadyState(true);
					// run warmup for a specific time intervall
					m_GameState = GameState;
					m_GameStateTimer = Timer * Server()->TickSpeed();
				}

				// enable respawning in survival when activating warmup
				if(IsSurvival())
				{
					for(int i = 0; i < MAX_CLIENTS; ++i)
					{
						CPlayer *pPlayer = GetPlayerIfInRoom(i);
						if(pPlayer)
							pPlayer->m_RespawnDisabled = false;
					}
				}
				GameWorld()->m_Paused = false;
				m_PauseRequested = false;
				m_SuddenDeath = 0;
			}
			else
			{
				// start new match
				StartMatch();
			}
		}
		break;
	case IGS_START_COUNTDOWN:
		// only possible when game, pause or start countdown is running
		if(m_GameState == IGS_GAME_RUNNING || m_GameState == IGS_GAME_PAUSED || m_GameState == IGS_START_COUNTDOWN)
		{
			if((m_Countdown < 0 && IsSurvival()) || m_Countdown > 0)
			{
				m_GameState = GameState;
				m_GameStateTimer = absolute(m_Countdown) * Server()->TickSpeed();
				GameWorld()->m_Paused = true;
			}
			else
			{
				// no countdown, start new match right away
				SetGameState(IGS_GAME_RUNNING);
			}
		}
		break;
	case IGS_GAME_RUNNING:
		// always possible
		{
			m_GameState = GameState;
			m_GameStateTimer = TIMER_INFINITE;
			SetPlayersReadyState(true);
			GameWorld()->m_Paused = false;
			m_PauseRequested = false;
		}
		break;
	case IGS_GAME_PAUSED:
		// only possible when game is running or paused, or when game based warmup is running
		if(m_GameState == IGS_GAME_RUNNING || m_GameState == IGS_GAME_PAUSED || m_GameState == IGS_WARMUP_GAME)
		{
			if(Timer != 0)
			{
				// start pause
				if(Timer < 0)
				{
					// pauses infinitely till all players are ready or disabled via rcon command
					m_GameStateTimer = TIMER_INFINITE;
					SetPlayersReadyState(false);
				}
				else
				{
					// pauses for a specific time interval
					m_GameStateTimer = Timer * Server()->TickSpeed();
				}

				m_GameState = GameState;
				GameWorld()->m_Paused = true;
			}
			else
			{
				// start a countdown to end pause
				SetGameState(IGS_START_COUNTDOWN);
			}
		}
		break;
	case IGS_END_ROUND:
	case IGS_END_MATCH:
		m_PauseRequested = false;
		if(GameState == IGS_END_ROUND)
		{
			DoWincheckMatch();
			if(IsEndMatch())
				break;
		}

		// only possible when game is running or over
		if(m_GameState == IGS_GAME_RUNNING || m_GameState == IGS_END_MATCH || m_GameState == IGS_END_ROUND || m_GameState == IGS_GAME_PAUSED)
		{
			m_GameState = GameState;
			m_GameStateTimer = Timer * Server()->TickSpeed();
			if(m_GameState != IGS_END_ROUND)
				m_SuddenDeath = 0;
			GameWorld()->m_Paused = true;
		}
	}
}

void IGameController::TryStartWarmup(bool FallbackToWarmup)
{
	// no player is playing. reset the entire match
	if(m_GameState != IGS_WARMUP_GAME && HasNoPlayers())
	{
		ResetMatch();
		return;
	}

	if((m_GameState != IGS_WARMUP_GAME && m_GameState != IGS_WARMUP_USER) || m_GameStateTimer != TIMER_INFINITE)
		return;

	if(HasEnoughPlayers())
	{
		if(m_PlayerReadyMode & 1)
		{
			if(m_GameState != IGS_WARMUP_USER)
				SetGameState(IGS_WARMUP_USER, TIMER_INFINITE);
		}
		else
			SetGameState(IGS_WARMUP_USER, m_Warmup);
	}
	else if(FallbackToWarmup)
	{
		SetPlayersReadyState(true);
		SetGameState(IGS_WARMUP_GAME, TIMER_INFINITE);
	}
}

void IGameController::StartMatch()
{
	// If we passed warmup and still not enough player, do infinite timer
	if(IsWarmup() && !HasEnoughPlayers())
	{
		SetGameState(IGS_WARMUP_GAME, TIMER_INFINITE);
		return;
	}

	ResetGame();
	CheckGameInfo(false);

	m_GameStartTick = Server()->Tick();
	m_RoundCount = 0;
	m_SuddenDeath = 0;
	m_GameInfo.m_MatchCurrent = m_RoundCount + 1;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(i);
		if(pPlayer)
			pPlayer->m_PauseCount = 0;
		UpdateGameInfo(i);
	}

	m_aTeamscore[TEAM_RED] = 0;
	m_aTeamscore[TEAM_BLUE] = 0;

	if(HasEnoughPlayers())
	{
		SetGameState(IGS_START_COUNTDOWN);
		OnGameStart(false);

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "start match type='%s' teamplay='%d' ddrteam='%d'", m_pGameType, m_GameFlags & IGF_TEAMS, GameWorld()->Team());
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}
	else
		SetGameState(IGS_WARMUP_USER, TIMER_INFINITE);
}

void IGameController::ResetMatch()
{
	ResetGame();
	CheckGameInfo(false);

	m_GameStartTick = Server()->Tick();
	m_RoundCount = 0;
	m_SuddenDeath = 0;

	for(int i = 0; i < MAX_CLIENTS; ++i)
		UpdateGameInfo(i);

	m_aTeamscore[TEAM_RED] = 0;
	m_aTeamscore[TEAM_BLUE] = 0;

	// default to wait for more players
	SetGameState(IGS_WARMUP_GAME, TIMER_INFINITE);

	TryStartWarmup();
}

void IGameController::StartRound()
{
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

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "start round type='%s' teamplay='%d' ddrteam='%d'", m_pGameType, m_GameFlags & IGF_TEAMS, GameWorld()->Team());
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}
	else
		SetGameState(IGS_WARMUP_GAME, TIMER_INFINITE);
}

void IGameController::OnPlayerReadyChange(CPlayer *pPlayer)
{
	if((IsPlayerReadyMode() || ((m_PlayerReadyMode & 2) && m_GameState == IGS_GAME_RUNNING)) && pPlayer->GetTeam() != TEAM_SPECTATORS && !pPlayer->m_DeadSpecMode)
	{
		if(m_GameState == IGS_GAME_RUNNING && pPlayer->m_IsReadyToPlay)
		{
			if(!m_PauseRequested)
			{
				int ClientID = pPlayer->GetCID();

				if(m_PausePerMatch)
				{
					if(pPlayer->m_PauseCount >= m_PausePerMatch)
					{
						SendChatTarget(ClientID, "You can't pause the match anymore");
						return;
					}
				}

				pPlayer->m_PauseCount++;
				m_PauseRequested = true;
				m_PauseRequestedTicks = 0;
				// SetGameState(IGS_GAME_PAUSED, TIMER_INFINITE);
				SendGameMsg(GAMEMSG_GAME_PAUSED, -1, &ClientID);
			}
		}
		else
		{
			// change players ready state
			pPlayer->m_IsReadyToPlay ^= 1;
		}

		CheckReadyStates();
	}
}

void IGameController::SwapTeamScore()
{
	if(!IsTeamplay())
		return;

	int Score = m_aTeamscore[TEAM_RED];
	m_aTeamscore[TEAM_RED] = m_aTeamscore[TEAM_BLUE];
	m_aTeamscore[TEAM_BLUE] = Score;
}

void IGameController::SwapTeams()
{
	if(!IsTeamplay())
		return;

	SendGameMsg(GAMEMSG_TEAM_SWAP, -1);

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(i);
		if(pPlayer && pPlayer->GetTeam() != TEAM_SPECTATORS)
			DoTeamChange(pPlayer, pPlayer->GetTeam() ^ 1, false);
	}

	SwapTeamScore();
}

void IGameController::ShuffleTeams()
{
	if(!IsTeamplay())
		return;

	int rnd = 0;
	int PlayerTeam = 0;
	CPlayer *aPlayer[MAX_CLIENTS];

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(i);
		if(pPlayer && pPlayer->GetTeam() != TEAM_SPECTATORS)
			aPlayer[PlayerTeam++] = pPlayer;
	}

	SendGameMsg(GAMEMSG_TEAM_SHUFFLE, -1);

	//creating random permutation
	for(int i = PlayerTeam; i > 1; i--)
	{
		rnd = rand() % i;
		CPlayer *tmp = aPlayer[rnd];
		aPlayer[rnd] = aPlayer[i - 1];
		aPlayer[i - 1] = tmp;
	}
	//uneven Number of Players?
	rnd = PlayerTeam % 2 ? rand() % 2 : 0;

	for(int i = 0; i < PlayerTeam; i++)
		DoTeamChange(aPlayer[i], i < (PlayerTeam + rnd) / 2 ? TEAM_RED : TEAM_BLUE, false);
}

void IGameController::FakeGameMsgSound(int SnappingClient, int SoundID)
{
	CNetMsg_Sv_SoundGlobal Msg;
	Msg.m_SoundID = SoundID;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, SnappingClient);
}

// for compatibility of 0.7's round ends and infinite warmup
void IGameController::FakeClientBroadcast(int SnappingClient)
{
	if(Server()->IsSixup(SnappingClient))
		return;

	CPlayer *pPlayer = GetPlayerIfInRoom(SnappingClient);
	if(!pPlayer)
		return;

	SBroadcastState *pState = &m_aFakeClientBroadcast[SnappingClient];
	if(Server()->Tick() < pState->m_DisableUntil)
		return;

	int TimerNumber = (int)ceil(m_GameStateTimer / (float)Server()->TickSpeed());

	if(pState->m_LastDeadSpec == pPlayer->m_DeadSpecMode && pState->m_LastPlayerNotReady == m_NumPlayerNotReady && pState->m_LastGameState == m_GameState && pState->m_LastTimer == TimerNumber && (pState->m_NextBroadcastTick < 0 || Server()->Tick() < pState->m_NextBroadcastTick))
		return;

	pState->m_NextBroadcastTick = -1;
	pState->m_LastGameState = m_GameState;
	pState->m_LastTimer = TimerNumber;
	pState->m_LastPlayerNotReady = m_NumPlayerNotReady;
	pState->m_LastDeadSpec = pPlayer->m_DeadSpecMode;

	if(IsPlayerReadyMode() && m_NumPlayerNotReady > 0)
	{
		char aBuf[128];
		bool PlayerNeedToReady = pPlayer->GetTeam() != TEAM_SPECTATORS && pPlayer->m_IsReadyToPlay;
		if(m_NumPlayerNotReady == 1)
			str_format(aBuf, sizeof(aBuf), "%s\n\n\n%d player not ready", PlayerNeedToReady ? "" : "Say '/r' to ready", m_NumPlayerNotReady);
		else
			str_format(aBuf, sizeof(aBuf), "%s\n\n\n%d players not ready", PlayerNeedToReady ? "" : "Say '/r' to ready", m_NumPlayerNotReady);
		GameServer()->SendBroadcast(aBuf, SnappingClient, false);
		pState->m_NextBroadcastTick = Server()->Tick() + 5 * Server()->TickSpeed();
		return;
	}

	switch(m_GameState)
	{
	case IGS_WARMUP_GAME:
	case IGS_WARMUP_USER:
		if(m_GameStateTimer == TIMER_INFINITE && pPlayer->GetTeam() != TEAM_SPECTATORS)
			GameServer()->SendBroadcast("Waiting for more players", SnappingClient, false);
		else
			GameServer()->SendBroadcast(" ", SnappingClient, false);
		pState->m_NextBroadcastTick = Server()->Tick() + 5 * Server()->TickSpeed();
		break;
	case IGS_START_COUNTDOWN:
	case IGS_GAME_PAUSED:
		if(m_GameStateTimer != TIMER_INFINITE)
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Game starts in %d", TimerNumber);
			GameServer()->SendBroadcast(aBuf, SnappingClient, false);
		}
		break;
	case IGS_END_ROUND:
		GameServer()->SendBroadcast("Round over", SnappingClient, false);
		pState->m_NextBroadcastTick = Server()->Tick() + 5 * Server()->TickSpeed();
		break;
	case IGS_GAME_RUNNING:
		if(pPlayer->m_DeadSpecMode)
		{
			GameServer()->SendBroadcast("Wait for next round", SnappingClient, false);
			break;
		}
		GameServer()->SendBroadcast(" ", SnappingClient, false);
		break;
	case IGS_END_MATCH:
		break;
	}
}

void IGameController::Snap(int SnappingClient)
{
	FakeClientBroadcast(SnappingClient);

	bool isSixUp = Server()->IsSixup(SnappingClient);

	int GameStateFlags = 0;
	int GameStateEndTick = 0; // for sixup
	int WarmupTimer = 0; // for 0.6
	switch(m_GameState)
	{
	case IGS_WARMUP_GAME:
	case IGS_WARMUP_USER:
		if(isSixUp)
			GameStateFlags |= protocol7::GAMESTATEFLAG_WARMUP;
		if(m_GameStateTimer != TIMER_INFINITE)
		{
			GameStateEndTick = Server()->Tick() + m_GameStateTimer;
			WarmupTimer = m_GameStateTimer;
		}
		break;
	case IGS_START_COUNTDOWN:
		if(isSixUp)
			GameStateFlags |= protocol7::GAMESTATEFLAG_STARTCOUNTDOWN | protocol7::GAMESTATEFLAG_PAUSED;
		else
			GameStateFlags |= GAMESTATEFLAG_PAUSED;
		if(m_GameStateTimer != TIMER_INFINITE)
			GameStateEndTick = Server()->Tick() + m_GameStateTimer;

		break;
	case IGS_GAME_PAUSED:
		if(isSixUp)
			GameStateFlags |= protocol7::GAMESTATEFLAG_PAUSED;
		else
			GameStateFlags |= GAMESTATEFLAG_PAUSED;
		if(m_GameStateTimer != TIMER_INFINITE)
			GameStateEndTick = Server()->Tick() + m_GameStateTimer;
		break;
	case IGS_END_ROUND:
		if(isSixUp)
			GameStateFlags |= protocol7::GAMESTATEFLAG_ROUNDOVER;
		else
			GameStateFlags |= 0;

		GameStateEndTick = Server()->Tick() - m_GameStartTick - TIMER_END / 2 * Server()->TickSpeed() + m_GameStateTimer;
		break;
	case IGS_END_MATCH:
		if(isSixUp)
			GameStateFlags |= protocol7::GAMESTATEFLAG_GAMEOVER;
		else
			GameStateFlags |= GAMESTATEFLAG_GAMEOVER | GAMESTATEFLAG_PAUSED;

		GameStateEndTick = Server()->Tick() - m_GameStartTick - TIMER_END * Server()->TickSpeed() + m_GameStateTimer;
		break;
	case IGS_GAME_RUNNING:
		// not effected
		break;
	}

	if(m_SuddenDeath)
		GameStateFlags |= GAMESTATEFLAG_SUDDENDEATH;

	if(!isSixUp)
	{
		CNetObj_GameInfo *pGameInfoObj = (CNetObj_GameInfo *)Server()->SnapNewItem(NETOBJTYPE_GAMEINFO, 0, sizeof(CNetObj_GameInfo));
		if(!pGameInfoObj)
			return;

		pGameInfoObj->m_GameFlags = MakeGameFlag(m_GameFlags);
		pGameInfoObj->m_GameStateFlags = GameStateFlags;
		if(IsCountdown() && m_pWorld->m_Paused)
			pGameInfoObj->m_RoundStartTick = m_GameStartTick - 2;
		else
			pGameInfoObj->m_RoundStartTick = m_GameStartTick;
		pGameInfoObj->m_WarmupTimer = WarmupTimer;

		pGameInfoObj->m_RoundNum = 0;
		pGameInfoObj->m_RoundCurrent = m_RoundCount + 1;
		pGameInfoObj->m_ScoreLimit = m_GameInfo.m_ScoreLimit;
		pGameInfoObj->m_TimeLimit = m_GameInfo.m_TimeLimit;
		pGameInfoObj->m_RoundCurrent = m_GameInfo.m_MatchCurrent;
		pGameInfoObj->m_RoundNum = m_GameInfo.m_MatchNum;

		CNetObj_GameInfoEx *pGameInfoEx = (CNetObj_GameInfoEx *)Server()->SnapNewItem(NETOBJTYPE_GAMEINFOEX, 0, sizeof(CNetObj_GameInfoEx));
		if(!pGameInfoEx)
			return;

		pGameInfoEx->m_Flags = m_DDNetInfoFlag;
		pGameInfoEx->m_Flags2 = m_DDNetInfoFlag2;
		pGameInfoEx->m_Version = GAMEINFO_CURVERSION;

		CNetObj_GameData *pGameDataObj = (CNetObj_GameData *)Server()->SnapNewItem(NETOBJTYPE_GAMEDATA, 0, sizeof(CNetObj_GameData));
		if(!pGameDataObj)
			return;

		pGameDataObj->m_TeamscoreRed = m_aTeamscore[TEAM_RED];
		pGameDataObj->m_TeamscoreBlue = m_aTeamscore[TEAM_BLUE];

		SFlagState FlagState;
		if(GetFlagState(&FlagState))
		{
			pGameDataObj->m_FlagCarrierRed = FlagState.m_RedFlagCarrier;
			pGameDataObj->m_FlagCarrierBlue = FlagState.m_BlueFlagCarrier;
		}
		else
		{
			pGameDataObj->m_FlagCarrierRed = 0;
			pGameDataObj->m_FlagCarrierBlue = 0;
		}
	}
	else
	{
		protocol7::CNetObj_GameData *pGameData = static_cast<protocol7::CNetObj_GameData *>(Server()->SnapNewItem(-protocol7::NETOBJTYPE_GAMEDATA, 0, sizeof(protocol7::CNetObj_GameData)));
		if(!pGameData)
			return;

		pGameData->m_GameStartTick = m_GameStartTick;
		pGameData->m_GameStateFlags = GameStateFlags;
		pGameData->m_GameStateEndTick = GameStateEndTick;

		if(IsTeamplay())
		{
			protocol7::CNetObj_GameDataTeam *pGameDataTeam = static_cast<protocol7::CNetObj_GameDataTeam *>(Server()->SnapNewItem(-protocol7::NETOBJTYPE_GAMEDATATEAM, 0, sizeof(protocol7::CNetObj_GameDataTeam)));
			if(!pGameDataTeam)
				return;

			pGameDataTeam->m_TeamscoreRed = m_aTeamscore[TEAM_RED];
			pGameDataTeam->m_TeamscoreBlue = m_aTeamscore[TEAM_BLUE];
		}

		SFlagState FlagState;
		if(GetFlagState(&FlagState))
		{
			protocol7::CNetObj_GameDataFlag *pGameDataFlag = static_cast<protocol7::CNetObj_GameDataFlag *>(Server()->SnapNewItem(-protocol7::NETOBJTYPE_GAMEDATAFLAG, 0, sizeof(protocol7::CNetObj_GameDataFlag)));
			if(!pGameDataFlag)
				return;

			pGameDataFlag->m_FlagDropTickRed = FlagState.m_RedFlagDroppedTick;
			switch(FlagState.m_RedFlagCarrier)
			{
			case FLAG_ATSTAND:
				pGameDataFlag->m_FlagCarrierRed = protocol7::FLAG_ATSTAND;
				break;
			case FLAG_TAKEN:
				pGameDataFlag->m_FlagCarrierRed = protocol7::FLAG_TAKEN;
				break;
			case FLAG_MISSING:
				pGameDataFlag->m_FlagCarrierRed = protocol7::FLAG_MISSING;
				break;
			default:
				pGameDataFlag->m_FlagCarrierRed = FlagState.m_RedFlagCarrier;
			}
			pGameDataFlag->m_FlagDropTickBlue = FlagState.m_BlueFlagDroppedTick;
			switch(FlagState.m_BlueFlagCarrier)
			{
			case FLAG_ATSTAND:
				pGameDataFlag->m_FlagCarrierBlue = protocol7::FLAG_ATSTAND;
				break;
			case FLAG_TAKEN:
				pGameDataFlag->m_FlagCarrierBlue = protocol7::FLAG_TAKEN;
				break;
			case FLAG_MISSING:
				pGameDataFlag->m_FlagCarrierBlue = protocol7::FLAG_MISSING;
				break;
			default:
				pGameDataFlag->m_FlagCarrierBlue = FlagState.m_BlueFlagCarrier;
			}
		}

		// demo recording
		// MYTODO: fix demo
		if(SnappingClient < 0)
		{
			protocol7::CNetObj_De_GameInfo *pGameInfo = static_cast<protocol7::CNetObj_De_GameInfo *>(Server()->SnapNewItem(-protocol7::NETOBJTYPE_DE_GAMEINFO, 0, sizeof(protocol7::CNetObj_De_GameInfo)));
			if(!pGameInfo)
				return;

			pGameInfo->m_GameFlags = MakeGameFlagSixUp(m_GameFlags);
			pGameInfo->m_ScoreLimit = m_GameInfo.m_ScoreLimit;
			pGameInfo->m_TimeLimit = m_GameInfo.m_TimeLimit;
			pGameInfo->m_MatchNum = m_GameInfo.m_MatchNum;
			pGameInfo->m_MatchCurrent = m_GameInfo.m_MatchCurrent;
		}
	}

	OnSnap(SnappingClient);
}

void IGameController::Tick()
{
	if(m_PauseRequested)
	{
		if(CanPause(m_PauseRequestedTicks))
		{
			SetGameState(IGS_GAME_PAUSED, TIMER_INFINITE);
			m_PauseRequested = false;
		}
		m_PauseRequestedTicks++;
	}

	if(m_ResendVotes)
	{
		// reset sending of vote options
		// only reset for player in room
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			CPlayer *pPlayer = GetPlayerIfInRoom(i);
			if(pPlayer)
			{
				// clear vote options
				CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
				Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, pPlayer->GetCID());
				pPlayer->m_SendVoteIndex = 0;
			}
		}
		m_ResendVotes = false;
	}

	// update voting
	if(m_VoteCloseTime)
	{
		// abort the kick-vote on player-leave
		if(m_VoteEnforce == VOTE_ENFORCE_ABORT)
		{
			SendChatTarget(-1, "Vote aborted");
			EndVote(true);
		}
		else
		{
			int Total = 0, Yes = 0, No = 0;
			if(m_VoteUpdate)
			{
				// count votes
				char aaBuf[MAX_CLIENTS][NETADDR_MAXSTRSIZE] = {{0}}, *pIP = NULL;
				bool SinglePlayer = true;
				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					CPlayer *pPlayer = GetPlayerIfInRoom(i);
					if(pPlayer)
					{
						Server()->GetClientAddr(i, aaBuf[i], NETADDR_MAXSTRSIZE);
						if(!pIP)
							pIP = aaBuf[i];
						else if(SinglePlayer && str_comp(pIP, aaBuf[i]))
							SinglePlayer = false;
					}
				}

				// remember checked players, only the first player with a specific ip will be handled
				bool aVoteChecked[MAX_CLIENTS] = {false};
				int64 Now = Server()->Tick();
				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					CPlayer *pPlayer = GetPlayerIfInRoom(i);
					if(!pPlayer || aVoteChecked[i])
						continue;

					if(pPlayer->GetTeam() == TEAM_SPECTATORS)
						continue;

					if(pPlayer->m_Afk && i != m_VoteCreator)
						continue;

					// can't vote in kick and spec votes in the beginning after joining
					if((IsKickVote() || IsSpecVote()) && Now < pPlayer->m_FirstVoteTick)
						continue;

					// connecting clients with spoofed ips can clog slots without being ingame
					if(((CServer *)Server())->m_aClients[i].m_State != CServer::CClient::STATE_INGAME)
						continue;

					// don't count votes by blacklisted clients
					if(g_Config.m_SvDnsblVote && !m_pServer->DnsblWhite(i) && !SinglePlayer)
						continue;

					int ActVote = pPlayer->m_Vote;
					int ActVotePos = pPlayer->m_VotePos;

					// only allow IPs to vote once
					// check for more players with the same ip (only use the vote of the one who voted first)
					for(int j = i + 1; j < MAX_CLIENTS; j++)
					{
						CPlayer *pOtherPlayer = GetPlayerIfInRoom(j);
						if(!pOtherPlayer || aVoteChecked[j] || str_comp(aaBuf[j], aaBuf[i]) != 0)
							continue;

						// count the latest vote by this ip
						if(ActVotePos < pOtherPlayer->m_VotePos)
						{
							ActVote = pOtherPlayer->m_Vote;
							ActVotePos = pOtherPlayer->m_VotePos;
						}

						aVoteChecked[j] = true;
					}

					Total++;
					if(ActVote > 0)
						Yes++;
					else if(ActVote < 0)
						No++;
				}

				if(g_Config.m_SvVoteMaxTotal && Total > g_Config.m_SvVoteMaxTotal &&
					(IsKickVote() || IsSpecVote()))
					Total = g_Config.m_SvVoteMaxTotal;

				if(Yes > Total / (100.0f / g_Config.m_SvVoteYesPercentage))
					m_VoteEnforce = VOTE_ENFORCE_YES;
				else if(No >= Total - Total / (100.0f / g_Config.m_SvVoteYesPercentage))
					m_VoteEnforce = VOTE_ENFORCE_NO;

				m_VoteWillPass = Yes > (Yes + No) / (100.0f / g_Config.m_SvVoteYesPercentage);
			}

			if(time_get() > m_VoteCloseTime && !g_Config.m_SvVoteMajority)
				m_VoteEnforce = (m_VoteWillPass) ? VOTE_ENFORCE_YES : VOTE_ENFORCE_NO;

			// / Ensure minimum time for vote to end when moderating.
			if(m_VoteEnforce == VOTE_ENFORCE_YES && !(GameServer()->PlayerModerating() &&
									(IsKickVote() || IsSpecVote()) && time_get() < m_VoteCloseTime))
			{
				// silence voted command response
				GameServer()->m_ChatResponseTargetID = -1;
				Server()->SetRconCID(IServer::RCON_CID_VOTE);
				InstanceConsole()->SetFlagMask(CFGFLAG_INSTANCE);
				InstanceConsole()->ExecuteLine(m_aVoteCommand, m_VoteCreator);
				Server()->SetRconCID(IServer::RCON_CID_SERV);
				EndVote(true);
				SendChatTarget(-1, "Vote passed", CGameContext::CHAT_SIX);

				CPlayer *pVotePlayer = GetPlayerIfInRoom(m_VoteCreator);
				if(pVotePlayer && !IsKickVote() && !IsSpecVote())
					pVotePlayer->m_LastVoteCall = 0;
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_YES_ADMIN)
			{
				char aBuf[64];
				// silence voted command response
				GameServer()->m_ChatResponseTargetID = -1;
				str_format(aBuf, sizeof(aBuf), "Vote passed enforced by authorized player");
				InstanceConsole()->SetFlagMask(CFGFLAG_INSTANCE);
				InstanceConsole()->ExecuteLine(m_aVoteCommand, m_VoteCreator);
				SendChatTarget(-1, aBuf, CGameContext::CHAT_SIX);
				EndVote(true);
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_NO_ADMIN)
			{
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "Vote failed enforced by authorized player");
				EndVote(true);
				SendChatTarget(-1, aBuf, CGameContext::CHAT_SIX);
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_NO || (time_get() > m_VoteCloseTime && g_Config.m_SvVoteMajority))
			{
				EndVote(true);
				SendChatTarget(-1, "Vote failed", CGameContext::CHAT_SIX);
			}
			else if(m_VoteUpdate)
			{
				m_VoteUpdate = false;
				SendVoteStatus(-1, Total, Yes, No);
			}
		}
	}

	OnPreTick();

	// handle game states
	if(m_GameState != IGS_GAME_RUNNING)
	{
		if(m_GameStateTimer > 0)
			--m_GameStateTimer;

		if(m_GameStateTimer == 0)
		{
			// timer fires
			switch(m_GameState)
			{
			case IGS_WARMUP_USER:
				// end warmup
				SetGameState(IGS_WARMUP_USER, 0);
				break;
			case IGS_START_COUNTDOWN:
				// unpause the game
				SetGameState(IGS_GAME_RUNNING);
				break;
			case IGS_GAME_PAUSED:
				// end pause
				SetGameState(IGS_GAME_PAUSED, 0);
				break;
			case IGS_END_ROUND:
				StartRound();
				break;
			case IGS_END_MATCH:
				if(m_MatchSwap == 1)
					SwapTeams();
				if(m_MatchSwap == 2)
					ShuffleTeams();
				if(m_ResetOnMatchEnd)
					ResetMatch();
				else
					StartMatch();
				break;
			case IGS_WARMUP_GAME:
			case IGS_GAME_RUNNING:
				// not effected
				break;
			}
		}
		else
		{
			// timer still running
			switch(m_GameState)
			{
			case IGS_WARMUP_USER:
				// check if player ready mode was disabled and it waits that all players are ready -> end warmup
				if(!(m_PlayerReadyMode & 1) && m_GameStateTimer == TIMER_INFINITE)
					SetGameState(IGS_WARMUP_USER, m_Warmup);
				break;
			case IGS_START_COUNTDOWN:
			case IGS_GAME_PAUSED:
			case IGS_END_ROUND:
				// freeze the game
				++m_GameStartTick;
				break;
			case IGS_WARMUP_GAME:
			case IGS_GAME_RUNNING:
			case IGS_END_MATCH:
				// not effected
				break;
			}
		}
	}

	// do team-balancing (skip this in survival, done there when a round starts)
	if(IsTeamplay() && !IsSurvival())
	{
		switch(m_UnbalancedTick)
		{
		case TBALANCE_CHECK:
			CheckTeamBalance();
			break;
		case TBALANCE_OK:
			break;
		default:
			if(Server()->Tick() > m_UnbalancedTick + m_TeambalanceTime * Server()->TickSpeed() * 60)
				DoTeamBalance();
		}
	}

	if((m_GameState == IGS_GAME_RUNNING || m_GameState == IGS_GAME_PAUSED) && !GameWorld()->m_ResetRequested)
	{
		// win check
		if(IsRoundBased())
			DoWincheckRound();
		else
			DoWincheckMatch();
	}

	OnPostTick();
}

// info
void IGameController::CheckGameInfo(bool SendInfo)
{
	bool GameInfoChanged = (m_GameInfo.m_MatchNum != m_Roundlimit) ||
			       (m_GameInfo.m_ScoreLimit != m_Scorelimit) ||
			       (m_GameInfo.m_TimeLimit != m_Timelimit);
	m_GameInfo.m_MatchNum = m_Roundlimit;
	m_GameInfo.m_ScoreLimit = m_Scorelimit;
	m_GameInfo.m_TimeLimit = m_Timelimit;
	if(GameInfoChanged && SendInfo)
		for(int i = 0; i < MAX_CLIENTS; ++i)
			UpdateGameInfo(i);
}

bool IGameController::IsFriendlyFire(int ClientID1, int ClientID2) const
{
	if(ClientID1 == ClientID2)
		return false;

	if(IsTeamplay())
	{
		CPlayer *pPlayer1 = GetPlayerIfInRoom(ClientID1);
		CPlayer *pPlayer2 = GetPlayerIfInRoom(ClientID2);
		if(!pPlayer1 || !pPlayer2)
			return false;

		if(!m_Teamdamage && pPlayer1->GetTeam() == pPlayer2->GetTeam())
			return true;
	}

	return false;
}

bool IGameController::IsFriendlyTeamFire(int Team1, int Team2) const
{
	return IsTeamplay() && !m_Teamdamage && Team1 == Team2;
}

bool IGameController::IsPlayerReadyMode() const
{
	return m_GameStateTimer == TIMER_INFINITE && (((m_PlayerReadyMode & 1) && m_GameState == IGS_WARMUP_USER) || ((m_PlayerReadyMode & 2) && m_GameState == IGS_GAME_PAUSED));
}

bool IGameController::IsTeamChangeAllowed() const
{
	return !GameWorld()->m_Paused || (m_GameState == IGS_START_COUNTDOWN && m_GameStartTick == Server()->Tick());
}

void IGameController::UpdateGameInfo(int ClientID)
{
	if(!GetPlayerIfInRoom(ClientID) || !Server()->ClientIngame(ClientID))
		return;

	if(Server()->IsSixup(ClientID))
	{
		protocol7::CNetMsg_Sv_GameInfo GameInfoMsg;
		GameInfoMsg.m_GameFlags = MakeGameFlagSixUp(m_GameFlags);
		GameInfoMsg.m_ScoreLimit = m_GameInfo.m_ScoreLimit;
		GameInfoMsg.m_TimeLimit = m_GameInfo.m_TimeLimit;
		GameInfoMsg.m_MatchNum = m_GameInfo.m_MatchNum;
		GameInfoMsg.m_MatchCurrent = m_GameInfo.m_MatchCurrent;

		Server()->SendPackMsg(&GameInfoMsg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
	}
}

void IGameController::SendGameMsg(int GameMsgID, int ClientID, int *i1, int *i2, int *i3)
{
	char aBuf[256] = {0};
	int Start = ClientID;
	int Limit = ClientID + 1;
	if(ClientID < 0)
	{
		Start = 0;
		Limit = MAX_CLIENTS;
	}

	for(int CID = Start; CID < Limit; ++CID)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(CID);
		if(!pPlayer)
			continue;

		if(Server()->IsSixup(CID))
		{
			CMsgPacker Msg(protocol7::NETMSGTYPE_SV_GAMEMSG, false, true);
			Msg.AddInt(GameMsgID);
			if(i1)
				Msg.AddInt(*i1);
			if(i2)
				Msg.AddInt(*i2);
			if(i3)
				Msg.AddInt(*i3);
			Server()->SendMsg(&Msg, MSGFLAG_VITAL, CID);
		}
		else
		{
			switch(GameMsgID)
			{
			case GAMEMSG_TEAM_SWAP:
				GameServer()->SendChatLocalized(CID, "Teams were swapped");
				break;
			case GAMEMSG_SPEC_INVALIDID:
				GameServer()->SendChatLocalized(CID, "You can't spectate this player");
				break;
			case GAMEMSG_TEAM_SHUFFLE:
				GameServer()->SendChatLocalized(CID, "Teams were shuffled");
				break;
			case GAMEMSG_TEAM_BALANCE:
				GameServer()->SendChatLocalized(CID, "Teams have been balanced");
				break;
			case GAMEMSG_CTF_DROP:
				FakeGameMsgSound(CID, SOUND_CTF_DROP);
				break;
			case GAMEMSG_CTF_RETURN:
				FakeGameMsgSound(CID, SOUND_CTF_RETURN);
				break;
			case GAMEMSG_TEAM_ALL:
			{
				if(!i1)
					break;

				GameServer()->SendChatLocalized(CID, "All players were moved to the %s", GetTeamName(*i1));
				break;
			}
			case GAMEMSG_TEAM_BALANCE_VICTIM:
			{
				if(!i1)
					break;

				if(!aBuf[0])
					str_format(aBuf, sizeof(aBuf), "You were moved to the %s due to team balancing", GetTeamName(*i1));
				GameServer()->SendBroadcast(aBuf, CID);
				break;
			}
			case GAMEMSG_CTF_GRAB:
				if(!i1)
					break;

				if(pPlayer->GetTeam() == *i1)
					FakeGameMsgSound(CID, SOUND_CTF_GRAB_PL);
				else
					FakeGameMsgSound(CID, SOUND_CTF_GRAB_EN);
				break;
			case GAMEMSG_CTF_CAPTURE:
			{
				if(!i1 || !i2 || !i3)
					break;

				if(!aBuf[0])
				{
					const char *pFlagName = *i1 ? GameServer()->LocalizeFor(CID, "blue", "FLAG") : GameServer()->LocalizeFor(CID, "red", "FLAG");
					float CaptureTime = *i3 / (float)Server()->TickSpeed();
					if(CaptureTime <= 60)
						GameServer()->SendChatLocalized(CID, "The %s flag was captured by '%s' (%d.%s%d seconds)", pFlagName, Server()->ClientName(*i2), (int)CaptureTime % 60, ((int)(CaptureTime * 100) % 100) < 10 ? "0" : "", (int)(CaptureTime * 100) % 100);
					else
					{
						GameServer()->SendChatLocalized(CID, "The %s flag was captured by '%s'", pFlagName, Server()->ClientName(*i2));
					}
				}

				FakeGameMsgSound(CID, SOUND_CTF_CAPTURE);
				break;
			}
			case GAMEMSG_GAME_PAUSED:
				if(!i1)
					break;

				GameServer()->SendChatLocalized(CID, "'%s' initiated a pause", Server()->ClientName(*i1));
				break;
			}
		}
	}
}

// spawn
bool IGameController::CanSpawn(int Team, vec2 *pOutPos) const
{
	// spectators can't spawn
	if(Team == TEAM_SPECTATORS || GameWorld()->m_Paused || GameWorld()->m_ResetRequested)
		return false;

	CSpawnEval Eval;
	Eval.m_RandomSpawn = IsSpawnRandom();
	Eval.m_SpawningTeam = Team;

	if(IsTeamplay())
	{
		// first try own team spawn, then normal spawn and then enemy
		EvaluateSpawnType(&Eval, 1 + (Team & 1));
		if(!Eval.m_Got)
		{
			EvaluateSpawnType(&Eval, 0);
			if(!Eval.m_Got)
				EvaluateSpawnType(&Eval, 1 + ((Team + 1) & 1));
		}
	}
	else
	{
		EvaluateSpawnType(&Eval, 0);
		EvaluateSpawnType(&Eval, 1);
		EvaluateSpawnType(&Eval, 2);
	}

	*pOutPos = Eval.m_Pos;
	return Eval.m_Got;
}

float IGameController::EvaluateSpawnPos(CSpawnEval *pEval, vec2 Pos) const
{
	float Score = 0.0f;
	CCharacter *pC = static_cast<CCharacter *>(GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER));
	for(; pC; pC = (CCharacter *)pC->TypeNext())
	{
		float Scoremod = SpawnPosDangerScore(Pos, pEval->m_SpawningTeam, pC);

		float d = distance(Pos, pC->m_Pos);
		Score += Scoremod * (d == 0 ? 1000000000.0f : 1.0f / d);
	}

	return Score;
}

void IGameController::EvaluateSpawnType(CSpawnEval *pEval, int Type) const
{
	// get spawn point
	for(int i = 0; i < m_aNumSpawnPoints[Type]; i++)
	{
		// check if the position is occupado
		CCharacter *aEnts[MAX_CLIENTS];
		int Num = GameWorld()->FindEntities(m_aaSpawnPoints[Type][i], 64, (CEntity **)aEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		vec2 Positions[5] = {vec2(0.0f, 0.0f), vec2(-32.0f, 0.0f), vec2(0.0f, -32.0f), vec2(32.0f, 0.0f), vec2(0.0f, 32.0f)}; // start, left, up, right, down
		int Result = -1;
		for(int Index = 0; Index < 5 && Result == -1; ++Index)
		{
			Result = Index;
			if(!GameWorld()->m_Core.m_Tuning.m_PlayerCollision)
				break;
			for(int c = 0; c < Num; ++c)
			{
				if(GameServer()->Collision()->CheckPoint(m_aaSpawnPoints[Type][i] + Positions[Index]) ||
					distance(aEnts[c]->m_Pos, m_aaSpawnPoints[Type][i] + Positions[Index]) <= aEnts[c]->GetProximityRadius())
				{
					Result = -1;
					break;
				}
			}
		}
		if(Result == -1)
			continue; // try next spawn point

		vec2 P = m_aaSpawnPoints[Type][i] + Positions[Result];
		float S = pEval->m_RandomSpawn ? Result + frandom() : EvaluateSpawnPos(pEval, P);
		if(!pEval->m_Got || pEval->m_Score > S)
		{
			pEval->m_Got = true;
			pEval->m_Score = S;
			pEval->m_Pos = P;
		}
	}
}

float IGameController::SpawnPosDangerScore(vec2 Pos, int SpawningTeam, class CCharacter *pChar) const
{
	// team mates are not as dangerous as enemies
	if(IsTeamplay() && pChar->GetPlayer()->GetTeam() == SpawningTeam)
		return 0.5f;
	else
		return 1.0f;
}

bool IGameController::CanDeadPlayerFollow(const CPlayer *pSpectator, const CPlayer *pTarget)
{
	return pSpectator->GetTeam() == pTarget->GetTeam();
}

bool IGameController::GetStartRespawnState() const
{
	if(IsSurvival())
	{
		// players can always respawn during warmup
		if(m_GameState == IGS_WARMUP_GAME || m_GameState == IGS_WARMUP_USER || (m_GameState == IGS_START_COUNTDOWN && m_GameStartTick == Server()->Tick()))
			return false;
		else
			return true;
	}
	else
		return false;
}

// team
bool IGameController::CanChangeTeam(CPlayer *pPlayer, int JoinTeam) const
{
	if(!IsTeamplay() || JoinTeam == TEAM_SPECTATORS || !m_TeambalanceTime)
		return true;

	// simulate what would happen if the player changes team
	int aPlayerCount[2] = {m_aTeamSize[TEAM_RED], m_aTeamSize[TEAM_BLUE]};
	aPlayerCount[JoinTeam]++;
	if(pPlayer->GetTeam() != TEAM_SPECTATORS)
		aPlayerCount[JoinTeam ^ 1]--;

	// check if the player-difference decreases or is smaller than 2
	return aPlayerCount[JoinTeam] - aPlayerCount[JoinTeam ^ 1] < 2;
}

bool IGameController::CanJoinTeam(int Team, int ClientID, bool SendReason) const
{
	if(Team == TEAM_SPECTATORS)
		return true;

	if(GameWorld()->Team() == 0 && Config()->m_SvRoom == 2)
	{
		if(SendReason)
		{
			if(Config()->m_SvRoomVotes)
				GameServer()->SendBroadcast("You need to join a room to play. Check vote menu to join rooms.", ClientID);
			else
				GameServer()->SendBroadcast("You need to join a room to play.", ClientID);
		}

		return false;
	}

	// check if there're enough player slots left
	CPlayer *pPlayer = GetPlayerIfInRoom(ClientID);
	int TeamMod = pPlayer && pPlayer->GetTeam() != TEAM_SPECTATORS ? -1 : 0;

	bool CanJoin = TeamMod + m_aTeamSize[TEAM_RED] + m_aTeamSize[TEAM_BLUE] < m_PlayerSlots;

	if(!CanJoin && SendReason)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Only %d active players are allowed in this room", m_PlayerSlots);
		GameServer()->SendBroadcast(aBuf, ClientID);
	}
	return CanJoin;
}

int IGameController::ClampTeam(int Team) const
{
	if(Team < TEAM_RED)
		return TEAM_SPECTATORS;
	if(IsTeamplay())
		return Team & 1;
	return TEAM_RED;
}

void IGameController::DoTeamChange(CPlayer *pPlayer, int Team, bool DoChatMsg)
{
	Team = ClampTeam(Team);
	int OldTeam = pPlayer->GetTeam();

	if(Team == OldTeam)
		return;

	char aBuf[512];
	int ClientID = pPlayer->GetCID();

	pPlayer->SetTeam(Team);

	if(DoChatMsg)
	{
		str_format(aBuf, sizeof(aBuf), "'%s' joined the %s", Server()->ClientName(ClientID), GetTeamName(Team));
		SendChatTarget(-1, aBuf);
	}

	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' team=%d->%d", ClientID, Server()->ClientName(ClientID), OldTeam, Team);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	// update effected game settings
	if(OldTeam != TEAM_SPECTATORS)
	{
		--m_aTeamSize[OldTeam];
		dbg_msg("game", "team size decreased to %d, team='%d', ddrteam='%d'", m_aTeamSize[OldTeam], OldTeam, GameWorld()->Team());
		m_UnbalancedTick = TBALANCE_CHECK;
	}
	if(Team != TEAM_SPECTATORS)
	{
		++m_aTeamSize[Team];
		dbg_msg("game", "team size increased to %d, team='%d', ddrteam='%d'", m_aTeamSize[Team], Team, GameWorld()->Team());
		m_UnbalancedTick = TBALANCE_CHECK;
		pPlayer->m_IsReadyToPlay = !IsPlayerReadyMode();
		if(IsSurvival())
			pPlayer->m_RespawnDisabled = GetStartRespawnState();
	}

	CheckReadyStates();
	OnPlayerChangeTeam(pPlayer, OldTeam, Team);
	TryStartWarmup(true);

	// reset inactivity counter when joining the game
	if(OldTeam == TEAM_SPECTATORS)
		pPlayer->m_InactivityTickCounter = 0;

	m_VoteUpdate = true;
}

int IGameController::GetStartTeam()
{
	if(Config()->m_SvTournamentMode || (GameWorld()->Team() == 0 && Config()->m_SvRoom == 2))
		return TEAM_SPECTATORS;

	// determine new team
	int Team = TEAM_RED;
	if(IsTeamplay())
	{
		// this will force the auto balancer to work overtime aswell
		Team = m_aTeamSize[TEAM_RED] > m_aTeamSize[TEAM_BLUE] ? TEAM_BLUE : TEAM_RED;
	}

	// check if there're enough player slots left
	if(m_aTeamSize[TEAM_RED] + m_aTeamSize[TEAM_BLUE] < m_PlayerSlots)
		return Team;

	return TEAM_SPECTATORS;
}

const char *IGameController::GetTeamName(int Team)
{
	if(IsTeamplay())
	{
		if(Team == 0)
			return "red team";
		if(Team == 1)
			return "blue team";
		return "spectators";
	}
	else
	{
		if(Team == 0)
			return "game";
		return "spectators";
	}
}

// ddrace
int IGameController::GetPlayerTeam(int ClientID) const
{
	return GameServer()->GetPlayerDDRTeam(ClientID);
}

CPlayer *IGameController::GetPlayerIfInRoom(int ClientID) const
{
	if(GameServer()->PlayerExists(ClientID) && GameServer()->GetPlayerDDRTeam(ClientID) == GameWorld()->Team())
		return GameServer()->m_apPlayers[ClientID];
	return nullptr;
}

void IGameController::InitController(class CGameContext *pGameServer, class CGameWorld *pWorld)
{
	m_Started = false;
	m_pGameServer = pGameServer;
	m_pConfig = m_pGameServer->Config();
	m_pServer = m_pGameServer->Server();
	m_pWorld = pWorld;
	m_GameStartTick = m_pServer->Tick();
	m_pInstanceConsole->InitNoConfig(m_pGameServer->Storage());
	m_PauseRequested = false;

	// game
	m_aTeamscore[TEAM_RED] = 0;
	m_aTeamscore[TEAM_BLUE] = 0;

	// spawn
	m_aNumSpawnPoints[0] = 0;
	m_aNumSpawnPoints[1] = 0;
	m_aNumSpawnPoints[2] = 0;
	OnInit();
}

void IGameController::CallVote(int ClientID, const char *pDesc, const char *pCmd, const char *pReason, const char *pChatmsg, const char *pSixupDesc)
{
	// check if a vote is already running
	if(m_VoteCloseTime)
		return;

	int64 Now = Server()->Tick();
	CPlayer *pPlayer = GetPlayerIfInRoom(ClientID);

	if(!pPlayer)
		return;

	SendChatTarget(-1, pChatmsg, CGameContext::CHAT_SIX);
	if(!pSixupDesc)
		pSixupDesc = pDesc;

	m_VoteCreator = ClientID;
	StartVote(pDesc, pCmd, pReason, pSixupDesc);
	pPlayer->m_Vote = 1;
	pPlayer->m_VotePos = m_VotePos = 1;
	pPlayer->m_LastVoteCall = Now;
}

void IGameController::StartVote(const char *pDesc, const char *pCommand, const char *pReason, const char *pSixupDesc)
{
	// reset votes
	m_VoteEnforce = VOTE_ENFORCE_UNKNOWN;
	m_VoteEnforcer = -1;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(i);
		if(pPlayer)
		{
			pPlayer->m_Vote = 0;
			pPlayer->m_VotePos = 0;
		}
	}

	// start vote
	m_VoteCloseTime = time_get() + time_freq() * g_Config.m_SvVoteTime;
	str_copy(m_aVoteDescription, pDesc, sizeof(m_aVoteDescription));
	str_copy(m_aSixupVoteDescription, pSixupDesc, sizeof(m_aSixupVoteDescription));
	str_copy(m_aVoteCommand, pCommand, sizeof(m_aVoteCommand));
	str_copy(m_aVoteReason, pReason, sizeof(m_aVoteReason));
	SendVoteSet(-1);
	m_VoteUpdate = true;
}

void IGameController::EndVote(bool SendInfo)
{
	m_VoteCloseTime = 0;
	if(SendInfo)
		SendVoteSet(-1);
	m_VoteVictim = -1;
}

bool IGameController::IsVoting()
{
	return m_VoteCloseTime > 0;
}

void IGameController::ForceVote(int EnforcerID, bool Success)
{
	// check if there is a vote running
	if(!m_VoteCloseTime)
		return;

	m_VoteEnforce = Success ? VOTE_ENFORCE_YES_ADMIN : VOTE_ENFORCE_NO_ADMIN;
	m_VoteEnforcer = EnforcerID;

	char aBuf[256];
	const char *pOption = Success ? "yes" : "no";
	str_format(aBuf, sizeof(aBuf), "authorized player forced vote %s", pOption);
	SendChatTarget(-1, aBuf);
	str_format(aBuf, sizeof(aBuf), "forcing vote %s", pOption);
	InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

struct CVoteOptionServer *IGameController::GetVoteOption(int Index)
{
	CVoteOptionServer *pCurrent;
	for(pCurrent = m_pVoteOptionFirst;
		Index > 0 && pCurrent;
		Index--, pCurrent = pCurrent->m_pNext)
		;

	if(Index > 0)
		return 0;
	return pCurrent;
}

void IGameController::SendVoteSet(int ClientID) const
{
	if(ClientID >= 0 && !GetPlayerIfInRoom(ClientID))
		return;

	::CNetMsg_Sv_VoteSet Msg6;
	protocol7::CNetMsg_Sv_VoteSet Msg7;

	Msg7.m_ClientID = m_VoteCreator;
	if(m_VoteCloseTime)
	{
		Msg6.m_Timeout = Msg7.m_Timeout = (m_VoteCloseTime - time_get()) / time_freq();
		Msg6.m_pDescription = m_aVoteDescription;
		Msg7.m_pDescription = m_aSixupVoteDescription;
		Msg6.m_pReason = Msg7.m_pReason = m_aVoteReason;

		int &Type = (Msg7.m_Type = protocol7::VOTE_UNKNOWN);
		if(IsKickVote())
			Type = protocol7::VOTE_START_KICK;
		else if(IsSpecVote())
			Type = protocol7::VOTE_START_SPEC;
		else if(IsOptionVote())
			Type = protocol7::VOTE_START_OP;
	}
	else
	{
		Msg6.m_Timeout = Msg7.m_Timeout = 0;
		Msg6.m_pDescription = Msg7.m_pDescription = "";
		Msg6.m_pReason = Msg7.m_pReason = "";

		int &Type = (Msg7.m_Type = protocol7::VOTE_UNKNOWN);
		if(m_VoteEnforce == VOTE_ENFORCE_NO || m_VoteEnforce == VOTE_ENFORCE_NO_ADMIN)
			Type = protocol7::VOTE_END_FAIL;
		else if(m_VoteEnforce == VOTE_ENFORCE_YES || m_VoteEnforce == VOTE_ENFORCE_YES_ADMIN)
			Type = protocol7::VOTE_END_PASS;
		else if(m_VoteEnforce == VOTE_ENFORCE_ABORT)
			Type = protocol7::VOTE_END_ABORT;

		if(m_VoteEnforce == VOTE_ENFORCE_NO_ADMIN || m_VoteEnforce == VOTE_ENFORCE_YES_ADMIN)
			Msg7.m_ClientID = -1;
	}

	if(ClientID == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CPlayer *pPlayer = GetPlayerIfInRoom(i);
			if(pPlayer)
			{
				if(!Server()->IsSixup(i))
					Server()->SendPackMsg(&Msg6, MSGFLAG_VITAL, i);
				else
					Server()->SendPackMsg(&Msg7, MSGFLAG_VITAL, i);
			}
		}
	}
	else
	{
		if(!Server()->IsSixup(ClientID))
			Server()->SendPackMsg(&Msg6, MSGFLAG_VITAL, ClientID);
		else
			Server()->SendPackMsg(&Msg7, MSGFLAG_VITAL, ClientID);
	}
}

void IGameController::SendVoteStatus(int ClientID, int Total, int Yes, int No) const
{
	if(ClientID == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
			if(Server()->ClientIngame(i))
				SendVoteStatus(i, Total, Yes, No);
		return;
	}

	CPlayer *pPlayer = GetPlayerIfInRoom(ClientID);
	if(!pPlayer)
		return;

	if(Total > VANILLA_MAX_CLIENTS && pPlayer && pPlayer->GetClientVersion() <= VERSION_DDRACE)
	{
		Yes = float(Yes) * VANILLA_MAX_CLIENTS / float(Total);
		No = float(No) * VANILLA_MAX_CLIENTS / float(Total);
		Total = VANILLA_MAX_CLIENTS;
	}

	CNetMsg_Sv_VoteStatus Msg = {0};
	Msg.m_Total = Total;
	Msg.m_Yes = Yes;
	Msg.m_No = No;
	Msg.m_Pass = Total - (Yes + No);

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void IGameController::SendChatTarget(int To, const char *pText, int Flags) const
{
	int Start = To;
	int Limit = To + 1;
	if(To < 0)
	{
		Start = 0;
		Limit = MAX_CLIENTS;
	}

	for(int i = Start; i < Limit; i++)
		if(GetPlayerIfInRoom(i))
			GameServer()->SendChatTarget(i, pText, Flags);
}

void IGameController::SendBroadcast(const char *pText, int ClientID, bool IsImportant) const
{
	int Start = ClientID;
	int Limit = ClientID + 1;
	if(ClientID < 0)
	{
		Start = 0;
		Limit = MAX_CLIENTS;
	}

	for(int i = Start; i < Limit; i++)
		if(GetPlayerIfInRoom(i))
			GameServer()->SendBroadcast(pText, i, IsImportant);
}

void IGameController::SendKillMsg(int Killer, int Victim, int Weapon, int ModeSpecial) const
{
	// send the kill message
	CNetMsg_Sv_KillMsg Msg;
	Msg.m_Killer = Killer;
	Msg.m_Victim = Victim;
	Msg.m_Weapon = Weapon;
	Msg.m_ModeSpecial = ModeSpecial;

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(GetPlayerIfInRoom(i))
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
	}
}

void IGameController::InstanceConsolePrint(const char *pStr, void *pUser)
{
	IGameController *pController = (IGameController *)pUser;
	const char *pLineOrig = pStr;

	if(pStr[0] == '[')
	{
		// Remove time and category: [20:39:00][Console]
		pStr = str_find(pStr, "]: ");
		if(pStr)
			pStr += 3;
		else
			pStr = pLineOrig;
	}

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s", pStr);

	char aBufInstance[32];
	str_format(aBufInstance, sizeof(aBufInstance), "instance %d", pController->GameWorld()->Team());
	pController->GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, aBufInstance, aBuf);
}

void IGameController::IntVariableCommand(IConsole::IResult *pResult, void *pUserData)
{
	CIntVariableData *pData = (CIntVariableData *)pUserData;

	if(pResult->NumArguments())
	{
		int Val = pResult->GetInteger(0);

		// do clamping
		if(pData->m_Min != pData->m_Max)
		{
			if(Val < pData->m_Min)
				Val = pData->m_Min;
			if(pData->m_Max != 0 && Val > pData->m_Max)
				Val = pData->m_Max;
		}

		*(pData->m_pVariable) = Val;
		if(pResult->m_ClientID != IConsole::CLIENT_ID_GAME)
			pData->m_OldValue = Val;
	}
	else
	{
		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), "Value: %d", *(pData->m_pVariable));
		pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}
}

void IGameController::ColVariableCommand(IConsole::IResult *pResult, void *pUserData)
{
	CColVariableData *pData = (CColVariableData *)pUserData;

	if(pResult->NumArguments())
	{
		ColorHSLA Col = pResult->GetColor(0, pData->m_Light);
		int Val = Col.Pack(pData->m_Light ? 0.5f : 0.0f, pData->m_Alpha);

		*(pData->m_pVariable) = Val;
		if(pResult->m_ClientID != IConsole::CLIENT_ID_GAME)
			pData->m_OldValue = Val;
	}
	else
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "Value: %u", *(pData->m_pVariable));
		pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);

		ColorHSLA Hsla(*(pData->m_pVariable), true);
		if(pData->m_Light)
			Hsla = Hsla.UnclampLighting();
		str_format(aBuf, sizeof(aBuf), "H: %d°, S: %d%%, L: %d%%", round_truncate(Hsla.h * 360), round_truncate(Hsla.s * 100), round_truncate(Hsla.l * 100));
		pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);

		ColorRGBA Rgba = color_cast<ColorRGBA>(Hsla);
		str_format(aBuf, sizeof(aBuf), "R: %d, G: %d, B: %d, #%06X", round_truncate(Rgba.r * 255), round_truncate(Rgba.g * 255), round_truncate(Rgba.b * 255), Rgba.Pack(false));
		pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);

		if(pData->m_Alpha)
		{
			str_format(aBuf, sizeof(aBuf), "A: %d%%", round_truncate(Hsla.a * 100));
			pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
		}
	}
}

void IGameController::StrVariableCommand(IConsole::IResult *pResult, void *pUserData)
{
	CStrVariableData *pData = (CStrVariableData *)pUserData;

	if(pResult->NumArguments())
	{
		const char *pString = pResult->GetString(0);
		if(!str_utf8_check(pString))
		{
			char aTemp[4];
			int Length = 0;
			while(*pString)
			{
				int Size = str_utf8_encode(aTemp, static_cast<unsigned char>(*pString++));
				if(Length + Size < pData->m_MaxSize)
				{
					mem_copy(pData->m_pStr + Length, aTemp, Size);
					Length += Size;
				}
				else
					break;
			}
			pData->m_pStr[Length] = 0;
		}
		else
			str_copy(pData->m_pStr, pString, pData->m_MaxSize);

		if(pData->m_pOldValue && pResult->m_ClientID != IConsole::CLIENT_ID_GAME)
			str_copy(pData->m_pOldValue, pData->m_pStr, pData->m_MaxSize);
	}
	else
	{
		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "Value: %s", pData->m_pStr);
		pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}
}