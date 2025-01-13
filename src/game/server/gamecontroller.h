/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTROLLER_H
#define GAME_SERVER_GAMECONTROLLER_H

#include <base/vmath.h>
#include <engine/console.h>
#include <engine/map.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/voting.h>

#include <map>
#include <vector>

#define INSTANCE_CONFIG_INT(Pointer, Command, Default, Min, Max, Flag, Desc) \
	{ \
		*Pointer = Default; \
		CIntVariableData *pInt = new CIntVariableData({IGameController::InstanceConsole(), Pointer, Min, Max, Default}); \
		IGameController::m_IntConfigStore.push_back(pInt); \
		IGameController::InstanceConsole()->Register(Command, "?i[value]", Flag, IGameController::IntVariableCommand, pInt, Desc); \
	}

#define INSTANCE_CONFIG_STR(Pointer, Command, Default, Flag, Desc) \
	{ \
		str_copy(Pointer, Default, sizeof(Pointer)); \
		CStrVariableData *pStr = new CStrVariableData({IGameController::InstanceConsole(), Pointer, sizeof(Pointer), nullptr}); \
		m_StrConfigStore.push_back(pStr); \
		IGameController::InstanceConsole()->Register(Command, "?r[value]", Flag, IGameController::StrVariableCommand, pStr, Desc); \
	}

#define INSTANCE_COMMAND_REMOVE(Command) \
	{ \
		IGameController::InstanceConsole()->Register(Command, "", 0, IGameController::EmptyCommand, nullptr, "This command has been removed"); \
	}

// for OnCharacterDeath
enum
{
	DEATH_NORMAL = 0,
	DEATH_VICTIM_HAS_FLAG = 1,
	DEATH_KILLER_HAS_FLAG = 2,

	// Do not increase killers score
	DEATH_SKIP_SCORE = 4,

	// Do not delay respawn if player killed
	DEATH_NO_SUICIDE_PANATY = 8,

	// Keep solo states when a character killed, be careful when using this
	DEATH_KEEP_SOLO = 16,

	// Do not generate kill message
	DEATH_NO_KILL_MSG = 32,
};

// for OnCharacterTakeDamage
enum
{
	DAMAGE_NORMAL = 0,
	// Do not take damage
	DAMAGE_NO_DAMAGE = 1,
	// Do not send hit sound to attacker
	DAMAGE_NO_HITSOUND = 2,
	// Do not send pain sound
	DAMAGE_NO_PAINSOUND = 4,
	// Do not change victim's emote
	DAMAGE_NO_EMOTE = 8,
	// Do not apply force
	DAMAGE_NO_KNOCKBACK = 16,
	// Do not die, even if character's health is 0
	DAMAGE_NO_DEATH = 32,
	// Do not send indicator stars
	DAMAGE_NO_INDICATOR = 64,
	// Do not do anything
	DAMAGE_SKIP = 127,
	// Use this if you still want everything but the character is killed by controller
	DAMAGE_DIED = DAMAGE_NO_DAMAGE | DAMAGE_NO_EMOTE | DAMAGE_NO_KNOCKBACK | DAMAGE_NO_DEATH,
};

enum
{
	INSTANCE_CONNECTION_NORMAL = 0,
	INSTANCE_CONNECTION_SERVER,
	INSTANCE_CONNECTION_CREATE,
	INSTANCE_CONNECTION_RELOAD,
	INSTANCE_CONNECTION_FORCED,
};

/*
	Class: Game Controller
		Controls the main game logic. Keeping track of team and player score,
		winning conditions and specific game logic.
*/
class IGameController
{
	class CGameContext *m_pGameServer;
	class CConfig *m_pConfig;
	class IServer *m_pServer;
	class CGameWorld *m_pWorld;
	class IConsole *m_pInstanceConsole;

	bool GetPlayersReadyState(int WithoutID = -1);
	void SetPlayersReadyState(bool ReadyState);
	void CheckReadyStates(int WithoutID = -1);
	int MakeGameFlag(int GameFlag);
	int MakeGameFlagSixUp(int GameFlag);

