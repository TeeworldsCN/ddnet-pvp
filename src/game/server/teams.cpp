/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "teams.h"
#include "teehistorian.h"
#include <engine/shared/config.h>
#include <game/version.h>

#include "entities/character.h"
#include "player.h"

#include "gamemodes.h"
#include "gamemodes/dm.h"

CGameTeams::CGameTeams()
{
	m_pGameContext = nullptr;
	mem_zero(m_aTeamInstances, sizeof(m_aTeamInstances));
	mem_zero(m_apWantedGameType, sizeof(m_apWantedGameType));
	mem_zero(m_aTeamReload, sizeof(m_aTeamReload));
	mem_zero(m_aTeamMapIndex, sizeof(m_aTeamMapIndex));
	mem_zero(m_aRoomVotes, sizeof(m_aRoomVotes));
	mem_zero(m_aTeamState, sizeof(m_aTeamState));
	mem_zero(m_aTeamLocked, sizeof(m_aTeamLocked));
	mem_zero(m_aInvited, sizeof(m_aInvited));
	m_NumRooms = 0;
}

CGameTeams::~CGameTeams()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
		DestroyGameInstance(i);
}

void CGameTeams::Init(CGameContext *pGameServer)
{
	m_pGameContext = pGameServer;
	CreateGameInstance(0, nullptr, -1);
}

void CGameTeams::Reset()
{
	m_Core.Reset();
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		DestroyGameInstance(i);
		m_aTeamState[i] = TEAMSTATE_EMPTY;
		m_aTeamLocked[i] = false;
		m_aInvited[i] = 0;
	}
}

void CGameTeams::ResetRoundState(int Team)
{
	ResetInvited(Team);
	ResetSwitchers(Team);
}

void CGameTeams::ResetSwitchers(int Team)
{
	if(GameServer()->Collision()->m_NumSwitchers > 0)
	{
		for(int i = 0; i < GameServer()->Collision()->m_NumSwitchers + 1; ++i)
		{
			GameServer()->Collision()->m_pSwitchers[i].m_Status[Team] = GameServer()->Collision()->m_pSwitchers[i].m_Initial;
			GameServer()->Collision()->m_pSwitchers[i].m_EndTick[Team] = 0;
			GameServer()->Collision()->m_pSwitchers[i].m_Type[Team] = TILE_SWITCHOPEN;
		}
	}
}

const char *CGameTeams::SetPlayerTeam(int ClientID, int Team, const char *pGameType)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return "Invalid client ID";
	if(Team < 0 || Team >= MAX_CLIENTS + 1)
		return "Invalid room number";
	if(Team != TEAM_SUPER && m_aTeamState[Team] > TEAMSTATE_OPEN)
		return "This room started already";
	if(m_Core.Team(ClientID) == Team)
		return "You are in this room already";

	CCharacter *pChar = GameServer()->GetPlayerChar(ClientID);
	if(pChar)
	{
		if(Team == TEAM_SUPER && !pChar->m_Super)
			return "You can't join super room if you don't have super rights";
		if(Team != TEAM_SUPER && pChar->m_DDRaceState != DDRACE_NONE)
			return "You have started the game already";
	}

	if(!SetForcePlayerTeam(ClientID, Team, TEAM_REASON_NORMAL, pGameType))
		return "You need to specify a game type to create a room";

	return nullptr;
}

