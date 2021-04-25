/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTROLLER_H
#define GAME_SERVER_GAMECONTROLLER_H

#include <base/vmath.h>
#include <engine/map.h>
#include <game/generated/protocol.h>

#include <map>
#include <vector>

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

	void DoActivityCheck();
	bool GetPlayersReadyState(int WithoutID = -1);
	void SetPlayersReadyState(bool ReadyState);
	void CheckReadyStates(int WithoutID = -1);

	// balancing
	enum
	{
		TBALANCE_CHECK = -2,
		TBALANCE_OK,
	};
	int m_aTeamSize[2];
	int m_UnbalancedTick;

	virtual bool CanBeMovedOnBalance(int ClientID) const;
	void CheckTeamBalance();
	void DoTeamBalance();

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

	virtual bool DoWincheckMatch(); // returns true when the match is over
	virtual bool DoWincheckRound(); // returns true when the round is over
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

protected:
	CGameContext *GameServer() const { return m_pGameServer; }
	CConfig *Config() const { return m_pConfig; }
	IServer *Server() const { return m_pServer; }
	CGameWorld *GameWorld() const { return m_pWorld; }

	// game
	int m_GameStartTick;
	int m_MatchCount;
	int m_RoundCount;
	int m_SuddenDeath;
	int m_aTeamscore[2];

	struct SFlagState
	{
		int m_RedFlagDroppedTick;
		int m_RedFlagCarrier;
		int m_BlueFlagDroppedTick;
		int m_BlueFlagCarrier;
	};
	virtual bool GetFlagState(SFlagState *pState);
	void EndMatch() { SetGameState(IGS_END_MATCH, TIMER_END); }
	void EndRound() { SetGameState(IGS_END_ROUND, TIMER_END / 2); }

	enum
	{
		DEATH_NORMAL = 0,
		DEATH_VICTIM_HAS_FLAG = 1,
		DEATH_KILLER_HAS_FLAG = 2,
		DEATH_SKIP_SCORE = 4,
		DEATH_NO_SUICIDE_PANATY = 8,

		// be careful when using this
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

	void UpdateGameInfo(int ClientID);

	// event
	/*
		Function: OnPlayerJoin
			Called when a player joins the game controlled by this controller.
			This is called before the player's character is spawned.

		Arguments:
			pPlayer - The CPlayer that is joining.
	*/
	virtual void OnPlayerJoin(class CPlayer *pPlayer);

	/*
		Function: OnPlayerLeave
			Called when a player leaves the game controlled by this controller.
			This is called before the player's character is killed.

		Arguments:
			pPlayer - The CPlayer that is leaving.
	*/
	virtual void OnPlayerLeave(class CPlayer *pPlayer);
	/*
		Function: OnCharacterDeath
			Called when a CCharacter in the world dies.

		Arguments:
			pVictim - The CCharacter that died.
			pKiller - The player that killed it.
			Weapon - What weapon that killed it. Can be -1 for undefined
				weapon when switching team or player suicides.
	*/
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	/*
		Function: OnCharacterSpawn
			Called when a CCharacter spawns into the game world.

		Arguments:
			pChr - The CCharacter that was spawned.
	*/
	virtual void OnCharacterSpawn(class CCharacter *pChr);

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
	virtual bool OnCharacterTile(class CCharacter *pChr, int MapIndex);

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
	virtual bool OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number = 0);

public:
	IGameController();
	virtual ~IGameController();

	// events
	/*
		Function: OnFlagReset
			Called when a CFlag resets it's position its stand.

		Arguments:
			pFlag - The CFlag that was reset.
	*/
	virtual void OnFlagReset(class CFlag *pFlag);

	// internal events
	void OnInternalPlayerJoin(class CPlayer *pPlayer);
	void OnInternalPlayerLeave(class CPlayer *pPlayer);
	int OnInternalCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	void OnInternalCharacterSpawn(class CCharacter *pChr);
	bool OnInternalCharacterTile(class CCharacter *pChr, int MapIndex);
	void OnInternalEntity(int Index, vec2 Pos, int Layer, int Flags, int Number = 0);

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

	// general
	virtual void Snap(int SnappingClient);
	virtual void Tick();

	/*
		Function: CanKill
			Whether the player can use kill command.

		Arguments:
			ClientID - player's cid

		Return:
			bool - player can use kill command if set to true.
	*/
	virtual bool CanKill(int ClientID) const;

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
	virtual bool IsDisruptiveLeave(int ClientID) const;

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

	//spawn
	bool CanSpawn(int Team, vec2 *pPos) const;
	bool GetStartRespawnState() const;

	// team
	bool CanJoinTeam(int Team, int NotThisID) const;
	bool CanChangeTeam(CPlayer *pPplayer, int JoinTeam) const;

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
	bool IsPlayerInRoom(int ClientID) const;
	void InitController(int Team, class CGameContext *pGameServer, class CGameWorld *pWorld);
};

#endif
