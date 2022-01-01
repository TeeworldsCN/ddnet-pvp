/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_PLAYER_H
#define GAME_SERVER_PLAYER_H


#include "alloc.h"

// this include should perhaps be removed
// #include "score.h"
#include "teams.h"
#include "teeinfo.h"
#include <game/server/gamecontext.h>

enum
{
	WEAPON_GAME = -3, // team switching etc
	WEAPON_SELF = -2, // console kill command
	WEAPON_WORLD = -1, // death tiles etc
};

enum
{
	SHOW_DISTANCE_DEFAULT_X = 1200,
	SHOW_DISTANCE_DEFAULT_Y = 800,
};

enum
{
    HUMAN = 1,
	ZOMBIE = 2,
};

// player object
class CPlayer
{
	MACRO_ALLOC_POOL_ID()

public:
	CPlayer(CGameContext *pGameServer, int ClientID, bool AsSpec);
	~CPlayer();

	bool m_IsZombie = false;
	int BDState;

	void Reset();
	void GameReset();

	bool IsSpawning() { return m_Spawning; };
	void TryRespawn();
	void Respawn();
	void CancelSpawn();
	CCharacter *ForceSpawn(vec2 Pos);
	void SetTeam(int Team);
	int GetTeam() const { return m_Team; };
	int GetCID() const { return m_ClientID; };
	int GetClientVersion() const;
	bool SetTimerType(int NewType);
	void SendCurrentTeamInfo();

	void Tick();
	void PostTick();

	void Snap(int SnappingClient);
	void FakeSnap();

	void OnDirectInput(CNetObj_PlayerInput *NewInput);
	void OnPredictedInput(CNetObj_PlayerInput *NewInput);
	void OnPredictedEarlyInput(CNetObj_PlayerInput *NewInput);
	void OnDisconnect();

	void KillCharacter(int Weapon = WEAPON_GAME);
	CCharacter *GetCharacter();

	void SpectatePlayerName(const char *pName);
	bool IsSpectating() { return m_Team == TEAM_SPECTATORS || m_Paused; }

	//---------------------------------------------------------
	// this is used for snapping so we know how we can clip the view for the player
	vec2 m_ViewPos;
	int m_TuneZone;
	int m_TuneZoneOld;

	// states if the client is chatting, accessing a menu etc.
	int m_PlayerFlags;

	// used for snapping to just update latency if the scoreboard is active
	int m_aActLatency[MAX_CLIENTS];

	// used for spectator mode
	enum ESpecMode
	{
		FREEVIEW = 0,
		PLAYER,
		FLAGRED, // SIXUP only
		FLAGBLUE, // SIXUP only
		NUM_SPECMODES
	};
	int GetSpectatorID() const { return m_SpectatorID; }
	bool SetSpecMode(ESpecMode SpecMode, int SpectatorID = -1);
	bool m_DeadSpecMode;
	bool DeadCanFollow(CPlayer *pPlayer) const;
	void UpdateDeadSpecMode();

	bool m_IsReadyToEnter;
	bool m_IsReadyToPlay;

	bool m_RespawnDisabled;

	bool m_LastFire;

	//
	int m_Vote;
	int m_VotePos;
	//
	int m_LastVoteCall;
	int m_LastVoteTry;
	int m_LastChat;
	int m_LastSetTeam;
	int m_LastSetSpectatorMode;
	int m_LastChangeInfo;
	int m_LastEmote;
	int m_LastKill;
	int m_LastCommands[4];
	int m_LastCommandPos;
	int m_LastWhisperTo;
	int m_LastInvited;

	int m_SendVoteIndex;

	CTeeInfo m_TeeInfos;

	int m_RespawnTick;
	int m_DieTick;
	int m_PreviousDieTick;
	int m_Score;
	int m_ScoreStartTick;
	int m_JoinTick;
	bool m_ForceBalanced;
	int m_LastActionTick;
	int m_TeamChangeTick;
	bool m_SentSemicolonTip;
	int m_InactivityTickCounter;
	struct
	{
		int m_TargetX;
		int m_TargetY;
	} m_LatestActivity;