bool CGameTeams::SetForcePlayerTeam(int ClientID, int Team, int State, const char *pGameType)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	if(!pPlayer)
		return false;

	int OldTeam = m_Core.Team(ClientID);

	if(OldTeam != Team && !m_aTeamInstances[Team].m_IsCreated)
	{
		if(!pGameType && State != TEAM_REASON_FORCE && m_GameTypes.size() > 1)
			return false;

		if(!CreateGameInstance(Team, pGameType, ClientID))
			return false;
	}

	if(Team != OldTeam && OldTeam != TEAM_SUPER && m_aTeamState[OldTeam] != TEAMSTATE_EMPTY)
	{
		if(State == TEAM_REASON_NORMAL && m_aTeamInstances[OldTeam].m_IsCreated)
			m_aTeamInstances[OldTeam].m_pController->OnInternalPlayerLeave(GameServer()->m_apPlayers[ClientID], false);
		if(Count(OldTeam) <= 1 && OldTeam != TEAM_FLOCK)
		{
			m_aTeamState[OldTeam] = TEAMSTATE_EMPTY;

			// unlock team when last player leaves
			SetTeamLock(OldTeam, false);
			ResetRoundState(OldTeam);
			DestroyGameInstance(OldTeam);
		}
	}

	switch(State)
	{
	case TEAM_REASON_DISCONNECT:
		m_Core.Leave(ClientID);
		break;
	case TEAM_REASON_CONNECT:
		m_Core.Join(ClientID, Team);
		break;
	default:
		m_Core.Team(ClientID, Team);
	}

	if(OldTeam != Team)
	{
		if(State != TEAM_REASON_DISCONNECT)
			m_aTeamInstances[Team].m_pController->OnInternalPlayerJoin(GameServer()->m_apPlayers[ClientID], false, false);

		for(int LoopClientID = 0; LoopClientID < MAX_CLIENTS; ++LoopClientID)
			if(GameServer()->IsPlayerValid(LoopClientID))
				SendTeamsState(LoopClientID);
	}

	int TeamOldState = m_aTeamState[Team];

	if(Team != TEAM_SUPER && (TeamOldState == TEAMSTATE_EMPTY || m_aTeamLocked[Team]))
	{
		if(!m_aTeamLocked[Team])
			ChangeTeamState(Team, TEAMSTATE_OPEN);

		ResetSwitchers(Team);
	}

	UpdateVotes();
	return true;
}

int CGameTeams::Count(int Team) const
{
	return m_Core.Count(Team);
}

void CGameTeams::ChangeTeamState(int Team, int State)
{
	m_aTeamState[Team] = State;
}

