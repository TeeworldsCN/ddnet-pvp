/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/teams.h>

#include <game/generated/protocol.h>

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
#include <game/layers.h>

#include <engine/server/server.h>

static void ConchainGameInfoUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() >= 1)
	{
		IGameController *pThis = static_cast<IGameController *>(pUserData);
		pThis->CheckGameInfo();
	}
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

static void ConSetTeamAll(IConsole::IResult *pResult, void *pUserData)
{
	IGameController *pSelf = (IGameController *)pUserData;
	int Team = clamp(pResult->GetInteger(0), -1, 1);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "All players were moved to the %s", pSelf->GetTeamName(Team));
	pSelf->SendChatTarget(-1, aBuf);

	for(auto &pPlayer : pSelf->GameServer()->m_apPlayers)
		if(pPlayer && pSelf->IsPlayerInRoom(pPlayer->GetCID()))
			pSelf->DoTeamChange(pPlayer, Team, false);
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
		if(pSelf->GameServer()->m_pTeams->SetForcePlayerTeam(VictimID, 0, CGameTeams::TEAM_REASON_FORCE, nullptr))
			pSelf->GameServer()->m_pTeams->SetClientInvited(pSelf->GameWorld()->Team(), VictimID, false);
		else
			pSelf->GameServer()->Console()->ExecuteLine(pResult->GetString(1));
	}
}

static void ConVoteCommand(IConsole::IResult *pResult, void *pUserData)
{
	IGameController *pSelf = (IGameController *)pUserData;
	pSelf->GameServer()->Console()->ExecuteLine(pResult->GetString(0));
}

