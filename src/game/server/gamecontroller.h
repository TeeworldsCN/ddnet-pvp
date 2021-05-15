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
		CIntVariableData *pInt = new CIntVariableData({InstanceConsole(), Pointer, Min, Max, Default}); \
		m_IntConfigStore.push_back(pInt); \
		InstanceConsole()->Register(Command, "?i[value]", Flag, IGameController::IntVariableCommand, pInt, Desc); \
	}

#define INSTANCE_CONFIG_STR(Pointer, Command, Default, Flag, Desc) \
	{ \
		str_copy(Pointer, Default, sizeof(Pointer)); \
		CStrVariableData *pStr = new CStrVariableData({InstanceConsole(), Pointer, sizeof(Pointer), nullptr}); \
		m_StrConfigStore.push_back(pStr); \
		InstanceConsole()->Register(Command, "?r[value]", Flag, IGameController::StrVariableCommand, pStr, Desc); \
	}

/*
	Class: Game Controller
		Controls the main game logic. Keeping track of team and player score,
		winning conditions and specific game logic.
*/
class IGameController
{
protected:
	struct SFlagState
	{
		int m_RedFlagDroppedTick;
		int m_RedFlagCarrier;
		int m_BlueFlagDroppedTick;
		int m_BlueFlagCarrier;
	};

	// GameController Interface

	// =================
	//    GAME STATES
	// =================
	/*
		Function: CanBeMovedOnBalance
			Whether a player can be moved to the other team when a team balancing is taking place

		Arguments:
			ClientID - which player is being evaluated.

		Return:
			return true if the player can be killed and moved to the other team

		Note:
			Team balancing will always try to match both team's player score.
			If you don't want this, you should override DoTeamBalance instead.
	*/
	virtual bool CanBeMovedOnBalance(int ClientID) const { return true; };

	/*
		Function: AreTeamsUnbalanced
			Whether the teams are balanced and should call DoTeamBalance

		Return:
			return true if the teams need balancing
	*/
	virtual bool AreTeamsUnbalanced() const { return absolute(m_aTeamSize[TEAM_RED] - m_aTeamSize[TEAM_BLUE]) >= 2; };

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
			Called after gamestate is updated and before OnPostTick
				when the gamestate is IGS_GAME_RUNNING or IGS_GAME_PAUSED
		
		Return:
			true if the match should be ended.

		Note:
			The result allows you to skip extra logics during a tick,
				but it won't end the game for you.
				You should call EndMatch() in this function to end the match.
	*/
	virtual bool DoWincheckRound();

	/*
		Function: DoWincheckMatch
			Called after gamestate is updated and before OnPostTick
				when the gamestate is IGS_GAME_RUNNING or IGS_GAME_PAUSED
				AND when DoWincheckRound is true
		
		Return:
			true if the match should be ended.

		Note:
			The result allows you to skip extra logics during a tick,
				but it won't end the game for you.
				You should call EndMatch() in this function to end the match.
	*/
	virtual bool DoWincheckMatch();

	/*
		Function: GetFlagState
			Called during Snap() to send flag information to clients
		
		Return:
			true if the game wants to send flag info.
	*/
	virtual bool GetFlagState(SFlagState *pState) { return false; };

	virtual bool IsSpawnRandom() const { return IsSurvival(); };
	virtual bool ShouldSpawnAwayFromCharacter(class CCharacter *pChar) const { return true; };

	// =============
	//   GAME CORE
	// =============
	/*
		Function: OnPreTick
			Called before the gamestate is updated during a Tick
	*/
	virtual void OnPreTick(){};

	/*
		Function: OnPostTick
			Called after the gamestate is updated during a Tick
	*/
	virtual void OnPostTick(){};

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
	virtual void OnInit(){};
	/*
		Function: OnControllerStart()
			Called when the controller and its world are fully prepared.
			When this is called, the controllers gamestate is properly set.
	*/
	virtual void OnControllerStart(){};
	/*
		Function: OnPlayerJoin
			Called when a player joins the game controlled by this controller.
			This is called before the player's character is spawned.

		Arguments:
			pPlayer - The CPlayer that is joining.
	*/
	virtual void OnPlayerJoin(class CPlayer *pPlayer){};