void CGameTeams::SendTeamsState(int ClientID)
{
	if(!m_pGameContext->m_apPlayers[ClientID] || m_pGameContext->m_apPlayers[ClientID]->GetClientVersion() <= VERSION_DDRACE)
		return;

	CMsgPacker Msg(NETMSGTYPE_SV_TEAMSSTATE);

	for(unsigned i = 0; i < MAX_CLIENTS; i++)
		Msg.AddInt(m_Core.Team(i));

	GameServer()->Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameTeams::SetTeamLock(int Team, bool Lock)
{
	if(Team > TEAM_FLOCK && Team < TEAM_SUPER)
		m_aTeamLocked[Team] = Lock;
}

void CGameTeams::ResetInvited(int Team)
{
	m_aInvited[Team] = 0;
}

void CGameTeams::SetClientInvited(int Team, int ClientID, bool Invited)
{
	if(Team > TEAM_FLOCK && Team < TEAM_SUPER)
	{
		if(Invited)
			m_aInvited[Team] |= 1ULL << ClientID;
		else
			m_aInvited[Team] &= ~(1ULL << ClientID);
	}
}

SGameInstance CGameTeams::GetGameInstance(int Team)
{
	return m_aTeamInstances[Team];
}

SGameInstance CGameTeams::GetPlayerGameInstance(int ClientID)
{
	return m_aTeamInstances[m_Core.Team(ClientID)];
}

void CGameTeams::ReloadGameInstance(int Team)
{
	if(!m_aTeamInstances[Team].m_IsCreated)
		return;

	m_aTeamReload[Team] = RELOAD_TYPE_SOFT;
}

bool CGameTeams::CreateGameInstance(int Team, const char *pGameName, int Asker)
{
	SGameType Type;
	Type.IsFile = false;
	Type.pGameType = nullptr;
	Type.pName = nullptr;
	Type.pSettings = nullptr;

	if(pGameName == nullptr)
	{
		if(!m_DefaultGameType.pGameType)
		{
			Type.pGameType = "dm";
			Type.pSettings = nullptr;
		}
		else
		{
			Type = m_DefaultGameType;
		}
	}
	else
		for(auto GameType : m_GameTypes)
			if(str_comp_nocase(GameType.pName, pGameName) == 0)
				Type = GameType;

	if(!Type.pGameType)
		return false;

	m_apWantedGameType[Team] = Type.pName;

	if(m_aTeamInstances[Team].m_IsCreated)
		DestroyGameInstance(Team);

	IGameController *Game = nullptr;
	if(false)
		return false;
#define REGISTER_GAME_TYPE(TYPE, CLASS) \
	else if(str_comp_nocase(#TYPE, Type.pGameType) == 0) \
		Game = new CLASS();
#include "gamemodes.h"
#undef REGISTER_GAME_TYPE
	else
	{
		Game = new CGameControllerDM();
		Type.pSettings = nullptr;
	}

	CGameWorld *pWorld = new CGameWorld(Team, m_pGameContext);
	m_aTeamInstances[Team].m_pWorld = pWorld;
	m_aTeamInstances[Team].m_pController = Game;
	m_aTeamInstances[Team].m_pController->m_MapIndex = m_NumMaps > 0 ? 1 : 0;
	m_aTeamInstances[Team].m_pController->InitController(m_pGameContext, pWorld);
	m_aTeamInstances[Team].m_IsCreated = true;
	m_aTeamInstances[Team].m_Init = false;
	m_aTeamInstances[Team].m_Entities = 0;
	m_aTeamInstances[Team].m_Creator = Asker;

	if(Type.pSettings && Type.pSettings[0])
	{
		if(Type.IsFile)
			m_aTeamInstances[Team].m_pController->InstanceConsole()->ExecuteFile(Type.pSettings);
		else
			m_aTeamInstances[Team].m_pController->InstanceConsole()->ExecuteLine(Type.pSettings);
	}

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "game controller %d is created", Team);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "team", aBuf);

	for(int i = 0; i < MAX_CLIENTS; i++)
		if(GameServer()->IsPlayerValid(i) && m_Core.Team(i) == Team)
			m_aTeamInstances[Team].m_pController->OnInternalPlayerJoin(GameServer()->m_apPlayers[i], false, false);

	return true;
}

bool CGameTeams::RecreateGameInstance(int Team, const char *pGameName)
{
	SGameType Type;
	Type.IsFile = false;
	Type.pGameType = nullptr;
	Type.pName = nullptr;
	Type.pSettings = nullptr;

	if(pGameName == nullptr)
	{
		if(!m_DefaultGameType.pGameType)
		{
			Type.pGameType = "dm";
			Type.pSettings = nullptr;
			Type.pName = nullptr;
		}
		else
		{
			Type = m_DefaultGameType;
		}
	}
	else
		for(auto GameType : m_GameTypes)
			if(str_comp_nocase(GameType.pName, pGameName) == 0)
				Type = GameType;

	if(!Type.pGameType)
		return false;

	m_apWantedGameType[Team] = Type.pName;
	m_aTeamReload[Team] = RELOAD_TYPE_HARD;
	return true;
}

void CGameTeams::DestroyGameInstance(int Team)
{
	if(!m_aTeamInstances[Team].m_IsCreated)
		return;

	for(int i = 0; i < MAX_CLIENTS; i++)
		if(GameServer()->IsPlayerValid(i) && m_Core.Team(i) == Team)
			GameServer()->m_apPlayers[i]->KillCharacter();

	delete m_aTeamInstances[Team].m_pController;
	delete m_aTeamInstances[Team].m_pWorld;
	m_aTeamInstances[Team].m_Init = false;
	m_aTeamInstances[Team].m_IsCreated = false;
	m_aTeamInstances[Team].m_pController = nullptr;
	m_aTeamInstances[Team].m_pWorld = nullptr;
	m_aTeamInstances[Team].m_Entities = 0;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "game controller %d is deleted", Team);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "team", aBuf);
}

void CGameTeams::OnPlayerConnect(CPlayer *pPlayer)
{
	int ClientID = pPlayer->GetCID();

	SetForcePlayerTeam(ClientID, 0, TEAM_REASON_CONNECT);

	if(m_aTeamInstances[m_Core.Team(ClientID)].m_IsCreated)
		m_aTeamInstances[m_Core.Team(ClientID)].m_pController->OnInternalPlayerJoin(pPlayer, true, false);

	if(!GameServer()->Server()->ClientPrevIngame(ClientID))
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' team=%d", ClientID, GameServer()->Server()->ClientName(ClientID), pPlayer->GetTeam());
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

		GameServer()->SendChatTarget(ClientID, "DDNet PvP Mod. Version: " GAME_VERSION);
		GameServer()->SendChatTarget(ClientID, "say /info for detail and make sure to read our /rules");
	}
}