	struct SBroadcastState
	{
		int m_LastGameState;
		int m_LastTimer;
		int m_LastPlayerNotReady;
		int m_NextBroadcastTick;
		bool m_LastDeadSpec;
		int m_DisableUntil;
	};
	SBroadcastState m_aFakeClientBroadcast[MAX_CLIENTS];
	void FakeClientBroadcast(int SnappingClient);
	void FakeGameMsgSound(int SnappingClient, int SoundID);

protected:
	bool m_Started;

	// balancing
	enum
	{
		TBALANCE_CHECK = -2,
		TBALANCE_OK,
	};
	int m_aTeamSize[2];
	int m_UnbalancedTick;
	int m_NumPlayerNotReady;
	bool m_PauseRequested;
	int m_PauseRequestedTicks;

	void CheckTeamBalance();

	// game
	enum EGameState
	{
		// internal game states
		IGS_WARMUP_GAME, // warmup started by game because there're not enough players (infinite)
		IGS_WARMUP_USER, // warmup started by user action via rcon or new match (infinite or timer)

		IGS_START_COUNTDOWN, // start countown to unpause the game or start match/round (tick timer)

		IGS_GAME_PAUSED, // game paused (infinite or tick timer)
		IGS_GAME_RUNNING, // game running (infinite)

		IGS_END_MATCH, // match is over (tick timer)
		IGS_END_ROUND, // round is over (tick timer)
	};
	EGameState m_GameState;
	int m_GameStateTimer;

	bool HasEnoughPlayers() const
	{
		return m_MinimumPlayers > 0 && ((IsTeamplay() && m_aTeamSize[TEAM_RED] > 0 && m_aTeamSize[TEAM_BLUE] > 0 && m_aTeamSize[TEAM_RED] + m_aTeamSize[TEAM_BLUE] >= m_MinimumPlayers) || (!IsTeamplay() && m_aTeamSize[TEAM_RED] >= m_MinimumPlayers));
	}
	bool HasNoPlayers() const { return m_aTeamSize[TEAM_RED] == 0 && m_aTeamSize[TEAM_BLUE] == 0; }
	void ResetGame();
	void SetGameState(EGameState GameState, int Timer = 0);
	void StartMatch();
	void ResetMatch();
	void StartRound();

	struct CSpawnEval
	{
		CSpawnEval()
		{
			m_Got = false;
			m_SpawningTeam = -1;
			m_Pos = vec2(100, 100);
		}

		vec2 m_Pos;
		bool m_Got;
		bool m_RandomSpawn;
		int m_SpawningTeam;
		float m_Score;
	};
	vec2 m_aaSpawnPoints[3][64];
	int m_aNumSpawnPoints[3];

	float EvaluateSpawnPos(CSpawnEval *pEval, vec2 Pos) const;
	void EvaluateSpawnType(CSpawnEval *pEval, int Type) const;

	// team
	int ClampTeam(int Team) const;

	// vote
	int m_VotePos;
	int m_VoteCreator;
	int m_VoteEnforcer;
	int m_VoteType;
	int64 m_VoteCloseTime;
	bool m_VoteUpdate;
	char m_aVoteDescription[VOTE_DESC_LENGTH];
	char m_aSixupVoteDescription[VOTE_DESC_LENGTH];
	char m_aVoteCommand[VOTE_CMD_LENGTH];
	char m_aVoteReason[VOTE_REASON_LENGTH];
	int m_VoteEnforce;
	bool m_VoteWillPass;
	int m_VoteVictim;

	struct SFlagState
	{
		int m_RedFlagDroppedTick;
		int m_RedFlagCarrier;
		int m_BlueFlagDroppedTick;
		int m_BlueFlagCarrier;
	};

	// config variables
	std::vector<CIntVariableData *> m_IntConfigStore;
	std::vector<CStrVariableData *> m_StrConfigStore;

	// game
	int m_GameStartTick;
	int m_RoundCount;
	int m_SuddenDeath;
	int m_aTeamscore[2];