IGameController::IGameController()
{
	m_pGameServer = nullptr;
	m_pConfig = nullptr;
	m_pServer = nullptr;
	m_pWorld = nullptr;
	m_pInstanceConsole = new CConsole(CFGFLAG_INSTANCE);
	m_ChatResponseTargetID = -1;

	// balancing
	m_aTeamSize[TEAM_RED] = 0;
	m_aTeamSize[TEAM_BLUE] = 0;
	m_UnbalancedTick = TBALANCE_OK;

	// game
	m_GameState = IGS_GAME_RUNNING;
	m_GameStateTimer = TIMER_INFINITE;
	m_GameStartTick = 0;
	m_MatchCount = 0;
	m_RoundCount = 0;
	m_SuddenDeath = 0;
	m_aTeamscore[TEAM_RED] = 0;
	m_aTeamscore[TEAM_BLUE] = 0;

	// info
	m_GameFlags = 0;
	m_pGameType = "unknown";
	m_GameInfo.m_MatchCurrent = m_MatchCount + 1;
	m_GameInfo.m_MatchNum = 0;
	m_GameInfo.m_ScoreLimit = 0;
	m_GameInfo.m_TimeLimit = 0;

	// spawn
	m_aNumSpawnPoints[0] = 0;
	m_aNumSpawnPoints[1] = 0;
	m_aNumSpawnPoints[2] = 0;

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

	// fake client broadcast
	mem_zero(m_aFakeClientBroadcast, sizeof(m_aFakeClientBroadcast));

	m_pInstanceConsole->RegisterPrintCallback(IConsole::OUTPUT_LEVEL_STANDARD, InstanceConsolePrint, this);

	INSTANCE_CONFIG_INT(&m_Warmup, "warmup", 10, 0, 1000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Number of seconds to do warmup before round starts");
	INSTANCE_CONFIG_INT(&m_Countdown, "countdown", 0, -1000, 1000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Number of seconds to freeze the game in a countdown before match starts, (-: for survival, +: for all")
	INSTANCE_CONFIG_INT(&m_Teamdamage, "teamdamage", 0, 0, 2, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Team damage (1 = half damage, 2 = full damage)")
	INSTANCE_CONFIG_INT(&m_RoundSwap, "round_swap", 1, 0, 1, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Swap teams between rounds")
	INSTANCE_CONFIG_INT(&m_MatchSwap, "match_swap", 1, 0, 1, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Swap teams between matches")
	INSTANCE_CONFIG_INT(&m_Powerups, "powerups", 1, 0, 1, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Allow powerups like ninja")
	INSTANCE_CONFIG_INT(&m_Scorelimit, "scorelimit", 20, 0, 1000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Score limit (0 disables)")
	INSTANCE_CONFIG_INT(&m_Timelimit, "timelimit", 0, 0, 1000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Time limit in minutes (0 disables)")
	INSTANCE_CONFIG_INT(&m_Roundlimit, "roundlimit", 0, 0, 1000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Round limit for game with rounds (0 disables)")
	INSTANCE_CONFIG_INT(&m_TeambalanceTime, "teambalance_time", 1, 0, 1000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "How many minutes to wait before autobalancing teams")

	m_pInstanceConsole->Chain("scorelimit", ConchainGameInfoUpdate, this);
	m_pInstanceConsole->Chain("timelimit", ConchainGameInfoUpdate, this);
	m_pInstanceConsole->Chain("roundlimit", ConchainGameInfoUpdate, this);

	m_pInstanceConsole->Chain("cmdlist", ConchainReplyOnly, this);

	m_pInstanceConsole->Register("pause", "?i[seconds]", CFGFLAG_CHAT | CFGFLAG_INSTANCE, ConPause, this, "Pause/unpause game");
	m_pInstanceConsole->Register("restart", "?i[seconds]", CFGFLAG_CHAT | CFGFLAG_INSTANCE, ConRestart, this, "Restart in x seconds (0 = abort)");
	m_pInstanceConsole->Register("set_team_all", "i[team-id]", CFGFLAG_INSTANCE, ConSetTeamAll, this, "Set team of all players to team");
	m_pInstanceConsole->Register("help", "?r[command]", CFGFLAG_CHAT | CFGFLAG_INSTANCE | CFGFLAG_NO_CONSENT, ConHelp, this, "Shows help to command, general help if left blank");

	// vote helper
	m_pInstanceConsole->Register("vote_kick", "i[id] r[real-command]", CFGFLAG_INSTANCE, ConKick, this, "Helper command for vote kicking, it is not recommended to call this directly");
	m_pInstanceConsole->Register("vote_command", "r[real-command]", CFGFLAG_INSTANCE, ConVoteCommand, this, "Helper command for vote commands, it is not recommended to call this directly");
}

IGameController::~IGameController()
{
	delete m_pInstanceConsole;
	for(auto pInt : m_IntConfigStore)
		delete pInt;
}

void IGameController::DoActivityCheck()
{
	if(Config()->m_SvInactiveKickTime == 0)
		return;

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
#ifdef CONF_DEBUG
		if(g_Config.m_DbgDummies)
		{
			if(i >= MAX_CLIENTS - g_Config.m_DbgDummies)
				break;
		}
#endif
		if(GameServer()->IsClientPlayer(i) &&
			Server()->GetAuthedState(i) == AUTHED_NO && (GameServer()->m_apPlayers[i]->m_InactivityTickCounter > Config()->m_SvInactiveKickTime * Server()->TickSpeed() * 60))
		{
			if(GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS)
			{
				if(Config()->m_SvInactiveKickSpec)
					Server()->Kick(i, "Kicked for inactivity");
			}
			else
			{
				switch(Config()->m_SvInactiveKick)
				{
				case 0:
				{
					// move player to spectator
					DoTeamChange(GameServer()->m_apPlayers[i], TEAM_SPECTATORS);
				}
				break;
				case 1:
				{
					// move player to spectator if the reserved slots aren't filled yet, kick him otherwise
					int Spectators = 0;
					for(auto &pPlayer : GameServer()->m_apPlayers)
						if(pPlayer && pPlayer->GetTeam() == TEAM_SPECTATORS)
							++Spectators;
					if(Spectators >= g_Config.m_SvSpectatorSlots)
						Server()->Kick(i, "Kicked for inactivity");
					else
						DoTeamChange(GameServer()->m_apPlayers[i], TEAM_SPECTATORS);
				}
				break;
				case 2:
				{
					// kick the player
					Server()->Kick(i, "Kicked for inactivity");
				}
				}
			}
		}
	}
}

// TODO: maybe move these out too?
bool IGameController::GetPlayersReadyState(int WithoutID)
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(i == WithoutID)
			continue; // skip
		if(GameServer()->IsClientReadyToPlay(i))
			return false;
	}

	return true;
}

void IGameController::SetPlayersReadyState(bool ReadyState)
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(GameServer()->IsClientPlayer(i) && (ReadyState || !GameServer()->m_apPlayers[i]->m_DeadSpecMode))
			GameServer()->m_apPlayers[i]->m_IsReadyToPlay = ReadyState;
	}
}

// to be called when a player changes state, spectates or disconnects
void IGameController::CheckReadyStates(int WithoutID)
{
	if(Config()->m_SvPlayerReadyMode)
	{
		switch(m_GameState)
		{
		case IGS_WARMUP_USER:
			// all players are ready -> end warmup
			if(GetPlayersReadyState(WithoutID))
				SetGameState(IGS_WARMUP_USER, 0);
			break;
		case IGS_GAME_PAUSED:
			// all players are ready -> unpause the game
			if(GetPlayersReadyState(WithoutID))
				SetGameState(IGS_GAME_PAUSED, 0);
			break;
		case IGS_GAME_RUNNING:
		case IGS_WARMUP_GAME:
		case IGS_START_COUNTDOWN:
		case IGS_END_MATCH:
		case IGS_END_ROUND:
			// not affected
			break;
		}
	}
}

// balancing
bool IGameController::CanBeMovedOnBalance(int ClientID) const
{
	return true;
}

void IGameController::CheckTeamBalance()
{
	if(!IsTeamplay() || !m_TeambalanceTime)
	{
		m_UnbalancedTick = TBALANCE_OK;
		return;
	}

	// check if teams are unbalanced
	char aBuf[256];
	if(absolute(m_aTeamSize[TEAM_RED] - m_aTeamSize[TEAM_BLUE]) >= 2)
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
	if(!IsTeamplay() || !m_TeambalanceTime || absolute(m_aTeamSize[TEAM_RED] - m_aTeamSize[TEAM_BLUE]) < 2)
		return;

	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", "Balancing teams");

	float aTeamScore[2] = {0};
	float aPlayerScore[MAX_CLIENTS] = {0.0f};

	// gather stats
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(IsPlayerInRoom(i) && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
		{
			aPlayerScore[i] = GameServer()->m_apPlayers[i]->m_Score * Server()->TickSpeed() * 60.0f /
					  (Server()->Tick() - GameServer()->m_apPlayers[i]->m_ScoreStartTick);
			aTeamScore[GameServer()->m_apPlayers[i]->GetTeam()] += aPlayerScore[i];
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
			if(!IsPlayerInRoom(i) || !CanBeMovedOnBalance(i))
				continue;

			// remember the player whom would cause lowest score-difference
			if(GameServer()->m_apPlayers[i]->GetTeam() == BiggerTeam &&
				(!pPlayer || absolute((aTeamScore[BiggerTeam ^ 1] + aPlayerScore[i]) - (aTeamScore[BiggerTeam] - aPlayerScore[i])) < ScoreDiff))
			{
				pPlayer = GameServer()->m_apPlayers[i];
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

// virtual events
void IGameController::OnPlayerJoin(CPlayer *pPlayer)
{
}

void IGameController::OnPlayerLeave(CPlayer *pPlayer)
{
}

int IGameController::OnCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, int Weapon)
{
	return DEATH_NORMAL;
}

void IGameController::OnCharacterSpawn(CCharacter *pChr)
{
}

bool IGameController::OnCharacterTile(CCharacter *pChr, int MapIndex)
{
	return false;
}

bool IGameController::OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number)
{
	return false;
}

void IGameController::OnFlagReset(CFlag *pFlag)
{
}

// virtual states
bool IGameController::CanKill(int ClientID) const
{
	return true;
}

bool IGameController::IsDisruptiveLeave(int ClientID) const
{
	return false;
}

// event
int IGameController::OnInternalCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, int Weapon)
{
	int DeathFlag = OnCharacterDeath(pVictim, pKiller, Weapon);

	if(!(DeathFlag & DEATH_KEEP_SOLO))
		GameServer()->m_pTeams->m_Core.SetSolo(pVictim->GetPlayer()->GetCID(), false);

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

	// update spectator modes for dead players in survival
	if(IsSurvival())
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
			if(IsPlayerInRoom(i) && GameServer()->m_apPlayers[i]->m_DeadSpecMode)
				GameServer()->m_apPlayers[i]->UpdateDeadSpecMode();
	}

	return DeathFlag & (DEATH_VICTIM_HAS_FLAG | DEATH_KILLER_HAS_FLAG);
}

void IGameController::OnInternalCharacterSpawn(CCharacter *pChr)
{
	OnCharacterSpawn(pChr);
}

bool IGameController::OnInternalCharacterTile(CCharacter *pChr, int MapIndex)
{
	if(OnCharacterTile(pChr, MapIndex))
		return true;

	CPlayer *pPlayer = pChr->GetPlayer();
	int ClientID = pPlayer->GetCID();

	int m_TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
	int m_TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);

	// solo part
	if(((m_TileIndex == TILE_SOLO_ENABLE) || (m_TileFIndex == TILE_SOLO_ENABLE)) && !GameServer()->m_pTeams->m_Core.GetSolo(ClientID))
	{
		GameServer()->SendChatTarget(ClientID, "You are now in a solo part");
		pChr->SetSolo(true);
	}
	else if(((m_TileIndex == TILE_SOLO_DISABLE) || (m_TileFIndex == TILE_SOLO_DISABLE)) && GameServer()->m_pTeams->m_Core.GetSolo(ClientID))
	{
		GameServer()->SendChatTarget(ClientID, "You are now out of the solo part");
		pChr->SetSolo(false);
	}

	return false;
}

void IGameController::OnInternalEntity(int Index, vec2 Pos, int Layer, int Flags, int Number)
{
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
		CProjectile *bullet = new CProjectile(
			GameWorld(),
			WEAPON_SHOTGUN, //Type
			-1, //Owner
			Pos, //Pos
			vec2(sin(Deg), cos(Deg)), //Dir
			-2, //Span
			0, //Damage
			true, //Explosive
			0, //Force
			(g_Config.m_SvShotgunBulletSound) ? SOUND_GRENADE_EXPLODE : -1, //SoundImpact
			true, //Freeze
			Layer,
			Number);
		bullet->SetBouncing(2 - (Dir % 2));
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
		CProjectile *bullet = new CProjectile(
			GameWorld(),
			WEAPON_SHOTGUN, //Type
			-1, //Owner
			Pos, //Pos
			vec2(sin(Deg), cos(Deg)), //Dir
			-2, //Span
			0, //Damage
			false, //Explosive
			0,
			SOUND_GRENADE_EXPLODE,
			true, //Freeze
			Layer,
			Number);
		bullet->SetBouncing(2 - (Dir % 2));
	}

	if(Index == ENTITY_ARMOR_1)
		Type = POWERUP_ARMOR;
	else if(Index == ENTITY_HEALTH_1)
		Type = POWERUP_HEALTH;
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

void IGameController::OnInternalPlayerJoin(CPlayer *pPlayer, bool ServerJoin, bool Creating)
{
	int ClientID = pPlayer->GetCID();
	pPlayer->m_IsReadyToPlay = !IsPlayerReadyMode();
	pPlayer->m_RespawnDisabled = GetStartRespawnState();
	pPlayer->m_Vote = 0;
	pPlayer->m_VotePos = 0;

	if(GameServer()->m_VoteCloseTime > 0)
		GameServer()->SendVoteSet(ClientID);
	else
		SendVoteSet(ClientID);

	// HACK: resend map info can reset player's team info
	// SideEffect: gets rid of ddnet dummies
	// TODO: enable this
	// if(!ServerJoin && !Server()->IsSixup(ClientID))
	// 	Server()->SendMap(ClientID);

	// update game info first
	UpdateGameInfo(ClientID);

	// change team second
	pPlayer->SetTeam(GetStartTeam(), false);

	// sixup: update team info for fake spectators
	pPlayer->SendCurrentTeamInfo();

	pPlayer->Respawn();

	m_aFakeClientBroadcast[ClientID].m_LastGameState = -1;
	m_aFakeClientBroadcast[ClientID].m_LastTimer = -1;
	m_aFakeClientBroadcast[ClientID].m_NextBroadcastTick = -1;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "ddrteam_join player='%d:%s' team=%d ddrteam='%d'", ClientID, Server()->ClientName(ClientID), pPlayer->GetTeam(), GameWorld()->Team());
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	if(ServerJoin)
		str_format(aBuf, sizeof(aBuf), "'%s' entered and joined the %s in %s room %d", Server()->ClientName(ClientID), GetTeamName(pPlayer->GetTeam()), m_pGameType, GameWorld()->Team());
	else
		str_format(aBuf, sizeof(aBuf), "'%s' %sjoined the %s in %s room %d", Server()->ClientName(ClientID), Creating ? "created and " : "", GetTeamName(pPlayer->GetTeam()), m_pGameType, GameWorld()->Team());

	GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, -1);

	OnPlayerJoin(pPlayer);

	m_VoteUpdate = true;
}

void IGameController::OnInternalPlayerLeave(CPlayer *pPlayer, bool ServerLeave)
{
	int ClientID = pPlayer->GetCID();

	if(Server()->ClientIngame(ClientID))
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "ddrteam_leave player='%d:%s' ddrteam='%d'", ClientID, Server()->ClientName(ClientID), GameWorld()->Team());
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}

	if(pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		--m_aTeamSize[pPlayer->GetTeam()];
		dbg_msg("game", "team size decreased to %d, team='%d', ddrteam='%d'", m_aTeamSize[pPlayer->GetTeam()], pPlayer->GetTeam(), GameWorld()->Team());
		m_UnbalancedTick = TBALANCE_CHECK;
	}

	CheckReadyStates(ClientID);
	OnPlayerLeave(pPlayer);

	if(m_VoteVictim == ClientID && (GameWorld()->Team() > 0 || ServerLeave))
	{
		EndVote(true);
	}
	m_VoteUpdate = true;
}