	/*
		Function: OnPlayerLeave
			Called when a player leaves the game controlled by this controller.
			This is called before the player's character is killed.

		Arguments:
			pPlayer - The CPlayer that is leaving.
	*/
	virtual void OnPlayerLeave(class CPlayer *pPlayer){};
	/*
		Function: OnCharacterDeath
			Called when a CCharacter in the world dies.

		Arguments:
			pVictim - The CCharacter that died.
			pKiller - The player that killed it.
			Weapon - What weapon that killed it. Can be -1 for undefined
				weapon when switching team or player suicides.

		Return:
			A flag indicating the behaviour of character's death

	*/
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) { return DEATH_NORMAL; };
	/*
		Function: OnCharacterSpawn
			Called when a CCharacter spawns into the game world.

		Arguments:
			pChr - The CCharacter that was spawned.
	*/
	virtual void OnCharacterSpawn(class CCharacter *pChr){};

	/*
		Function: OnCharacterTile
			Called when a CCharacter intersects with a tile.

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

public:
	// =================
	//   ENTITY EVENTS
	// =================
	/*
		Function: OnFlagReset
			Called when a CFlag resets it's position to its stand due to out of bounds

		Arguments:
			pFlag - The CFlag that was reset.
	*/
	virtual void OnFlagReset(class CFlag *pFlag){};

private:
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
		int m_NextBroadcastTick;
	};
	SBroadcastState m_aFakeClientBroadcast[MAX_CLIENTS];
	void FakeClientBroadcast(int SnappingClient);