	// internal game flag
	enum
	{
		IGF_TEAMS = 1,
		IGF_FLAGS = 2,

		// use default survival behaviour
		// player spawns at round/match start, no respawn
		IGF_SURVIVAL = 4,

		// currently unused
		IGF_RACE = 8,

		// set if the game is round based
		// timer will reset after each round
		// and won't be used for win check by default
		IGF_ROUND_TIMER_ROUND = 16,

		// set if the game is round based
		// timer will not be reset after each round
		// and will be used for win check by default
		IGF_MATCH_TIMER_ROUND = 32,

		// whether the game mode uses suddendeath
		// applies to default win check
		IGF_SUDDENDEATH = 64,

		// mark the game as survival even if it isn't
		// ideal for controlling your own gameplay
		// while letting clients show player count
		IGF_MARK_SURVIVAL = 128,
	};
	int m_GameFlags;
	const char *m_pGameType;
	struct CGameInfo
	{
		int m_MatchCurrent;
		int m_MatchNum;
		int m_ScoreLimit;
		int m_TimeLimit;
	} m_GameInfo;

	// default to: GAMEINFOFLAG_ALLOW_EYE_WHEEL | GAMEINFOFLAG_ALLOW_HOOK_COLL | GAMEINFOFLAG_PREDICT_DDRACE_TILES
	int m_DDNetInfoFlag;
	// default to: 0
	int m_DDNetInfoFlag2;

public:
	IGameController();
	virtual ~IGameController();

	CGameContext *GameServer() const { return m_pGameServer; }
	CConfig *Config() const { return m_pConfig; }
	IServer *Server() const { return m_pServer; }
	CGameWorld *GameWorld() const { return m_pWorld; }
	IConsole *InstanceConsole() const { return m_pInstanceConsole; }

	// common config
	int m_Warmup;
	int m_Countdown;
	int m_Teamdamage;
	int m_MatchSwap;
	int m_Powerups;
	int m_Scorelimit;
	int m_Timelimit;
	int m_Roundlimit;
	int m_TeambalanceTime;
	int m_KillDelay;
	int m_PlayerSlots;
	int m_PlayerReadyMode;
	int m_ResetOnMatchEnd;
	int m_PausePerMatch;
	int m_MinimumPlayers;

	// mega map stuff
	char m_aMap[128];
	int m_MapIndex;

	// internal events
	void StartController();
	void OnInternalPlayerJoin(class CPlayer *pPlayer, int Type);
	void OnInternalPlayerLeave(class CPlayer *pPlayer, int Type);
	int OnInternalCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	void OnInternalCharacterSpawn(class CCharacter *pChr);
	bool OnInternalCharacterTile(class CCharacter *pChr, int MapIndex);
	void OnInternalEntity(int Index, vec2 Pos, int Layer, int Flags, int MegaMapIndex, int Number);
	void OnPlayerReadyChange(class CPlayer *pPlayer);
	void OnReset();

	// game
	enum
	{
		TIMER_INFINITE = -1,
		TIMER_END = 10,
	};

	void TryStartWarmup(bool FallbackToWarmup = false);
	void DoCountdown(int Seconds) { SetGameState(IGS_START_COUNTDOWN, Seconds); }
	void DoPause(int Seconds) { SetGameState(IGS_GAME_PAUSED, Seconds); }
	void DoWarmup(int Seconds) { SetGameState(IGS_WARMUP_USER, Seconds); }
	void AbortWarmup()
	{
		if((m_GameState == IGS_WARMUP_GAME || m_GameState == IGS_WARMUP_USER) && m_GameStateTimer != TIMER_INFINITE)
		{
			SetGameState(IGS_GAME_RUNNING);
		}
	}
	void SwapTeamScore();
	void SwapTeams();
	void ShuffleTeams();
	void EndMatch() { SetGameState(IGS_END_MATCH, TIMER_END); }
	void EndRound() { SetGameState(IGS_END_ROUND, TIMER_END / 2); }

	// general
	void Snap(int SnappingClient);
	void Tick();
	void UpdateGameInfo(int ClientID);

