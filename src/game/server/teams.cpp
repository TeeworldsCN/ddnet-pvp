/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "teams.h"
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

	SGameInstance Instance = GetPlayerGameInstance(ClientID);
	if(Instance.m_Init && Instance.m_pController->IsDisruptiveLeave(GameServer()->m_apPlayers[ClientID]))
		return "You can't change room right now";

	CCharacter *pChar = GameServer()->GetPlayerChar(ClientID);
	if(pChar)
	{
		if(Team == TEAM_SUPER && !pChar->m_Super)
			return "You can't join super room if you don't have super rights";
		if(Team != TEAM_SUPER && pChar->m_DDRaceState != DDRACE_NONE)
			return "You have started the game already";
	}

	if(!SetForcePlayerTeam(ClientID, Team, TEAM_REASON_NORMAL, pGameType))
		return "Room does not exists. Use /create to create a room";

	return nullptr;
}

bool CGameTeams::SetForcePlayerTeam(int ClientID, int Team, int State, const char *pGameType)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	if(!pPlayer)
		return false;

	int OldTeam = m_Core.Team(ClientID);
	bool CreatingRoom = false;

	if(OldTeam != Team && !m_aTeamInstances[Team].m_IsCreated)
	{
		if(!pGameType && State != TEAM_REASON_FORCE && m_GameTypes.size() > 1)
			return false;

		CreatingRoom = true;
		if(!CreateGameInstance(Team, pGameType, ClientID))
			return false;
	}

	if(Team != OldTeam && OldTeam != TEAM_SUPER && m_aTeamState[OldTeam] != TEAMSTATE_EMPTY)
	{
		if((State == TEAM_REASON_NORMAL || State == TEAM_REASON_FORCE) && m_aTeamInstances[OldTeam].m_IsCreated)
			m_aTeamInstances[OldTeam].m_pController->OnInternalPlayerLeave(GameServer()->m_apPlayers[ClientID], State == TEAM_REASON_FORCE ? INSTANCE_CONNECTION_FORCED : (CreatingRoom ? INSTANCE_CONNECTION_CREATE : INSTANCE_CONNECTION_NORMAL));
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
			m_aTeamInstances[Team].m_pController->OnInternalPlayerJoin(GameServer()->m_apPlayers[ClientID], CreatingRoom ? INSTANCE_CONNECTION_CREATE : INSTANCE_CONNECTION_NORMAL);

		for(int LoopClientID = 0; LoopClientID < MAX_CLIENTS; ++LoopClientID)
			if(GameServer()->PlayerExists(LoopClientID))
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
	if(!m_pGameContext->m_apPlayers[ClientID])
		return;

	CMsgPacker Msg(NETMSGTYPE_SV_TEAMSSTATE);
	CMsgPacker MsgLegacy(NETMSGTYPE_SV_TEAMSSTATELEGACY);

	for(unsigned i = 0; i < MAX_CLIENTS; i++)
	{
		Msg.AddInt(m_Core.Team(i));
		MsgLegacy.AddInt(m_Core.Team(i));
	}

	GameServer()->Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
	int ClientVersion = GameServer()->m_apPlayers[ClientID]->GetClientVersion();
	if(!GameServer()->Server()->IsSixup(ClientID) && VERSION_DDRACE < ClientVersion && ClientVersion <= VERSION_DDNET_MSG_LEGACY)
	{
		GameServer()->Server()->SendMsg(&MsgLegacy, MSGFLAG_VITAL, ClientID);
	}
}