	// network latency calculations
	struct
	{
		int m_Accum;
		int m_AccumMin;
		int m_AccumMax;
		int m_Avg;
		int m_Min;
		int m_Max;
	} m_Latency;

private:
	CCharacter *m_pCharacter;
	int m_NumInputs;
	CGameContext *m_pGameServer;

	int DDRTeam() const { return m_pGameServer->GetPlayerDDRTeam(m_ClientID); }
	CGameContext *GameServer() const { return m_pGameServer; }
	CGameWorld *GameWorld() const
	{
		SGameInstance Instance = m_pGameServer->GameInstance(DDRTeam());
		if(!Instance.m_Init)
			return nullptr;
		return Instance.m_pWorld;
	}
	IGameController *Controller() const
	{
		SGameInstance Instance = m_pGameServer->GameInstance(DDRTeam());
		if(!Instance.m_Init)
			return nullptr;
		return Instance.m_pController;
	}
	IServer *Server() const;

	//
	bool m_Spawning;
	int m_ClientID;
	int m_Team;

	bool m_Paused;
	int64 m_LastPause;

	int m_DefEmote;
	int m_OverrideEmote;
	int m_OverrideEmoteReset;
	bool m_Halloween;

	ESpecMode m_SpecMode;
	int m_SpectatorID;
	void SetSpectatorID(int ClientID);

public:
	enum
	{
		TIMERTYPE_DEFAULT = -1,
		TIMERTYPE_GAMETIMER = 0,
		TIMERTYPE_BROADCAST,
		TIMERTYPE_GAMETIMER_AND_BROADCAST,
		TIMERTYPE_SIXUP,
		TIMERTYPE_NONE,
	};

	enum
	{
		SHOWOTHERS_OFF = 0,
		SHOWOTHERS_ON = 1,
		SHOWOTHERS_DISTRACTING = 2,
	};

	bool m_DND;
	int64 m_FirstVoteTick;
	char m_TimeoutCode[64];

	void ProcessPause();
	int Pause(bool Paused, bool Force);
	bool IsPaused();

	bool IsPlaying();
	int64 m_LastKickVote;
	int64 m_LastRoomChange;
	int64 m_LastRoomCreation;
	int64 m_LastRoomInfoChange;
	int64 m_LastReadyChangeTick;
	int m_ShowOthers;
	int ShowOthersMode();
	vec2 m_ShowDistance;
	bool m_SpecTeam;
	bool m_Afk;
	int m_PauseCount;

	int m_ChatScore;

	bool m_Moderating;

	bool AfkTimer(int new_target_x, int new_target_y); //returns true if kicked
	void UpdatePlaytime();
	void AfkVoteTimer(CNetObj_PlayerInput *NewTarget);
	int64 m_LastPlaytime;
	int64 m_LastEyeEmote;
	int64 m_LastBroadcast;
	bool m_LastBroadcastImportance;
	int m_LastTarget_x;
	int m_LastTarget_y;
	CNetObj_PlayerInput *m_pLastTarget;
	int m_Sent1stAfkWarning; // afk timer's 1st warning after 50% of sv_max_afk_time
	int m_Sent2ndAfkWarning; // afk timer's 2nd warning after 90% of sv_max_afk_time
	char m_pAfkMsg[160];
	bool m_EyeEmoteEnabled;
	int m_TimerType;

	int GetDefaultEmote() const;
	void OverrideDefaultEmote(int Emote, int Tick);
	bool CanOverrideDefaultEmote() const;

	bool m_FirstPacket;
	bool m_NotEligibleForFinish;
	int64 m_EligibleForFinishCheck;

	class CFlag *m_pSpecFlag;
	bool m_ActiveSpecSwitch;
};

#endif