	// info
	void CheckGameInfo(bool SendInfo = true);
	bool IsFriendlyFire(int ClientID1, int ClientID2) const;
	bool IsFriendlyTeamFire(int Team1, int Team2) const;
	bool IsGamePaused() const { return m_GameState == IGS_GAME_PAUSED || m_GameState == IGS_START_COUNTDOWN; }
	bool IsGameRunning() const { return m_GameState == IGS_GAME_RUNNING; }
	bool IsPlayerReadyMode() const;
	bool IsTeamChangeAllowed() const;
	bool IsTeamplay() const { return m_GameFlags & IGF_TEAMS; }
	bool IsSurvival() const { return m_GameFlags & IGF_SURVIVAL; }
	bool IsRoundBased() const { return m_GameFlags & (IGF_ROUND_TIMER_ROUND | IGF_MATCH_TIMER_ROUND); }
	bool IsRoundTimer() const { return m_GameFlags & IGF_ROUND_TIMER_ROUND; }
	bool UseSuddenDeath() const { return m_GameFlags & IGF_SUDDENDEATH; }

	void SendGameMsg(int GameMsgID, int ClientID, int *i1 = nullptr, int *i2 = nullptr, int *i3 = nullptr);

	const char *GetGameType() const { return m_pGameType; }
	int IsEndRound() const { return m_GameState == IGS_END_ROUND; }
	int IsEndMatch() const { return m_GameState == IGS_END_MATCH; }
	int IsWarmup() const { return !m_Started || m_GameState == IGS_WARMUP_GAME || m_GameState == IGS_WARMUP_USER; }
	int IsCountdown() const { return m_GameState == IGS_START_COUNTDOWN; }
	int IsRunning() const { return m_GameState == IGS_GAME_RUNNING; }
	int GetGameStateTimer() const { return m_GameStateTimer; }

	// spawn
	bool CanSpawn(int Team, vec2 *pPos) const;
	bool GetStartRespawnState() const;

	// team
	bool CanJoinTeam(int Team, int ClientID, bool SendReason) const;

