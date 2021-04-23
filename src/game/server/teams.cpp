/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "teams.h"
#include "teehistorian.h"
#include <engine/shared/config.h>

#include "entities/character.h"
#include "player.h"

#include "gamemodes/DDRace.h"

CGameTeams::CGameTeams(CGameContext *pGameContext) :
	m_pGameContext(pGameContext)
{
	mem_zero(m_aTeamInstances, sizeof(m_aTeamInstances));
	Reset();
}

CGameTeams::~CGameTeams()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
		DestroyGameInstance(i);
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
	CreateGameInstance(0);
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

const char *CGameTeams::SetCharacterTeam(int ClientID, int Team)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return "Invalid client ID";
	if(Team < 0 || Team >= MAX_CLIENTS + 1)
		return "Invalid room number";
	if(Team != TEAM_SUPER && m_aTeamState[Team] > TEAMSTATE_OPEN)
		return "This room started already";
	if(m_Core.Team(ClientID) == Team)
		return "You are in this room already";

	CCharacter *pChar = Character(ClientID);
	if(pChar)
	{
		if(Team == TEAM_SUPER && !pChar->m_Super)
			return "You can't join super room if you don't have super rights";
		if(Team != TEAM_SUPER && pChar->m_DDRaceState != DDRACE_NONE)
			return "You have started the game already";

		pChar->Die(ClientID, WEAPON_GAME);
	}

	CPlayer *pPlayer = GetPlayer(ClientID);
	// clear score when joining a new room
	pPlayer->m_Score = 0;

	SetForceCharacterTeam(ClientID, Team);
	return nullptr;
}

void CGameTeams::SetForceCharacterTeam(int ClientID, int Team)
{
	int OldTeam = m_Core.Team(ClientID);

	if(Team != OldTeam && (OldTeam != TEAM_FLOCK || g_Config.m_SvTeam == 3) && OldTeam != TEAM_SUPER && m_aTeamState[OldTeam] != TEAMSTATE_EMPTY)
	{
		bool NoElseInOldTeam = Count(OldTeam) <= 1;
		if(NoElseInOldTeam)
		{
			m_aTeamState[OldTeam] = TEAMSTATE_EMPTY;

			// unlock team when last player leaves
			SetTeamLock(OldTeam, false);
			ResetRoundState(OldTeam);
			DestroyGameInstance(OldTeam);
			// do not reset SaveTeamResult, because it should be logged into teehistorian even if the team leaves
		}
	}

	m_Core.Team(ClientID, Team);

	if(OldTeam != Team)
	{
		for(int LoopClientID = 0; LoopClientID < MAX_CLIENTS; ++LoopClientID)
			if(GetPlayer(LoopClientID))
				SendTeamsState(LoopClientID);
	}

	int TeamOldState = m_aTeamState[Team];

	if(Team != TEAM_SUPER && (TeamOldState == TEAMSTATE_EMPTY || m_aTeamLocked[Team]))
	{
		if(!m_aTeamLocked[Team])
			ChangeTeamState(Team, TEAMSTATE_OPEN);

		ResetSwitchers(Team);
	}

	if(TeamOldState == TEAMSTATE_EMPTY && m_aTeamState[Team] != TEAMSTATE_EMPTY)
	{
		if(!m_aTeamInstances[Team].m_IsCreated)
			CreateGameInstance(Team);
	}
}

int CGameTeams::Count(int Team) const
{
	if(Team == TEAM_SUPER)
		return -1;

	int Count = 0;

	for(int i = 0; i < MAX_CLIENTS; ++i)
		if(m_Core.Team(i) == Team)
			Count++;

	return Count;
}

void CGameTeams::ChangeTeamState(int Team, int State)
{
	m_aTeamState[Team] = State;
}