void IGameController::OnReset()
{
	for(auto &pPlayer : GameServer()->m_apPlayers)
	{
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
			pPlayer->m_IsReadyToPlay = true;
		}
	}
}

// game
bool IGameController::GetFlagState(SFlagState *pState)
{
	return false;
}

bool IGameController::DoWincheckMatch()
{
	if(IsTeamplay())
	{
		// check score win condition
		if((m_GameInfo.m_ScoreLimit > 0 && (m_aTeamscore[TEAM_RED] >= m_GameInfo.m_ScoreLimit || m_aTeamscore[TEAM_BLUE] >= m_GameInfo.m_ScoreLimit)) ||
			(m_GameInfo.m_TimeLimit > 0 && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60) ||
			(m_GameInfo.m_MatchNum > 0 && m_GameInfo.m_MatchCurrent >= m_GameInfo.m_MatchNum))
		{
			if(m_aTeamscore[TEAM_RED] != m_aTeamscore[TEAM_BLUE] || IsSurvival())
			{
				EndMatch();
				return true;
			}
			else
				m_SuddenDeath = 1;
		}
	}
	else
	{
		// gather some stats
		int Topscore = 0;
		int TopscoreCount = 0;
		for(auto &pPlayer : GameServer()->m_apPlayers)
		{
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
			(m_GameInfo.m_TimeLimit > 0 && (Server()->Tick() - m_GameStartTick) >= m_GameInfo.m_TimeLimit * Server()->TickSpeed() * 60) ||
			(m_GameInfo.m_MatchNum > 0 && m_GameInfo.m_MatchCurrent >= m_GameInfo.m_MatchNum))
		{
			if(TopscoreCount == 1)
			{
				EndMatch();
				return true;
			}
			else
				m_SuddenDeath = 1;
		}
	}
	return false;
}