	void DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg = true);
	void ForceTeamBalance()
	{
		if(!(m_GameFlags & IGF_SURVIVAL))
			DoTeamBalance();
	}

	int GetRealPlayerNum() const { return m_aTeamSize[TEAM_RED] + m_aTeamSize[TEAM_BLUE]; }
	int GetStartTeam();

	// DDRace
	int GetPlayerTeam(int ClientID) const;
	class CPlayer *GetPlayerIfInRoom(int ClientID) const;
	void InitController(class CGameContext *pGameServer, class CGameWorld *pWorld);

	// vote
	class CHeap *m_pVoteOptionHeap;
	CVoteOptionServer *m_pVoteOptionFirst;
	CVoteOptionServer *m_pVoteOptionLast;
	int m_NumVoteOptions;
	bool m_ResendVotes;

	inline bool IsOptionVote() const { return m_VoteType == VOTE_TYPE_OPTION; };
	inline bool IsKickVote() const { return m_VoteType == VOTE_TYPE_KICK; };
	inline bool IsSpecVote() const { return m_VoteType == VOTE_TYPE_SPECTATE; };

	void CallVote(int ClientID, const char *pDesc, const char *pCmd, const char *pReason, const char *pChatmsg, const char *pSixupDesc);
	void StartVote(const char *pDesc, const char *pCommand, const char *pReason, const char *pSixupDesc);
	void EndVote(bool SendInfo);
	bool IsVoting();
	void ForceVote(int EnforcerID, bool Success);
	void SetVoteVictim(int ClientID) { m_VoteVictim = ClientID; }
	void SetVoteType(int Type) { m_VoteType = Type; }
	void VoteUpdate() { m_VoteUpdate = true; }
	struct CVoteOptionServer *GetVoteOption(int Index);

	void SendVoteSet(int ClientID) const;
	void SendVoteStatus(int ClientID, int Total, int Yes, int No) const;

	// Instance Space Ops
	void SendChatTarget(int To, const char *pText, int Flags = 3) const;
	void SendBroadcast(const char *pText, int ClientID, bool IsImportant = true) const;
	void SendKillMsg(int Killer, int Victim, int Weapon, int ModeSpecial = 0) const;

	// helpers
	bool IsDDNetEntity(int Index) const;
	bool IsDDNetTile(int Index) const;

	// Instance Config
	static void InstanceConsolePrint(const char *pStr, void *pUser);
	static void IntVariableCommand(IConsole::IResult *pResult, void *pUserData);
	static void ColVariableCommand(IConsole::IResult *pResult, void *pUserData);
	static void StrVariableCommand(IConsole::IResult *pResult, void *pUserData);
	static void EmptyCommand(IConsole::IResult *pResult, void *pUserData) {};

	// GameController Interface

	// =================
	//    GAME STATES
	// =================
	/*
		Function: CanBeMovedOnBalance
			Whether a player can be moved to the other team when a team balancing is taking place

		Arguments:
			pPlayer - which player is being evaluated.

		Return:
			return true if the player can be killed and moved to the other team

		Note:
			Team balancing will always try to match both team's player score.
			If you don't want this, you should override DoTeamBalance instead.
	*/
	virtual bool CanBeMovedOnBalance(class CPlayer *pPlayer) const { return true; };

	/*
		Function: AreTeamsUnbalanced
			Whether the teams are balanced
				if you change this, you should also override CanChangeTeam().

		Return:
			return true if the teams need balancing
	*/
	virtual bool AreTeamsUnbalanced() const { return absolute(m_aTeamSize[TEAM_RED] - m_aTeamSize[TEAM_BLUE]) >= 2; };

	/*
		Function: CanChangeTeam
			Whether a player can change his team

		Arguments:
			Player - the player that is changing team
			JoinTeam - which team is the player joining

		Return:
			return true if the player is allowed to change team
	*/
	virtual bool CanChangeTeam(class CPlayer *pPlayer, int JoinTeam) const;

	// ==================
	//    GAME ACTIONS
	// ==================
	/*
		Function: DoTeamBalance
			Called when team balance time occurs and allowed team difference is
	*/
	virtual void DoTeamBalance();

	/*
		Function: DoWincheckRound
			In round based game:
				this will be called after gamestate is updated and before OnPostTick
				and if you call EndRound(), DoWincheckMatch() will be also called
				to check whether it is gameover
			In non-round game:
				this won't be called

		Note:
			You should call EndRound() to end the round
	*/
	virtual void DoWincheckRound() {};

	/*
		Function: DoWincheckMatch
			In round based game:
				this will be called if EndRound is called
			In non-round game:
				this will be called after gamestate is updated and before OnPostTick

		Note:
			You should call EndMatch() to end the match

	*/
	virtual void DoWincheckMatch();

	/*
		Function: GetFlagState
			Called during Snap() to send flag information to clients

		Return:
			true if the game wants to send flag info.
	*/
	virtual bool GetFlagState(SFlagState *pState) { return false; };

	/*
		Function: IsSpawnRandom
			Whether the character's spawn is randomly choosen
				or distance based

		Return:
			true if the game wants to send flag info.
	*/
	virtual bool IsSpawnRandom() const { return IsWarmup() ? false : IsSurvival(); };

	/*
		Function: SpawnPosDangerScore
			How danger is the character at this position

		Arguments:
			Pos - position that a character is trying to spawn at
			SpawningTeam - which team is the spawning character
			Char - the "dangreous" character which is not the spawning character.

		Return:
			float - a score represents the dangerousness, 0 being no danger at all.
	*/
	virtual float SpawnPosDangerScore(vec2 Pos, int SpawningTeam, class CCharacter *pChar) const;

	/*
		Function: CanDeadPlayerFollow
			Whether a dead spec player is allowed to spectate a client

		Arguments:
			Spec - the spectating player
			Target - the target player that is being spectated
				This player is guaranteed to be in the room

		Return:
			bool - true if can spectate

	*/
	virtual bool CanDeadPlayerFollow(const class CPlayer *pSpectator, const class CPlayer *pTarget);

	/*
		Function: CanPause
			Whether a game is allowed to be paused. Player can call pause anytime
				but the game will only pause after this has been true
				This will be called every tick starting from a player calling pause
				And will stop being called after pause

		Arguments:
			RequestedTicks - how long has the request been issued

		Return:
			bool - true if can pause
	*/
	virtual bool CanPause(int RequestedTicks) { return true; }

	// =============
	//   GAME CORE
	// =============
	/*
		Function: OnPreTick
			Called before the gamestate is updated during a Tick
	*/
	virtual void OnPreTick() {};

	/*
		Function: OnPostTick
			Called after the gamestate is updated during a Tick
	*/
	virtual void OnPostTick() {};

	/*
		Function: OnSnap
			Called during Snap() in case you need to fake some snapshots
			Usually this is not needed
	*/
	virtual void OnSnap(int SnappingClient) {};

	// =================
	//    GAME EVENTS
	// =================
	/*
		Function: OnInit
			Called when the controller is initialized or reloaded.
			When this is called, pWorld is garanteed to be empty and
				all existing entities has already been destroyed.

			You should reset any pointers to entities during OnInit.
	*/
	virtual void OnInit() {};
	/*
		Function: OnControllerStart
			Called when the controller and its world are fully prepared.
			When this is called:
				the controllers gamestate is properly set,
				and the config file is already executed.
	*/
	virtual void OnControllerStart() {};

	/*
		Function: OnGameStart
			Called when a round or match is starting
			When this is called:
				the world clear request is set, but has not been cleared yet
				if you want to cancel player's spawn use OnWorldReset instead

		Arguments:
			IsRound - whether it is a round that is starting
	*/
	virtual void OnGameStart(bool IsRound) {};

	/*
		Function: OnWorldReset
			Called after the world is reset
				usually happens when round or match starts
			When this is called:
				all players started respawning
				but no character has spawned yet
				world is not garanteed to be empty
	*/
	virtual void OnWorldReset() {};

	/*
		Function: OnPlayerJoin
			Called when a player joins the game controlled by this controller.
			This is called before the player's character is spawned.

		Arguments:
			pPlayer - The CPlayer that is joining.
	*/
	virtual void OnPlayerJoin(class CPlayer *pPlayer) {};

	/*
		Function: OnPlayerLeave
			Called when a player leaves the game controlled by this controller.
			This is called before the player's character is killed.

		Arguments:
			pPlayer - The CPlayer that is leaving.
	*/
	virtual void OnPlayerLeave(class CPlayer *pPlayer) {};

	/*
		Function: OnPlayerChangeTeam
			Called when a player changed team (including joined spectators)

		Arguments:
			pPlayer - The CPlayer that changed team
			FromTeam - which team was the player in
			ToTeam - which team has the player changed to
	*/
	virtual void OnPlayerChangeTeam(class CPlayer *pPlayer, int FromTeam, int ToTeam) {};

	/*
		Function: OnPlayerTryRespawn
			Called when a player tries to respawn, when this called, the character hasn't spawn yet.
			You can call `pPlayer->CancelSpawn()` and return false to cancel respawn.
			You can also call `pPlayer->CancelSpawn()` and return true to disable player spawn after they dies.
			Or just return true to spawn the character normally

		Arguments:
			pPlayer - The CPlayer that is trying to respawn
			Pos - The position the player will spawn in
	*/
	virtual bool OnPlayerTryRespawn(class CPlayer *pPlayer, vec2 Pos) { return true; };

	/*
		Function: OnCharacterDeath
			Called when a CCharacter in the world dies.

		Arguments:
			pVictim - The CCharacter that died.
			pKiller - The player that killed it.
			Weapon - What weapon that killed it. Can be -1 for undefined
				weapon when switching team or player suicides.

		Return:
			A flag (DEATH_*) indicating the behaviour of character's death

	*/
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) { return DEATH_NORMAL; };

	/*
		Function: OnCharacterSpawn
			Called when a CCharacter spawns into the game world.

		Arguments:
			pChr - The CCharacter that was spawned.
	*/
	virtual void OnCharacterSpawn(class CCharacter *pChr) {};

	/*
		Function: OnCharacterTakeDamage
			Called when a CCharacter takes damage, including 0 damage friendly fire and self damage.

		Arguments:
			pChr - The CCharacter that was being damaged
			Force - Knockback force
					Force is a reference so you can change it.
			Dmg - Raw damage (will be already halved for self damage, and will be 0 for friendly fire)
					Dmg is a reference so you can change it.
			From - Attacker's ClientID
			WeaponType - Weapon's appearence
			WeaponID - Weapon class identifier
			IsExplosion - Whether the damage is inflicted by explosion

		Result:
			A flag (DAMAGE_*) indicating the behaviour of the damage
	*/
	virtual int OnCharacterTakeDamage(class CCharacter *pChr, vec2 &Force, int &Dmg, int From, int WeaponType, int WeaponID, bool IsExplosion) { return DAMAGE_NORMAL; };

	/*
		Function: OnCharacterTile
			Called when a CCharacter intersects with a tile.
			All tiles intersects the path between ticks will be handled
			Does not account for ProximityRadius.

		Arguments:
			pChr - The CCharacter that is touching the tile.
			MapIndex - Use GameServer()->Collison() to find more
				information about the tile.

		Return:
			bool - any internal handling of the tile will be skipped
				if set to true.
	*/
	virtual bool OnCharacterTile(class CCharacter *pChr, int MapIndex) { return false; };

	/*
		Function: OnCharacterProximateTile
			Called when a CCharacter proximate a tile.
			Only the discrete position is checked.
			Account for ProximityRadius, but the tile may be skipped due to high speed or ninja.

		Arguments:
			pChr - The CCharacter that is touching the tile.
			MapIndex - Use GameServer()->Collison() to find more
				information about the tile.

		Return:
			bool - any internal handling of the tile (speedup tiles) will be skipped
				if set to true
	*/
	virtual bool OnCharacterProximateTile(class CCharacter *pChr, int MapIndex) { return false; };

	/*
		Function: OnEntity
			Called when the map is loaded to process an entity
			in the map.

		Arguments:
			index - Entity index.
			pos - Where the entity is located in the world.

		Return:
			bool - any internal entity will be skipped if set to true.
	*/
	virtual bool OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number = 0) { return false; };

	/*
		Function: OnKill
			Called when the player used kill command
			The controller should handle the actual kill

		Arguments:
			Player - the killing player
	*/
	virtual void OnKill(class CPlayer *pPlayer);