void CGameTeams::SendTeamsState(int ClientID)
{
	if(g_Config.m_SvTeam == 3)
		return;

	if(!m_pGameContext->m_apPlayers[ClientID] || m_pGameContext->m_apPlayers[ClientID]->GetClientVersion() <= VERSION_DDRACE)
		return;

	CMsgPacker Msg(NETMSGTYPE_SV_TEAMSSTATE);

	for(unsigned i = 0; i < MAX_CLIENTS; i++)
		Msg.AddInt(m_Core.Team(i));

	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameTeams::OnCharacterSpawn(int ClientID)
{
	m_Core.SetSolo(ClientID, false);
	int Team = m_Core.Team(ClientID);

	if(m_Core.Team(ClientID) >= TEAM_SUPER || !m_aTeamLocked[Team])
	{
		if(g_Config.m_SvTeam != 3)
			SetForceCharacterTeam(ClientID, Team);
		else
			SetForceCharacterTeam(ClientID, ClientID); // initialize team
	}
}

void CGameTeams::OnCharacterDeath(int ClientID, int Weapon)
{
	m_Core.SetSolo(ClientID, false);

	int Team = m_Core.Team(ClientID);

	if(g_Config.m_SvTeam == 3)
	{
		ChangeTeamState(Team, CGameTeams::TEAMSTATE_OPEN);
		ResetSwitchers(Team);
	}

	SetForceCharacterTeam(ClientID, Team);
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

void CGameTeams::CreateGameInstance(int Team)
{
	if(m_aTeamInstances[Team].m_IsCreated)
		DestroyGameInstance(Team);

	dbg_msg("team", "creating game controller %d", Team);
	CGameWorld *pWorld = new CGameWorld(Team, m_pGameContext);
	m_aTeamInstances[Team].m_pWorld = pWorld;
	m_aTeamInstances[Team].m_pController = new CGameControllerDDRace();
	m_aTeamInstances[Team].m_pController->InitController(Team, m_pGameContext, pWorld);
	for(auto &Ent : m_Entities)
		m_aTeamInstances[Team].m_pController->OnEntity(Ent.Index, Ent.Pos, Ent.Layer, Ent.Flags, Ent.Number);
	m_aTeamInstances[Team].m_IsCreated = true;
}

void CGameTeams::DestroyGameInstance(int Team)
{
	if(!m_aTeamInstances[Team].m_IsCreated)
		return;

	dbg_msg("team", "deleting game controller %d", Team);
	delete m_aTeamInstances[Team].m_pController;
	delete m_aTeamInstances[Team].m_pWorld;
	m_aTeamInstances[Team].m_IsCreated = false;
	m_aTeamInstances[Team].m_pController = nullptr;
	m_aTeamInstances[Team].m_pWorld = nullptr;
}

void CGameTeams::OnTick()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
		if(m_aTeamInstances[i].m_IsCreated)
		{
			m_aTeamInstances[i].m_pWorld->m_Core.m_Tuning[0] = *GameServer()->Tuning();
			m_aTeamInstances[i].m_pWorld->Tick();
			m_aTeamInstances[i].m_pController->Tick();
		}
}

void CGameTeams::OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number)
{
	SEntity Ent;
	Ent.Index = Index;
	Ent.Pos = Pos;
	Ent.Layer = Layer;
	Ent.Flags = Flags;
	Ent.Number = Number;
	m_Entities.push_back(Ent);
	for(int i = 0; i < MAX_CLIENTS; ++i)
		if(m_aTeamInstances[i].m_IsCreated)
			m_aTeamInstances[i].m_pController->OnEntity(Index, Pos, Layer, Flags, Number);
}

void CGameTeams::OnSnap(int SnappingClient)
{
	CPlayer *pPlayer = GetPlayer(SnappingClient);
	int ShowOthers = pPlayer->m_ShowOthers;

	int SnapAs = SnappingClient;
	if((pPlayer->GetTeam() == TEAM_SPECTATORS || pPlayer->IsPaused()) && pPlayer->m_SpectatorID != SPEC_FREEVIEW)
		SnapAs = pPlayer->m_SpectatorID;

	int SnapAsTeam = m_Core.Team(SnapAs);

	// Spectator
	if(ShowOthers)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
			if(m_aTeamInstances[i].m_IsCreated)
			{
				m_aTeamInstances[i].m_pWorld->Snap(SnapAs, SnapAsTeam == i ? 0 : ShowOthers);
				if(SnapAsTeam == i)
					m_aTeamInstances[i].m_pController->Snap(SnapAs);
			}
	}
	else
	{
		SGameInstance Instance = GetGameInstance(SnapAsTeam);
		if(!Instance.m_IsCreated)
			return;
		Instance.m_pWorld->Snap(SnapAs, 1);
		Instance.m_pController->Snap(SnapAs);
	}
}

void CGameTeams::OnPostSnap()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
		if(m_aTeamInstances[i].m_IsCreated)
			m_aTeamInstances[i].m_pWorld->OnPostSnap();
}