void CGameTeams::SetTeamLock(int Team, bool Lock)
{
	if(Team > TEAM_FLOCK && Team < TEAM_SUPER)
	{
		if(m_aTeamLocked[Team] != Lock)
		{
			m_aTeamLocked[Team] = Lock;
			UpdateVotes();
		}
	}
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

	if(m_aTeamInstances[Team].m_Entities == 0)
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

	CGameWorld *pWorld = new CGameWorld(Team, m_pGameContext, Game);
	m_aTeamInstances[Team].m_pWorld = pWorld;
	m_aTeamInstances[Team].m_pController = Game;
	m_aTeamInstances[Team].m_pController->m_MapIndex = m_NumMaps > 0 ? 1 : 0;
	m_aTeamInstances[Team].m_pController->InitController(m_pGameContext, pWorld);
	m_aTeamInstances[Team].m_IsCreated = true;
	m_aTeamInstances[Team].m_Init = false;
	m_aTeamInstances[Team].m_Entities = 0;

	// -2 means reload, if reload, don't update creator's name
	if(Asker == -1)
		m_aTeamInstances[Team].m_Creator[0] = 0;
	else if(Asker >= 0)
		str_copy(m_aTeamInstances[Team].m_Creator, GameServer()->Server()->ClientName(Asker), sizeof(m_aTeamInstances[Team].m_Creator));

	// surpress room creation reply
	GameServer()->m_ChatResponseTargetID = -1;
	m_aTeamInstances[Team].m_pController->InstanceConsole()->SetFlagMask(CFGFLAG_INSTANCE);

	if(Type.pSettings && Type.pSettings[0])
	{
		if(Type.IsFile)
			m_aTeamInstances[Team].m_pController->InstanceConsole()->ExecuteFile(Type.pSettings);
		else
			m_aTeamInstances[Team].m_pController->InstanceConsole()->ExecuteLine(Type.pSettings);
	}

	if(Team == 0 && g_Config.m_SvLobbyOverrideConfig[0])
		m_aTeamInstances[Team].m_pController->InstanceConsole()->ExecuteFile(g_Config.m_SvLobbyOverrideConfig);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "game controller %d is created", Team);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "team", aBuf);

	for(int i = 0; i < MAX_CLIENTS; i++)
		if(GameServer()->PlayerExists(i) && m_Core.Team(i) == Team)
			m_aTeamInstances[Team].m_pController->OnInternalPlayerJoin(GameServer()->m_apPlayers[i], INSTANCE_CONNECTION_RELOAD);

	UpdateGameTypeName();
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
		if(GameServer()->PlayerExists(i) && m_Core.Team(i) == Team)
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

	UpdateGameTypeName();
}

