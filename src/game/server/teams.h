/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_TEAMS_H
#define GAME_SERVER_TEAMS_H

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/teamscore.h>

#include <utility>
#include <vector>

struct SGameInstance
{
	bool m_IsCreated;
	IGameController *m_pController;
	CGameWorld *m_pWorld;
};

class CGameTeams
{
	SGameInstance m_aTeamInstances[MAX_CLIENTS];
	int m_aTeamState[MAX_CLIENTS];
	bool m_aTeamLocked[MAX_CLIENTS];
	uint64 m_aInvited[MAX_CLIENTS];

	class CGameContext *m_pGameContext;

	struct SEntity
	{
		int Index;
		vec2 Pos;
		int Layer;
		int Flags;
		int Number;
	};

	std::vector<SEntity> m_Entities;

public:
	enum
	{
		TEAMSTATE_EMPTY,
		TEAMSTATE_OPEN,
		TEAMSTATE_STARTED,
		TEAMSTATE_FINISHED
	};

	CTeamsCore m_Core;

	CGameTeams(CGameContext *pGameContext);
	~CGameTeams();

	// helper methods
	CCharacter *Character(int ClientID)
	{
		return GameServer()->GetPlayerChar(ClientID);
	}
	CPlayer *GetPlayer(int ClientID)
	{
		return GameServer()->m_apPlayers[ClientID];
	}

	class CGameContext *GameServer()
	{
		return m_pGameContext;
	}
	class IServer *Server()
	{
		return m_pGameContext->Server();
	}

	void OnCharacterSpawn(int ClientID);
	void OnCharacterDeath(int ClientID, int Weapon);

	// returns nullptr if successful, error string if failed
	const char *SetCharacterTeam(int ClientID, int Team);

	void ChangeTeamState(int Team, int State);

	int Count(int Team) const;

	// need to be very careful using this method. SERIOUSLY...
	void SetForceCharacterTeam(int ClientID, int Team);

	void Reset();
	void ResetRoundState(int Team);
	void ResetSwitchers(int Team);

	void SendTeamsState(int ClientID);
	void SetTeamLock(int Team, bool Lock);
	void ResetInvited(int Team);
	void SetClientInvited(int Team, int ClientID, bool Invited);

	int GetTeamState(int Team)
	{
		return m_aTeamState[Team];
	}

	bool TeamLocked(int Team)
	{
		if(Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
			return false;

		return m_aTeamLocked[Team];
	}

	bool IsInvited(int Team, int ClientID)
	{
		return m_aInvited[Team] & 1LL << ClientID;
	}

	// Game Instances
	SGameInstance GetGameInstance(int Team);
	SGameInstance GetPlayerGameInstance(int ClientID);
	void CreateGameInstance(int Team);
	void DestroyGameInstance(int Team);
	void OnTick();
	void OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number = 0);
	void OnSnap(int SnappingClient);
	void OnPostSnap();
};

#endif