void CGameTeams::OnPlayerDisconnect(CPlayer *pPlayer, const char *pReason)
{
	int ClientID = pPlayer->GetCID();
	bool WasModerator = pPlayer->m_Moderating && GameServer()->Server()->ClientIngame(ClientID);

	if(m_aTeamInstances[m_Core.Team(ClientID)].m_IsCreated)
		m_aTeamInstances[m_Core.Team(ClientID)].m_pController->OnInternalPlayerLeave(pPlayer, true);

	pPlayer->OnDisconnect();
	if(GameServer()->Server()->ClientIngame(ClientID))
	{
		char aBuf[512];
		if(pReason && *pReason)
			str_format(aBuf, sizeof(aBuf), "'%s' has left the game (%s)", GameServer()->Server()->ClientName(ClientID), pReason);
		else
			str_format(aBuf, sizeof(aBuf), "'%s' has left the game", GameServer()->Server()->ClientName(ClientID));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, -1, CGameContext::CHAT_SIX);

		str_format(aBuf, sizeof(aBuf), "leave player='%d:%s'", ClientID, GameServer()->Server()->ClientName(ClientID));
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);
	}

	if(!GameServer()->PlayerModerating() && WasModerator)
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, "Server kick/spec votes are no longer actively moderated.");

	SetForcePlayerTeam(ClientID, TEAM_FLOCK, TEAM_REASON_DISCONNECT);
}

void CGameTeams::OnTick()
{
	bool NeedToProcessEntities = false;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_aTeamInstances[i].m_Init)
		{
			if(m_aTeamReload[i] == RELOAD_TYPE_HARD)
			{
				m_aTeamReload[i] = RELOAD_TYPE_NO;
				CreateGameInstance(i, m_apWantedGameType[i], m_aTeamInstances[i].m_Creator);
			}
			else if(m_aTeamReload[i] == RELOAD_TYPE_SOFT)
			{
				m_aTeamReload[i] = RELOAD_TYPE_NO;

				for(int p = 0; p < MAX_CLIENTS; p++)
					if(GameServer()->IsPlayerValid(p) && m_Core.Team(p) == i)
						GameServer()->m_apPlayers[p]->KillCharacter();

				delete m_aTeamInstances[i].m_pWorld;
				m_aTeamInstances[i].m_Entities = 0;
				m_aTeamInstances[i].m_Init = false;
				m_aTeamInstances[i].m_pWorld = new CGameWorld(i, m_pGameContext);
				m_aTeamInstances[i].m_pController->InitController(m_pGameContext, m_aTeamInstances[i].m_pWorld);
			}
			else
			{
				m_aTeamInstances[i].m_pWorld->m_Core.m_Tuning[0] = *GameServer()->Tuning();
				m_aTeamInstances[i].m_pWorld->Tick();
				m_aTeamInstances[i].m_pController->Tick();
			}
		}

		if(m_aTeamInstances[i].m_IsCreated && !m_aTeamInstances[i].m_Init)
			NeedToProcessEntities = true;
	}

	if(NeedToProcessEntities)
	{
		int NumProcessed = 0;
		while(1)
		{
			int NumCreated = 0;
			int NumInit = 0;
			for(int i = 0; i < MAX_CLIENTS; ++i)
			{
				if(m_aTeamInstances[i].m_IsCreated && !m_aTeamInstances[i].m_Init)
				{
					if(m_aTeamInstances[i].m_Entities < m_Entities.size())
					{
						auto E = m_Entities[m_aTeamInstances[i].m_Entities];
						m_aTeamInstances[i].m_pController->OnInternalEntity(E.Index, E.Pos, E.Layer, E.Flags, E.MegaMapIndex, E.Number);
						m_aTeamInstances[i].m_Entities++;
						NumProcessed++;
					}

					if(m_aTeamInstances[i].m_Entities == m_Entities.size())
						m_aTeamInstances[i].m_Init = true;
				}
				if(m_aTeamInstances[i].m_IsCreated)
					NumCreated++;
				if(m_aTeamInstances[i].m_Init)
					NumInit++;
				if(NumProcessed >= ENTITIES_PER_TICK)
					break;
			}
			if(NumCreated == NumInit)
				break;
		}
	}
}