bool IGameController::DoWincheckRound()
{
	return true;
}

void IGameController::ResetGame()
{
	// reset the game
	GameWorld()->m_ResetRequested = true;

	SetGameState(IGS_GAME_RUNNING);
	m_SuddenDeath = 0;

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

				// enable respawning in survival when activating warmup
				if(IsSurvival())
				{
					for(int i = 0; i < MAX_CLIENTS; ++i)
						if(GameServer()->m_apPlayers[i])
							GameServer()->m_apPlayers[i]->m_RespawnDisabled = false;
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
					if(Config()->m_SvPlayerReadyMode)
					{
						// run warmup till all players are ready
						SetPlayersReadyState(false);
					}
				}
				else if(Timer > 0)
				{
					// run warmup for a specific time intervall
					m_GameState = GameState;
					m_GameStateTimer = Timer * Server()->TickSpeed();
				}

				// enable respawning in survival when activating warmup
				if(IsSurvival())
				{
					for(int i = 0; i < MAX_CLIENTS; ++i)
						if(GameServer()->m_apPlayers[i])
							GameServer()->m_apPlayers[i]->m_RespawnDisabled = false;
				}
				GameWorld()->m_Paused = false;
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
		if(GameState == IGS_END_ROUND && DoWincheckMatch())
			break;
		// only possible when game is running or over
		if(m_GameState == IGS_GAME_RUNNING || m_GameState == IGS_END_MATCH || m_GameState == IGS_END_ROUND || m_GameState == IGS_GAME_PAUSED)
		{
			m_GameState = GameState;
			m_GameStateTimer = Timer * Server()->TickSpeed();
			m_SuddenDeath = 0;
			GameWorld()->m_Paused = true;
		}
	}
}

