/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_TEAMS_H
#define GAME_SERVER_TEAMS_H

#include <engine/shared/config.h>
#include <game/teamscore.h>

#include <utility>
#include <vector>

struct SGameInstance
{
	bool m_Init;
	bool m_IsCreated;
	unsigned int m_Entities;
	class IGameController *m_pController;
	class CGameWorld *m_pWorld;
};

struct SGameType
{
	const char *pGameType;
	char *pName;
	char *pSettings;
	bool IsFile;
};

enum
{
	ENTITIES_PER_TICK = 25,
};

class CGameTeams
{
	SGameInstance m_aTeamInstances[MAX_CLIENTS];
	// TODO: team states is probably redundant
	int m_aTeamState[MAX_CLIENTS];
	bool m_aTeamLocked[MAX_CLIENTS];
	uint64 m_aInvited[MAX_CLIENTS];

	class CGameContext *m_pGameContext;

	// progressive
	struct SEntity
	{
		int Index;
		vec2 Pos;
		int Layer;
		int Flags;
		int Number;
	};
	std::vector<SEntity> m_Entities;

	// gametypes
	static std::vector<SGameType> m_GameTypes;
	static SGameType m_DefaultGameType;

public:
	enum
	{
		TEAMSTATE_EMPTY,
		TEAMSTATE_OPEN,
		TEAMSTATE_STARTED,
		TEAMSTATE_FINISHED
	};

	CTeamsCore m_Core;

	CGameTeams();
	~CGameTeams();

	void Init(class CGameContext *pGameServer);

	class CGameContext *GameServer() { return m_pGameContext; }

	// returns nullptr if successful, error string if failed
	const char *SetPlayerTeam(int ClientID, int Team, const char *pGameType);

	void ChangeTeamState(int Team, int State);

	int Count(int Team) const;

	// need to be very careful using this method. SERIOUSLY...
	enum
	{
		TEAM_REASON_NORMAL = 0,
		TEAM_REASON_CONNECT = 1,
		TEAM_REASON_DISCONNECT = 2,
		TEAM_REASON_FORCE = 3
	};
	bool SetForcePlayerTeam(int ClientID, int Team, int Reason, const char *pGameType = nullptr);

	void Reset();
	void ResetRoundState(int Team);
	void ResetSwitchers(int Team);

	void SendTeamsState(int ClientID);
	void SetTeamLock(int Team, bool Lock);
	void ResetInvited(int Team);
	void SetClientInvited(int Team, int ClientID, bool Invited);

	int GetTeamState(int Team);
	bool TeamLocked(int Team);
	bool IsInvited(int Team, int ClientID);
	int CanSwitchTeam(int ClientID);
	int FindAEmptyTeam();

	// Game Instances
	SGameInstance GetGameInstance(int Team);
	SGameInstance GetPlayerGameInstance(int ClientID);
	bool CreateGameInstance(int Team, const char *pGameName, int Asker);
	void DestroyGameInstance(int Team);
	void OnPlayerConnect(class CPlayer *pPlayer);
	void OnPlayerDisconnect(class CPlayer *pPlayer, const char *pReason);
	void OnTick();
	void OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number = 0);
	void OnSnap(int SnappingClient);
	void OnPostSnap();

	static void SetDefaultGameType(const char *pGameType, const char *pSettings, bool IsFile);
	static void AddGameType(const char *pGameType, const char *pName, const char *pSettings, bool IsFile);
	static void ClearGameTypes();
};

#endif