protected:
	// balancing
	enum
	{
		TBALANCE_CHECK = -2,
		TBALANCE_OK,
	};
	int m_aTeamSize[2];
	int m_UnbalancedTick;

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

	bool HasEnoughPlayers() const { return (IsTeamplay() && m_aTeamSize[TEAM_RED] > 0 && m_aTeamSize[TEAM_BLUE] > 0) || (!IsTeamplay() && m_aTeamSize[TEAM_RED] > 1); }
	void ResetGame();
	void SetGameState(EGameState GameState, int Timer = 0);
	void StartMatch();
	void StartRound();

	struct CSpawnEval
	{
		CSpawnEval()
		{
			m_Got = false;
			m_FriendlyTeam = -1;
			m_Pos = vec2(100, 100);
		}

		vec2 m_Pos;
		bool m_Got;
		bool m_RandomSpawn;
		int m_FriendlyTeam;
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

protected:
	// config variables
	std::vector<CIntVariableData *> m_IntConfigStore;
	std::vector<CStrVariableData *> m_StrConfigStore;

	// game
	int m_GameStartTick;
	int m_MatchCount;
	int m_RoundCount;
	int m_SuddenDeath;
	int m_aTeamscore[2];

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
		DEATH_KEEP_SOLO = 16
	};

	// internal game flag (for sixup)
	enum
	{
		IGF_TEAMS = 1,
		IGF_FLAGS = 2,
		IGF_SURVIVAL = 4,
		IGF_RACE = 8
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
	int m_DDNetServerCapability;

public:
	IGameController();
	virtual ~IGameController();

	CGameContext *GameServer() const { return m_pGameServer; }
	CConfig *Config() const { return m_pConfig; }
	IServer *Server() const { return m_pServer; }
	CGameWorld *GameWorld() const { return m_pWorld; }
	IConsole *InstanceConsole() const { return m_pInstanceConsole; }
	int m_ChatResponseTargetID;

	// common config
	int m_Warmup;
	int m_Countdown;
	int m_Teamdamage;
	int m_RoundSwap;
	int m_MatchSwap;
	int m_Powerups;
	int m_Scorelimit;
	int m_Timelimit;
	int m_Roundlimit;
	int m_TeambalanceTime;
	char m_aMap[128];
	int m_MapIndex;

	// internal events
	void StartController();
	void OnInternalPlayerJoin(class CPlayer *pPlayer, bool ServerJoin, bool Creating);
	void OnInternalPlayerLeave(class CPlayer *pPlayer, bool ServerLeave);
	int OnInternalCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	void OnInternalCharacterSpawn(class CCharacter *pChr);
	bool OnInternalCharacterTile(class CCharacter *pChr, int MapIndex);
	void OnInternalEntity(int Index, vec2 Pos, int Layer, int Flags, int MegaMapIndex, int Number);

	void OnReset();

	// game
	enum
	{
		TIMER_INFINITE = -1,
		TIMER_END = 10,
	};

	void DoPause(int Seconds) { SetGameState(IGS_GAME_PAUSED, Seconds); }
	void DoWarmup(int Seconds) { SetGameState(IGS_WARMUP_USER, Seconds); }
	void AbortWarmup()
	{
		if((m_GameState == IGS_WARMUP_GAME || m_GameState == IGS_WARMUP_USER) && m_GameStateTimer != TIMER_INFINITE)
		{
			SetGameState(IGS_GAME_RUNNING);
		}
	}
	void SwapTeamscore();
	void EndMatch() { SetGameState(IGS_END_MATCH, TIMER_END); }
	void EndRound() { SetGameState(IGS_END_ROUND, TIMER_END / 2); }

	// general
	virtual void Snap(int SnappingClient);
	virtual void Tick();
	void UpdateGameInfo(int ClientID);

	/*
		Function: OnKill
			Called when the player used kill command
			The controller should handle the actual kill

		Arguments:
			ClientID - player's cid
	*/
	virtual void OnKill(int ClientID) const {};

	/*
		Function: IsDisruptiveLeave
			Whether the player is disrupting the game by leaving
		
		Arguments:
			ClientID = player's cid
		
		return:
			bool - player can't switch room if set to true
				also, disconnected players' characters will not be
				killed until this check returns false.
	*/
	virtual bool IsDisruptiveLeave(int ClientID) const { return false; };

	// info
	void CheckGameInfo();
	bool IsFriendlyFire(int ClientID1, int ClientID2) const;
	bool IsFriendlyTeamFire(int Team1, int Team2) const;
	bool IsGamePaused() const { return m_GameState == IGS_GAME_PAUSED || m_GameState == IGS_START_COUNTDOWN; }
	bool IsGameRunning() const { return m_GameState == IGS_GAME_RUNNING; }
	bool IsPlayerReadyMode() const;
	bool IsTeamChangeAllowed() const;
	bool IsTeamplay() const { return m_GameFlags & IGF_TEAMS; }
	bool IsSurvival() const { return m_GameFlags & IGF_SURVIVAL; }
	void SendGameMsg(int GameMsgID, int ClientID, int *i1 = nullptr, int *i2 = nullptr, int *i3 = nullptr);

	const char *GetGameType() const { return m_pGameType; }
	int IsEndRound() const { return m_GameState == IGS_END_ROUND; }
	int IsEndMatch() const { return m_GameState == IGS_END_MATCH; }

	// spawn
	bool CanSpawn(int Team, vec2 *pPos) const;
	bool GetStartRespawnState() const;

	// team
	bool CanJoinTeam(int Team, int ClientID, bool SendReason) const;
	virtual bool CanChangeTeam(CPlayer *pPlayer, int JoinTeam) const;

	void DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg = true);
	void ForceTeamBalance()
	{
		if(!(m_GameFlags & IGF_SURVIVAL))
			DoTeamBalance();
	}

	int GetRealPlayerNum() const { return m_aTeamSize[TEAM_RED] + m_aTeamSize[TEAM_BLUE]; }
	int GetStartTeam();
	virtual const char *GetTeamName(int Team);

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

	// Instance Config
	static void InstanceConsolePrint(const char *pStr, void *pUser, ColorRGBA PrintColor);
	static void ConchainReplyOnly(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void IntVariableCommand(IConsole::IResult *pResult, void *pUserData);
	static void ColVariableCommand(IConsole::IResult *pResult, void *pUserData);
	static void StrVariableCommand(IConsole::IResult *pResult, void *pUserData);
};

#endif