void IGameController::StartMatch()
{
	ResetGame();
	CheckGameInfo();

	m_GameStartTick = Server()->Tick();
	m_RoundCount = 0;
	m_GameInfo.m_MatchCurrent = m_RoundCount + 1;
	for(int i = 0; i < MAX_CLIENTS; ++i)
		UpdateGameInfo(i);
	m_aTeamscore[TEAM_RED] = 0;
	m_aTeamscore[TEAM_BLUE] = 0;

	// start countdown if there're enough players, otherwise do warmup till there're
	if(HasEnoughPlayers())
		SetGameState(IGS_START_COUNTDOWN);
	else
		SetGameState(IGS_WARMUP_GAME, TIMER_INFINITE);

	// TODO: fix demo
	// Server()->DemoRecorder_HandleAutoStart();
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "start match type='%s' teamplay='%d' ddrteam='%d'", m_pGameType, m_GameFlags & IGF_TEAMS, GameWorld()->Team());
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
}

void IGameController::StartRound()
{
	ResetGame();

	++m_RoundCount;
	m_GameInfo.m_MatchCurrent = m_RoundCount + 1;
	for(int i = 0; i < MAX_CLIENTS; ++i)
		UpdateGameInfo(i);

	// start countdown if there're enough players, otherwise abort to warmup
	if(HasEnoughPlayers())
		SetGameState(IGS_START_COUNTDOWN);
	else
		SetGameState(IGS_WARMUP_GAME, TIMER_INFINITE);
}

void IGameController::SwapTeamscore()
{
	if(!IsTeamplay())
		return;

	int Score = m_aTeamscore[TEAM_RED];
	m_aTeamscore[TEAM_RED] = m_aTeamscore[TEAM_BLUE];
	m_aTeamscore[TEAM_BLUE] = Score;
}

// for compatibility of 0.7's round ends and infinite warmup
void IGameController::FakeClientBroadcast(int SnappingClient)
{
	if(Server()->IsSixup(SnappingClient))
		return;

	SBroadcastState *pState = &m_aFakeClientBroadcast[SnappingClient];
	int TimerNumber = (int)ceil(m_GameStateTimer / (float)Server()->TickSpeed());

	if(pState->m_LastGameState == m_GameState && pState->m_LastTimer == TimerNumber && (pState->m_NextBroadcastTick < 0 || Server()->Tick() < pState->m_NextBroadcastTick))
		return;

	pState->m_NextBroadcastTick = -1;
	pState->m_LastGameState = m_GameState;
	pState->m_LastTimer = TimerNumber;

	switch(m_GameState)
	{
	case IGS_WARMUP_GAME:
	case IGS_WARMUP_USER:
		if(m_GameStateTimer == TIMER_INFINITE)
			GameServer()->SendBroadcast("Waiting for more players", SnappingClient, false);
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
	case IGS_END_MATCH:
		GameServer()->SendBroadcast(" ", SnappingClient, false);
		break;
	}
}

void IGameController::Snap(int SnappingClient)
{
	// TODO: smarter broadcast
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
	{
		if(isSixUp)
			GameStateFlags |= protocol7::GAMESTATEFLAG_SUDDENDEATH;
		else
			GameStateFlags |= GAMESTATEFLAG_SUDDENDEATH;
	}

	if(!isSixUp)
	{
		CNetObj_GameInfo *pGameInfoObj = (CNetObj_GameInfo *)Server()->SnapNewItem(NETOBJTYPE_GAMEINFO, 0, sizeof(CNetObj_GameInfo));
		if(!pGameInfoObj)
			return;

		pGameInfoObj->m_GameFlags = m_GameFlags;
		pGameInfoObj->m_GameStateFlags = GameStateFlags;
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

		pGameInfoEx->m_Flags =
			GAMEINFOFLAG_ALLOW_EYE_WHEEL |
			GAMEINFOFLAG_ALLOW_HOOK_COLL |
			GAMEINFOFLAG_ENTITIES_DDNET |
			GAMEINFOFLAG_ENTITIES_DDRACE |
			GAMEINFOFLAG_ENTITIES_RACE |
			GAMEINFOFLAG_PREDICT_VANILLA |
			GAMEINFOFLAG_PREDICT_DDRACE_TILES;
		pGameInfoEx->m_Flags2 = 0;
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
		// TODO: fix demo
		if(SnappingClient < 0)
		{
			protocol7::CNetObj_De_GameInfo *pGameInfo = static_cast<protocol7::CNetObj_De_GameInfo *>(Server()->SnapNewItem(-protocol7::NETOBJTYPE_DE_GAMEINFO, 0, sizeof(protocol7::CNetObj_De_GameInfo)));
			if(!pGameInfo)
				return;

			pGameInfo->m_GameFlags = m_GameFlags;
			pGameInfo->m_ScoreLimit = m_GameInfo.m_ScoreLimit;
			pGameInfo->m_TimeLimit = m_GameInfo.m_TimeLimit;
			pGameInfo->m_MatchNum = m_GameInfo.m_MatchNum;
			pGameInfo->m_MatchCurrent = m_GameInfo.m_MatchCurrent;
		}
	}
}