public:
	// =================
	//   ENTITY EVENTS
	// =================
	/*
		Function: OnFlagReset
			Called when a CFlag resets it's position to its stand due to out of bounds

		Arguments:
			Flag - The CFlag that was reset.
	*/
	virtual void OnFlagReset(class CFlag *pFlag) {};

	/*
		Function: OnPickup
			Called when a CPickup interact with a character

		Arguments:
			Pickup - The CPickup that was being picked up
			Char - The character picking it up
			Sound - Set this variable to a sound enum if you want to play a sound

		Return:
			int - num of ticks to respawn this pickup, -1 = not disappear, -2 = destory it forever
	*/
	virtual int OnPickup(class CPickup *pPickup, class CCharacter *pChar, struct SPickupSound *pSound);

	// =================
	//   PLAYER STATES
	// =================
	/*
		Function: IsDisruptiveLeave
			Whether the player is disrupting the game by leaving

		Arguments:
			pPlayer - checking player

		Return:
			bool - player can't switch room or join spectator if set to true
				also, disconnected players' characters will not be
				killed until this check returns false.

		Note:
			This method will be called every tick
	*/
	virtual bool IsDisruptiveLeave(class CPlayer *pPlayer) const { return false; };

	// ============
	//    OTHERS
	// ============
	/*
		Function: GetTeamName
			Return the team name, will be used in joining message
				and team change message.

		Arguments:
			Team - which team

		Return:
			Team name
	*/
	virtual const char *GetTeamName(int Team);
};

#endif