void CGameTeams::OnEntity(int Index, vec2 Pos, int Layer, int Flags, int MegaMapIndex, int Number)
{
	SEntity Ent;
	Ent.Index = Index;
	Ent.Pos = Pos;
	Ent.Layer = Layer;
	Ent.Flags = Flags;
	Ent.MegaMapIndex = MegaMapIndex;
	Ent.Number = Number;
	m_Entities.push_back(Ent);
}

void CGameTeams::OnSnap(int SnappingClient)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[SnappingClient];
	int ShowOthers = pPlayer->m_ShowOthers || (m_Core.Team(SnappingClient) == 0 && g_Config.m_SvRoom == 2);

	int SnapAs = SnappingClient;
	if(pPlayer->IsSpectating() && pPlayer->m_SpectatorID != SPEC_FREEVIEW)
		SnapAs = pPlayer->m_SpectatorID;

	int SnapAsTeam = m_Core.Team(SnapAs);

	// Spectator
	if(ShowOthers)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
			if(m_aTeamInstances[i].m_Init)
			{
				m_aTeamInstances[i].m_pWorld->Snap(SnappingClient, SnapAsTeam == i ? 0 : ShowOthers);
				if(SnapAsTeam == i)
					m_aTeamInstances[i].m_pController->Snap(SnappingClient);
			}
	}
	else
	{
		SGameInstance Instance = GetGameInstance(SnapAsTeam);
		if(!Instance.m_Init)
			return;
		Instance.m_pWorld->Snap(SnappingClient, 0);
		Instance.m_pController->Snap(SnappingClient);
	}
}

void CGameTeams::OnPostSnap()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
		if(m_aTeamInstances[i].m_Init)
			m_aTeamInstances[i].m_pWorld->OnPostSnap();
}

int CGameTeams::GetTeamState(int Team)
{
	return m_aTeamState[Team];
}