void IGameController::Tick()
{
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
					if(GameServer()->m_apPlayers[i])
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
					if(!GameServer()->m_apPlayers[i] || aVoteChecked[i])
						continue;

					if(GameServer()->GetPlayerDDRTeam(i) != GameWorld()->Team())
						continue;

					if((IsKickVote() || IsSpecVote()) && (GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS ||
										     (GameServer()->GetPlayerChar(m_VoteCreator) && GameServer()->GetPlayerChar(i))))
						continue;

					if(GameServer()->m_apPlayers[i]->m_Afk && i != m_VoteCreator)
						continue;

					// can't vote in kick and spec votes in the beginning after joining
					if((IsKickVote() || IsSpecVote()) && Now < GameServer()->m_apPlayers[i]->m_FirstVoteTick)
						continue;

					// connecting clients with spoofed ips can clog slots without being ingame
					if(((CServer *)Server())->m_aClients[i].m_State != CServer::CClient::STATE_INGAME)
						continue;

					// don't count votes by blacklisted clients
					if(g_Config.m_SvDnsblVote && !m_pServer->DnsblWhite(i) && !SinglePlayer)
						continue;

					int ActVote = GameServer()->m_apPlayers[i]->m_Vote;
					int ActVotePos = GameServer()->m_apPlayers[i]->m_VotePos;

					// only allow IPs to vote once
					// check for more players with the same ip (only use the vote of the one who voted first)
					for(int j = i + 1; j < MAX_CLIENTS; j++)
					{
						if(!GameServer()->m_apPlayers[j] || aVoteChecked[j] || str_comp(aaBuf[j], aaBuf[i]) != 0)
							continue;

						// count the latest vote by this ip
						if(ActVotePos < GameServer()->m_apPlayers[j]->m_VotePos)
						{
							ActVote = GameServer()->m_apPlayers[j]->m_Vote;
							ActVotePos = GameServer()->m_apPlayers[j]->m_VotePos;
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
				Server()->SetRconCID(IServer::RCON_CID_VOTE);
				InstanceConsole()->ExecuteLine(m_aVoteCommand, m_VoteCreator);
				Server()->SetRconCID(IServer::RCON_CID_SERV);
				EndVote(true);
				SendChatTarget(-1, "Vote passed", CGameContext::CHAT_SIX);

				if(GameServer()->m_apPlayers[m_VoteCreator] && !IsKickVote() && !IsSpecVote())
					GameServer()->m_apPlayers[m_VoteCreator]->m_LastVoteCall = 0;
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_YES_ADMIN)
			{
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "Vote passed enforced by authorized player");
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
				// TODO: swap team for round
				StartRound();
				break;
			case IGS_END_MATCH:
				// TODO: swap team for match
				// if(Config()->m_SvMatchSwap)
				// 	GameServer()->SwapTeams();
				m_MatchCount++;
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
				if(!Config()->m_SvPlayerReadyMode && m_GameStateTimer == TIMER_INFINITE)
					SetGameState(IGS_WARMUP_USER, 0);
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

	// check for inactive players
	DoActivityCheck();

	if((m_GameState == IGS_GAME_RUNNING || m_GameState == IGS_GAME_PAUSED) && !GameWorld()->m_ResetRequested)
	{
		// win check
		if(DoWincheckRound())
			DoWincheckMatch();
	}
}

// info
void IGameController::CheckGameInfo()
{
	bool GameInfoChanged = (m_GameInfo.m_MatchNum != m_Roundlimit) ||
			       (m_GameInfo.m_ScoreLimit != m_Scorelimit) ||
			       (m_GameInfo.m_TimeLimit != m_Timelimit);
	m_GameInfo.m_MatchNum = m_Roundlimit;
	m_GameInfo.m_ScoreLimit = m_Scorelimit;
	m_GameInfo.m_TimeLimit = m_Timelimit;
	if(GameInfoChanged)
		for(int i = 0; i < MAX_CLIENTS; ++i)
			UpdateGameInfo(i);
}

bool IGameController::IsFriendlyFire(int ClientID1, int ClientID2) const
{
	if(ClientID1 == ClientID2)
		return false;

	if(IsTeamplay())
	{
		if(!GameServer()->m_apPlayers[ClientID1] || !GameServer()->m_apPlayers[ClientID2])
			return false;

		if(!m_Teamdamage && GameServer()->m_apPlayers[ClientID1]->GetTeam() == GameServer()->m_apPlayers[ClientID2]->GetTeam())
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
	return Config()->m_SvPlayerReadyMode != 0 && (m_GameStateTimer == TIMER_INFINITE && (m_GameState == IGS_WARMUP_USER || m_GameState == IGS_GAME_PAUSED));
}

bool IGameController::IsTeamChangeAllowed() const
{
	return !GameWorld()->m_Paused || (m_GameState == IGS_START_COUNTDOWN && m_GameStartTick == Server()->Tick());
}

void IGameController::UpdateGameInfo(int ClientID)
{
	if(!IsPlayerInRoom(ClientID) || !Server()->ClientIngame(ClientID))
		return;

	if(Server()->IsSixup(ClientID))
	{
		protocol7::CNetMsg_Sv_GameInfo GameInfoMsg;
		GameInfoMsg.m_GameFlags = m_GameFlags;
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
		if(!GameServer()->IsPlayerValid(CID) || GameServer()->GetPlayerDDRTeam(CID) != GameWorld()->Team())
			continue;

		if(Server()->IsSixup(CID))
		{
			CMsgPacker Msg(protocol7::NETMSGTYPE_SV_GAMEMSG);
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
				GameServer()->SendChatTarget(CID, "Teams were swapped");
				break;
			case GAMEMSG_SPEC_INVALIDID:
				GameServer()->SendChatTarget(CID, "You can't spectate this player");
				break;
			case GAMEMSG_TEAM_SHUFFLE:
				GameServer()->SendChatTarget(CID, "Teams were shuffled");
				break;
			case GAMEMSG_TEAM_BALANCE:
				GameServer()->SendChatTarget(CID, "Teams have been balanced");
				break;
			case GAMEMSG_CTF_DROP:
				GameWorld()->CreateSoundGlobal(SOUND_CTF_DROP, CID);
				break;
			case GAMEMSG_CTF_RETURN:
				GameWorld()->CreateSoundGlobal(SOUND_CTF_RETURN, CID);
				break;
			case GAMEMSG_TEAM_ALL:
			{
				if(!i1)
					break;

				if(!aBuf[0])
					str_format(aBuf, sizeof(aBuf), "All players were moved to the %s", GetTeamName(*i1));
				GameServer()->SendChatTarget(CID, aBuf);
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

				if(GameServer()->m_apPlayers[CID]->GetTeam() == *i1)
					GameWorld()->CreateSoundGlobal(SOUND_CTF_GRAB_PL, CID);
				else
					GameWorld()->CreateSoundGlobal(SOUND_CTF_GRAB_EN, CID);
				break;
			case GAMEMSG_CTF_CAPTURE:
			{
				if(!i1 || !i2 || !i3)
					break;

				if(!aBuf[0])
				{
					float CaptureTime = *i3 / (float)Server()->TickSpeed();
					if(CaptureTime <= 60)
					{
						str_format(aBuf, sizeof(aBuf), "The %s flag was captured by '%s' (%d.%s%d seconds)", *i1 ? "blue" : "red", Server()->ClientName(*i2), (int)CaptureTime % 60, ((int)(CaptureTime * 100) % 100) < 10 ? "0" : "", (int)(CaptureTime * 100) % 100);
					}
					else
					{
						str_format(aBuf, sizeof(aBuf), "The %s flag was captured by '%s'", *i1 ? "blue" : "red", Server()->ClientName(*i2));
					}
				}
				GameServer()->SendChatTarget(CID, aBuf);
				GameWorld()->CreateSoundGlobal(SOUND_CTF_CAPTURE, CID);
				break;
			}
			case GAMEMSG_GAME_PAUSED:
				if(!i1)
					break;

				if(!aBuf[0])
					str_format(aBuf, sizeof(aBuf), "'%s' initiated a pause", Server()->ClientName(*i1));
				GameServer()->SendChatTarget(CID, aBuf);
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
	Eval.m_RandomSpawn = IsSurvival();

	if(IsTeamplay())
	{
		Eval.m_FriendlyTeam = Team;

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
		// team mates are not as dangerous as enemies
		float Scoremod = 1.0f;
		if(pEval->m_FriendlyTeam != -1 && pC->GetPlayer()->GetTeam() == pEval->m_FriendlyTeam)
			Scoremod = 0.5f;

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
			if(!GameWorld()->m_Core.m_Tuning[0].m_PlayerCollision)
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
		// TODO: frandom to pRng?
		float S = pEval->m_RandomSpawn ? (Result + frandom()) : EvaluateSpawnPos(pEval, P);
		if(!pEval->m_Got || pEval->m_Score > S)
		{
			pEval->m_Got = true;
			pEval->m_Score = S;
			pEval->m_Pos = P;
		}
	}
}

bool IGameController::GetStartRespawnState() const
{
	if(IsSurvival())
	{
		// players can always respawn during warmup or match/round start countdown
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

bool IGameController::CanJoinTeam(int Team, int NotThisID) const
{
	if(Team == TEAM_SPECTATORS)
		return true;

	// check if there're enough player slots left
	int TeamMod = IsPlayerInRoom(NotThisID) && GameServer()->m_apPlayers[NotThisID]->GetTeam() != TEAM_SPECTATORS ? -1 : 0;
	return TeamMod + m_aTeamSize[TEAM_RED] + m_aTeamSize[TEAM_BLUE] < Config()->m_SvMaxClients - Config()->m_SvSpectatorSlots;
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
	if(Team == pPlayer->GetTeam())
		return;

	int OldTeam = pPlayer->GetTeam();
	pPlayer->SetTeam(Team, DoChatMsg);

	int ClientID = pPlayer->GetCID();

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' team=%d->%d", ClientID, Server()->ClientName(ClientID), OldTeam, Team);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	// update effected game settings
	if(OldTeam != TEAM_SPECTATORS)
	{
		--m_aTeamSize[OldTeam];
		dbg_msg("game", "team size decreased to %d, team='%d', ddrteam='%d'", m_aTeamSize[OldTeam], Team, GameWorld()->Team());
		m_UnbalancedTick = TBALANCE_CHECK;
	}
	if(Team != TEAM_SPECTATORS)
	{
		++m_aTeamSize[Team];
		dbg_msg("game", "team size increased to %d, team='%d', ddrteam='%d'", m_aTeamSize[Team], Team, GameWorld()->Team());
		m_UnbalancedTick = TBALANCE_CHECK;
		if(m_GameState == IGS_WARMUP_GAME && HasEnoughPlayers())
			SetGameState(IGS_WARMUP_GAME, 0);
		pPlayer->m_IsReadyToPlay = !IsPlayerReadyMode();
		if(IsSurvival())
			pPlayer->m_RespawnDisabled = GetStartRespawnState();
	}

	CheckReadyStates();

	// reset inactivity counter when joining the game
	if(OldTeam == TEAM_SPECTATORS)
		pPlayer->m_InactivityTickCounter = 0;

	m_VoteUpdate = true;
}

int IGameController::GetStartTeam()
{
	if(Config()->m_SvTournamentMode)
		return TEAM_SPECTATORS;

	// determine new team
	int Team = TEAM_RED;
	if(IsTeamplay())
	{
#ifdef CONF_DEBUG
		if(!Config()->m_DbgStress) // this will force the auto balancer to work overtime aswell
			Team = m_aTeamSize[TEAM_RED] > m_aTeamSize[TEAM_BLUE] ? TEAM_BLUE : TEAM_RED;
#endif // CONF_DEBUG
	}

	// check if there're enough player slots left
	if(m_aTeamSize[TEAM_RED] + m_aTeamSize[TEAM_BLUE] < Config()->m_SvMaxClients - Config()->m_SvSpectatorSlots)
	{
		++m_aTeamSize[Team];
		dbg_msg("game", "team size increased to %d, team='%d', ddrteam='%d'", m_aTeamSize[Team], Team, GameWorld()->Team());
		m_UnbalancedTick = TBALANCE_CHECK;
		if(m_GameState == IGS_WARMUP_GAME && HasEnoughPlayers())
			SetGameState(IGS_WARMUP_GAME, 0);
		return Team;
	}
	return TEAM_SPECTATORS;
}

const char *IGameController::GetTeamName(int Team)
{
	if(Team == 0)
		return "game";
	return "spectators";
}

// ddrace
int IGameController::GetPlayerTeam(int ClientID) const
{
	return GameServer()->GetPlayerDDRTeam(ClientID);
}

bool IGameController::IsPlayerInRoom(int ClientID) const
{
	return GameServer()->IsPlayerValid(ClientID) && GameServer()->GetPlayerDDRTeam(ClientID) == GameWorld()->Team();
}

void IGameController::InitController(int Team, class CGameContext *pGameServer, class CGameWorld *pWorld)
{
	m_pGameServer = pGameServer;
	m_pConfig = m_pGameServer->Config();
	m_pServer = m_pGameServer->Server();
	m_pWorld = pWorld;
	m_GameStartTick = m_pServer->Tick();
	if(m_Warmup)
		SetGameState(IGS_WARMUP_USER, m_Warmup);
	else
		SetGameState(IGS_WARMUP_GAME, TIMER_INFINITE);
	m_GameInfo.m_ScoreLimit = m_Scorelimit;
	m_GameInfo.m_TimeLimit = m_Timelimit;
	m_GameInfo.m_MatchNum = m_Roundlimit;
}

void IGameController::CallVote(int ClientID, const char *pDesc, const char *pCmd, const char *pReason, const char *pChatmsg, const char *pSixupDesc)
{
	// check if a vote is already running
	if(m_VoteCloseTime)
		return;

	int64 Now = Server()->Tick();
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];

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
	for(auto &pPlayer : GameServer()->m_apPlayers)
	{
		if(pPlayer && IsPlayerInRoom(pPlayer->GetCID()))
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

void IGameController::SendVoteSet(int ClientID) const
{
	if(ClientID >= 0 && !IsPlayerInRoom(ClientID))
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
			if(!IsPlayerInRoom(i))
				continue;
			if(!Server()->IsSixup(i))
				Server()->SendPackMsg(&Msg6, MSGFLAG_VITAL, i);
			else
				Server()->SendPackMsg(&Msg7, MSGFLAG_VITAL, i);
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

	if(!IsPlayerInRoom(ClientID))
		return;

	if(Total > VANILLA_MAX_CLIENTS && GameServer()->IsPlayerValid(ClientID) && GameServer()->m_apPlayers[ClientID]->GetClientVersion() <= VERSION_DDRACE)
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
		if(IsPlayerInRoom(i))
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
		if(IsPlayerInRoom(i))
			GameServer()->SendBroadcast(pText, i, IsImportant);
}

void IGameController::InstanceConsolePrint(const char *pStr, void *pUser, ColorRGBA PrintColor)
{
	IGameController *pController = (IGameController *)pUser;

	int TargetID = pController->m_ChatResponseTargetID;
	if(TargetID < 0 || TargetID >= MAX_CLIENTS)
		TargetID = -1;

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
	pController->SendChatTarget(TargetID, aBuf);
	pController->m_ChatResponseTargetID = -1;
}

void IGameController::ConchainReplyOnly(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	IGameController *pThis = static_cast<IGameController *>(pUserData);
	pThis->m_ChatResponseTargetID = pResult->m_ClientID;
	pfnCallback(pResult, pCallbackUserData);
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

		if(pResult->m_ClientID != IConsole::CLIENT_ID_GAME)
			str_copy(pData->m_pOldValue, pData->m_pStr, pData->m_MaxSize);
	}
	else
	{
		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "Value: %s", pData->m_pStr);
		pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}
}