void CGameTeams::OnPlayerConnect(CPlayer *pPlayer)
{
	int ClientID = pPlayer->GetCID();

	SetForcePlayerTeam(ClientID, 0, TEAM_REASON_CONNECT);

	if(m_aTeamInstances[m_Core.Team(ClientID)].m_IsCreated)
		m_aTeamInstances[m_Core.Team(ClientID)].m_pController->OnInternalPlayerJoin(pPlayer, INSTANCE_CONNECTION_SERVER);

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
		m_aTeamInstances[m_Core.Team(ClientID)].m_pController->OnInternalPlayerLeave(pPlayer, INSTANCE_CONNECTION_SERVER);

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
				CreateGameInstance(i, m_apWantedGameType[i], -2);
				UpdateVotes();
			}
			else if(m_aTeamReload[i] == RELOAD_TYPE_SOFT)
			{
				m_aTeamReload[i] = RELOAD_TYPE_NO;

				for(int p = 0; p < MAX_CLIENTS; p++)
					if(GameServer()->PlayerExists(p) && m_Core.Team(p) == i)
						GameServer()->m_apPlayers[p]->KillCharacter();

				delete m_aTeamInstances[i].m_pWorld;
				m_aTeamInstances[i].m_Entities = 0;
				m_aTeamInstances[i].m_Init = false;
				m_aTeamInstances[i].m_pWorld = new CGameWorld(i, m_pGameContext, m_aTeamInstances[i].m_pController);
				m_aTeamInstances[i].m_pController->InitController(m_pGameContext, m_aTeamInstances[i].m_pWorld);
			}
			else
			{
				m_aTeamInstances[i].m_pWorld->m_Core.m_Tuning = *GameServer()->Tuning();
				m_aTeamInstances[i].m_pController->Tick();
				m_aTeamInstances[i].m_pWorld->Tick();
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
					{
						m_aTeamInstances[i].m_Init = true;
						m_aTeamInstances[i].m_pController->StartController();
					}
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
	int ShowOthers = pPlayer->ShowOthersMode();

	int SnapAs = SnappingClient;
	if(pPlayer->IsSpectating() && pPlayer->GetSpectatorID() >= 0)
		SnapAs = pPlayer->GetSpectatorID();

	int SnapAsTeam = m_Core.Team(SnapAs);

	// Spectator
	if(ShowOthers)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(m_aTeamInstances[i].m_Init)
				m_aTeamInstances[i].m_pWorld->Snap(SnappingClient, SnapAsTeam == i ? 0 : ShowOthers);
			if(m_aTeamInstances[i].m_IsCreated && SnapAsTeam == i)
				m_aTeamInstances[i].m_pController->Snap(SnappingClient);
		}
	}
	else
	{
		SGameInstance Instance = GetGameInstance(SnapAsTeam);
		if(Instance.m_Init)
			Instance.m_pWorld->Snap(SnappingClient, 0);
		if(Instance.m_IsCreated)
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
	return Instance.m_Init;
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
char CGameTeams::m_aGameTypeName[17] = {0};

void CGameTeams::SetDefaultGameType(const char *pGameType, const char *pSettings, bool IsFile)
{
	if(!pGameType || !(*pGameType))
	{
		if(m_DefaultGameType.pGameType)
		{
			if(m_DefaultGameType.pSettings)
				free(m_DefaultGameType.pSettings);
			if(m_DefaultGameType.pName)
				free(m_DefaultGameType.pName);
			m_DefaultGameType.pGameType = nullptr;
		}
		return;
	}

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
	if(!GameServer())
		return;

	m_NumRooms = 0;

	if(!g_Config.m_SvRoomVotes || g_Config.m_SvRoom == 0)
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

	int PlayerCount = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
		PlayerCount += GameServer()->PlayerExists(i) ? 1 : 0;
	int RemainingSlots = g_Config.m_SvMaxClients - g_Config.m_SvReservedSlots - PlayerCount;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		int NumPlayersInRoom = m_Core.Count(i);
		IGameController *pController = m_aTeamInstances[i].m_pController;

		if(!pController || (NumPlayersInRoom == 0 && i != 0))
			continue;

		if(m_aTeamInstances[i].m_Creator[0])
		{
			str_format(m_aRoomVotes[m_NumRooms], sizeof(m_aRoomVotes[m_NumRooms]), "%s Room %d: ♙%d/%d [%s] ♔%s", TeamLocked(i) ? "⨂" : "⨀", i, NumPlayersInRoom, TeamLocked(i) ? NumPlayersInRoom : minimum(pController->m_PlayerSlots, RemainingSlots + NumPlayersInRoom + 1), pController->GetGameType(), m_aTeamInstances[i].m_Creator);
			str_format(m_aRoomVotesJoined[m_NumRooms], sizeof(m_aRoomVotesJoined[m_NumRooms]), "%s Room %d: ♙%d/%d [%s] ♔%s ⬅", TeamLocked(i) ? "⨂" : "⨀", i, NumPlayersInRoom, TeamLocked(i) ? NumPlayersInRoom : minimum(pController->m_PlayerSlots, RemainingSlots + NumPlayersInRoom), pController->GetGameType(), m_aTeamInstances[i].m_Creator);
		}
		else
		{
			str_format(m_aRoomVotes[m_NumRooms], sizeof(m_aRoomVotes[m_NumRooms]), "%s Room %d: ♙%d/%d [%s] 旁观房间", TeamLocked(i) ? "⨂" : "⨀", i, NumPlayersInRoom, TeamLocked(i) ? NumPlayersInRoom : minimum(pController->m_PlayerSlots, RemainingSlots + NumPlayersInRoom + 1), pController->GetGameType());
			str_format(m_aRoomVotesJoined[m_NumRooms], sizeof(m_aRoomVotesJoined[m_NumRooms]), "%s Room %d: ♙%d/%d [%s] 旁观房间 ⬅", TeamLocked(i) ? "⨂" : "⨀", i, NumPlayersInRoom, TeamLocked(i) ? NumPlayersInRoom : minimum(pController->m_PlayerSlots, RemainingSlots + NumPlayersInRoom), pController->GetGameType());
		}
		m_RoomNumbers[m_NumRooms] = i;
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
}

void CGameTeams::UpdateGameTypeName()
{
	char *TypeName = m_aGameTypeName;
	*TypeName = 0;
	char *Types[16];
	int NumTypes = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aTeamInstances[i].m_IsCreated)
		{
			const char *Type = m_aTeamInstances[i].m_pController->GetGameType();
			int Len = str_length(Type);
			int RemainLen = sizeof(m_aGameTypeName) - (size_t)(TypeName - m_aGameTypeName);
			bool AlreadyHas = false;
			if(RemainLen <= Len)
				continue;

			for(int t = 0; t < NumTypes; t++)
			{
				if(str_comp(Types[t], Type) == 0)
				{
					AlreadyHas = true;
					break;
				}
			}
			if(!AlreadyHas)
			{
				mem_copy(TypeName, Type, Len);
				TypeName[Len] = 0;
				Types[NumTypes] = TypeName;
				NumTypes++;
				TypeName += Len + 1;
			}
		}
	}

	if(NumTypes <= 1)
	{
		*(TypeName - 1) = '*';
		*TypeName = 0;
	}
	else
	{
		for(int i = 1; i < NumTypes; i++)
		{
			*(Types[i] - 1) = '|';
		}
	}
	GameServer()->Server()->ExpireServerInfo();
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