bool CGameTeams::TeamLocked(int Team)
{
	if(Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
		return false;

	return m_aTeamLocked[Team];
}

bool CGameTeams::IsInvited(int Team, int ClientID)
{
	return m_aInvited[Team] & 1LL << ClientID;
}

int CGameTeams::CanSwitchTeam(int ClientID)
{
	SGameInstance Instance = GetPlayerGameInstance(ClientID);
	return Instance.m_Init && !Instance.m_pWorld->m_Paused;
}

int CGameTeams::FindAEmptyTeam()
{
	for(int i = 1; i < MAX_CLIENTS; ++i)
		if(m_aTeamState[i] == TEAMSTATE_EMPTY)
			return i;
	return -1;
}
std::vector<SGameType> CGameTeams::m_GameTypes;
SGameType CGameTeams::m_DefaultGameType = {nullptr, nullptr, nullptr, false};
char CGameTeams::m_aMapNames[64][128];
int CGameTeams::m_NumMaps;

void CGameTeams::SetDefaultGameType(const char *pGameType, const char *pSettings, bool IsFile)
{
	bool IsValidGameType = false;
	if(false)
		return;
#define REGISTER_GAME_TYPE(TYPE, CLASS) \
	else if(str_comp_nocase(#TYPE, pGameType) == 0) \
	{ \
		m_DefaultGameType.pGameType = #TYPE; \
		IsValidGameType = true; \
	}
#include "gamemodes.h"
#undef REGISTER_GAME_TYPE

	if(!IsValidGameType)
		return;

	int Len;

	if(pSettings)
	{
		Len = str_length(pSettings) + 1;
		m_DefaultGameType.pSettings = (char *)malloc(Len * sizeof(char));
		str_copy(m_DefaultGameType.pSettings, pSettings, Len);
	}
	else
		m_DefaultGameType.pSettings = nullptr;

	m_DefaultGameType.IsFile = IsFile;
}

void CGameTeams::UpdateVotes()
{
	m_NumRooms = 0;

	if(!g_Config.m_SvRoomVotes)
	{
		if(m_aRoomVotes[m_NumRooms][0])
		{
			CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
			GameServer()->Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, -1);
			for(auto &pPlayer : GameServer()->m_apPlayers)
				if(pPlayer)
					pPlayer->m_SendVoteIndex = 0;
			m_aRoomVotes[m_NumRooms][0] = 0;
		}
		return;
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		int RoomSize = m_Core.Count(i);
		IGameController *pController = m_aTeamInstances[i].m_pController;

		if(!pController || (RoomSize == 0 && !(g_Config.m_SvRoom == 1 && i == 0)))
			continue;

		if(m_aTeamInstances[i].m_Creator < 0)
			str_format(m_aRoomVotes[m_NumRooms], sizeof(m_aRoomVotes[m_NumRooms]), "☉ Room %d: ♙%d [%s]", i, m_Core.Count(i), pController->GetGameType());
		else
			str_format(m_aRoomVotes[m_NumRooms], sizeof(m_aRoomVotes[m_NumRooms]), "☉ Room %d: ♙%d [%s] ♔%s", i, m_Core.Count(i), pController->GetGameType(), GameServer()->Server()->ClientName(m_aTeamInstances[i].m_Creator));
		m_NumRooms++;
	}

	if(m_NumRooms < MAX_CLIENTS)
		m_aRoomVotes[m_NumRooms][0] = 0;

	// reset sending of vote options
	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	GameServer()->Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, -1);
	for(auto &pPlayer : GameServer()->m_apPlayers)
		if(pPlayer)
			pPlayer->m_SendVoteIndex = 0;
}

void CGameTeams::AddGameType(const char *pGameType, const char *pName, const char *pSettings, bool IsFile)
{
	SGameType Type;

	bool IsValidGameType = false;
	if(false)
		return;
#define REGISTER_GAME_TYPE(TYPE, CLASS) \
	else if(str_comp_nocase(#TYPE, pGameType) == 0) \
	{ \
		Type.pGameType = #TYPE; \
		IsValidGameType = true; \
	}
#include "gamemodes.h"
#undef REGISTER_GAME_TYPE

	if(!IsValidGameType)
		return;

	int Len;

	if(pSettings)
	{
		Len = str_length(pSettings) + 1;
		Type.pSettings = (char *)malloc(Len * sizeof(char));
		str_copy(Type.pSettings, pSettings, Len);
	}
	else
		Type.pSettings = nullptr;

	if(pName)
	{
		Len = str_length(pName) + 1;
		Type.pName = (char *)malloc(Len * sizeof(char));
		str_copy(Type.pName, pName, Len);
	}
	else
	{
		Len = str_length(Type.pGameType) + 1;
		Type.pName = (char *)malloc(Len * sizeof(char));
		str_copy(Type.pName, Type.pGameType, Len);
	}

	Type.IsFile = IsFile;

	if(!m_DefaultGameType.pGameType)
		SetDefaultGameType(Type.pGameType, Type.pSettings, IsFile);

	m_GameTypes.push_back(Type);
}

void CGameTeams::ClearGameTypes()
{
	while(m_GameTypes.size() > 0)
	{
		SGameType Type = m_GameTypes.back();
		if(Type.pSettings)
			free(Type.pSettings);
		if(Type.pName)
			free(Type.pName);
		m_GameTypes.pop_back();
	}
	if(m_DefaultGameType.pGameType)
	{
		if(m_DefaultGameType.pSettings)
			free(m_DefaultGameType.pSettings);
		if(m_DefaultGameType.pName)
			free(m_DefaultGameType.pName);
		m_DefaultGameType.pGameType = nullptr;
	}
}

void CGameTeams::ClearMaps()
{
	m_NumMaps = 0;
}

void CGameTeams::AddMap(const char *pMapName)
{
	if(m_NumMaps >= 64)
		return;
	str_copy(m_aMapNames[m_NumMaps++], pMapName, 128);
}

int CGameTeams::GetMapIndex(const char *pMapName)
{
	for(int i = 0; i < m_NumMaps; i++)
		if(str_comp_nocase(pMapName, m_aMapNames[i]) == 0)
			return i + 1;

	return 0;
}