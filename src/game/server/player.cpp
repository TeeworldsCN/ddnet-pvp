/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "player.h"
#include <engine/shared/config.h>

#include "entities/character.h"
#include "gamecontext.h"
#include <engine/server.h>
#include <game/gamecore.h>
#include <game/server/entities/flag.h>
#include <game/version.h>

MACRO_ALLOC_POOL_ID_IMPL(CPlayer, MAX_CLIENTS)

IServer *CPlayer::Server() const { return m_pGameServer->Server(); }

CPlayer::CPlayer(CGameContext *pGameServer, int ClientID, bool AsSpec)
{
	m_pGameServer = pGameServer;
	m_ClientID = ClientID;
	m_Team = AsSpec ? TEAM_SPECTATORS : TEAM_RED; // controller will decide player's team again.
	m_NumInputs = 0;
	Reset();
	GameServer()->Antibot()->OnPlayerInit(m_ClientID);
}

CPlayer::~CPlayer()
{
	GameServer()->Antibot()->OnPlayerDestroy(m_ClientID);
	delete m_pLastTarget;
	delete m_pCharacter;
	m_pCharacter = 0;
}

void CPlayer::Reset()
{
	m_aOverrideName[0] = 0;
	m_aOverrideClan[0] = 0;

	GameReset();

	m_JoinTick = Server()->Tick();
	delete m_pCharacter;
	m_pCharacter = 0;
	m_RespawnDisabled = false;

	m_IsReadyToEnter = false;

	int *pIdMap = Server()->GetIdMap(m_ClientID);
	for(int i = 1; i < VANILLA_MAX_CLIENTS; i++)
	{
		pIdMap[i] = -1;
	}
	pIdMap[0] = m_ClientID;

	// DDRace

	m_LastCommandPos = 0;
	m_LastPlaytime = 0;
	m_LastEyeEmote = 0;
	m_Sent1stAfkWarning = 0;
	m_Sent2ndAfkWarning = 0;
	m_ChatScore = 0;
	m_Moderating = false;
	m_EyeEmoteEnabled = true;
	if(Server()->IsSixup(m_ClientID))
		m_TimerType = TIMERTYPE_SIXUP;
	else
		m_TimerType = (g_Config.m_SvDefaultTimerType == TIMERTYPE_GAMETIMER || g_Config.m_SvDefaultTimerType == TIMERTYPE_GAMETIMER_AND_BROADCAST) ? TIMERTYPE_BROADCAST : g_Config.m_SvDefaultTimerType;

	m_DefEmote = EMOTE_NORMAL;
	m_Afk = true;
	m_LastWhisperTo = -1;
	m_LastSetSpectatorMode = 0;
	m_TimeoutCode[0] = '\0';
	delete m_pLastTarget;
	m_pLastTarget = nullptr;
	m_TuneZone = 0;
	m_TuneZoneOld = m_TuneZone;
	m_Halloween = false;
	m_FirstPacket = true;

	m_SendVoteIndex = -1;

	if(g_Config.m_Events)
	{
		time_t rawtime;
		struct tm *timeinfo;
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		if((timeinfo->tm_mon == 11 && timeinfo->tm_mday == 31) || (timeinfo->tm_mon == 0 && timeinfo->tm_mday == 1))
		{ // New Year
			m_DefEmote = EMOTE_HAPPY;
		}
		else if((timeinfo->tm_mon == 9 && timeinfo->tm_mday == 31) || (timeinfo->tm_mon == 10 && timeinfo->tm_mday == 1))
		{ // Halloween
			m_DefEmote = EMOTE_ANGRY;
			m_Halloween = true;
		}
		else
		{
			m_DefEmote = EMOTE_NORMAL;
		}
	}
	m_OverrideEmoteReset = -1;

	m_ShowOthers = g_Config.m_SvShowOthersDefault;
	m_ShowDistance = vec2(SHOW_DISTANCE_DEFAULT_X, SHOW_DISTANCE_DEFAULT_Y);
	m_SpecTeam = 1; // PvP: default to spec only team

	m_Paused = false;
	m_DND = false;

	m_LastPause = 0;

	// Variable initialized:
	m_LastRoomChange = 0;

	int64 Now = Server()->Tick();
	int64 TickSpeed = Server()->TickSpeed();
	// If the player joins within ten seconds of the server becoming
	// non-empty, allow them to vote immediately. This allows players to
	// vote after map changes or when they join an empty server.
	//
	// Otherwise, block voting in the beginning after joining.
	if(Now > GameServer()->m_NonEmptySince + 10 * TickSpeed)
		m_FirstVoteTick = Now + g_Config.m_SvJoinVoteDelay * TickSpeed;
	else
		m_FirstVoteTick = Now;

	m_NotEligibleForFinish = false;
	m_EligibleForFinishCheck = 0;
}

void CPlayer::GameReset()
{
	m_Spawning = false;
	m_DieTick = Server()->Tick();
	m_ScoreStartTick = Server()->Tick();
	m_PreviousDieTick = m_DieTick;
	m_SpectatorID = SPEC_FREEVIEW;
	m_LastActionTick = Server()->Tick();
	m_TeamChangeTick = Server()->Tick();
	m_LastInvited = 0;
	m_InactivityTickCounter = 0;
	m_IsReadyToPlay = false;
	m_DeadSpecMode = false;
	m_Score = 0;
}
static int PlayerFlags_SevenToSix(int Flags)
{
	int Six = 0;
	if(Flags & protocol7::PLAYERFLAG_CHATTING)
		Six |= PLAYERFLAG_CHATTING;
	if(Flags & protocol7::PLAYERFLAG_SCOREBOARD)
		Six |= PLAYERFLAG_SCOREBOARD;

	return Six;
}

static int PlayerFlags_SixToSeven(int Flags)
{
	int Seven = 0;
	if(Flags & PLAYERFLAG_CHATTING)
		Seven |= protocol7::PLAYERFLAG_CHATTING;
	if(Flags & PLAYERFLAG_SCOREBOARD)
		Seven |= protocol7::PLAYERFLAG_SCOREBOARD;

	return Seven;
}

bool CPlayer::SetSpectatorID(int SpecMode, int SpectatorID)
{
	if((SpecMode == m_SpecMode && SpecMode != SPECMODE_PLAYER) ||
		(m_SpecMode == SPECMODE_PLAYER && SpecMode == SPECMODE_PLAYER && (SpectatorID == -1 || m_SpectatorID == SpectatorID || m_ClientID == SpectatorID)))
	{
		return false;
	}

	if(IsSpectating())
	{
		// check for freeview or if wanted player is playing
		if(SpecMode != SPECMODE_PLAYER || (SpecMode == SPECMODE_PLAYER && GameServer()->IsClientPlayer(SpectatorID)))
		{
			if(SpecMode == SPECMODE_FLAGRED || SpecMode == SPECMODE_FLAGBLUE)
			{
				CFlag *pFlag = (CFlag *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_FLAG);
				while(pFlag)
				{
					if((pFlag->GetTeam() == TEAM_RED && SpecMode == SPECMODE_FLAGRED) || (pFlag->GetTeam() == TEAM_BLUE && SpecMode == SPECMODE_FLAGBLUE))
					{
						m_pSpecFlag = pFlag;
						if(pFlag->GetCarrier())
							m_SpectatorID = pFlag->GetCarrier()->GetPlayer()->GetCID();
						else
							m_SpectatorID = -1;
						break;
					}
					pFlag = (CFlag *)pFlag->TypeNext();
				}
				if(!m_pSpecFlag)
					return false;
				m_SpecMode = SpecMode;
				return true;
			}
			m_pSpecFlag = 0;
			m_SpecMode = SpecMode;
			m_SpectatorID = SpectatorID;
			return true;
		}
	}
	else if(m_DeadSpecMode)
	{
		// check if wanted player can be followed
		if(SpecMode == SPECMODE_PLAYER && GameServer()->m_apPlayers[SpectatorID] && DeadCanFollow(GameServer()->m_apPlayers[SpectatorID]))
		{
			m_SpecMode = SpecMode;
			m_pSpecFlag = 0;
			m_SpectatorID = SpectatorID;
			return true;
		}
	}

	return false;
}

bool CPlayer::DeadCanFollow(CPlayer *pPlayer) const
{
	// check if wanted player is in the same team and alive
	return (!pPlayer->m_RespawnDisabled || (pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive())) && pPlayer->GetTeam() == m_Team && GameServer()->Teams()->m_Core.SameTeam(m_ClientID, pPlayer->GetCID());
}

void CPlayer::UpdateDeadSpecMode()
{
	// check if actual spectator id is valid
	if(m_SpectatorID != -1 && GameServer()->m_apPlayers[m_SpectatorID] && DeadCanFollow(GameServer()->m_apPlayers[m_SpectatorID]))
		return;

	// find player to follow
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(GameServer()->m_apPlayers[i] && DeadCanFollow(GameServer()->m_apPlayers[i]))
		{
			m_SpectatorID = i;
			return;
		}
	}

	// no one available to follow -> turn spectator mode off
	m_DeadSpecMode = false;
}

void CPlayer::Tick()
{
#ifdef CONF_DEBUG
	if(!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS - g_Config.m_DbgDummies)
#endif

		if(!Server()->ClientIngame(m_ClientID))
			return;

	if(m_ChatScore > 0)
		m_ChatScore--;

	SGameInstance Instance = GameServer()->PlayerGameInstance(m_ClientID);
	if(Instance.m_Init && Instance.m_pController->IsDisruptiveLeave(m_ClientID))
		Server()->SetDisruptiveLeave(m_ClientID, true);
	else
		Server()->SetDisruptiveLeave(m_ClientID, false);

	Server()->SetClientScore(m_ClientID, m_Score);

	if(m_Moderating && m_Afk)
	{
		m_Moderating = false;
		GameServer()->SendChatTarget(m_ClientID, "Active moderator mode disabled because you are afk.");

		if(!GameServer()->PlayerModerating())
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, "Server kick/spec votes are no longer actively moderated.");
	}

	// do latency stuff
	{
		IServer::CClientInfo Info;
		if(Server()->GetClientInfo(m_ClientID, &Info))
		{
			m_Latency.m_Accum += Info.m_Latency;
			m_Latency.m_AccumMax = maximum(m_Latency.m_AccumMax, Info.m_Latency);
			m_Latency.m_AccumMin = minimum(m_Latency.m_AccumMin, Info.m_Latency);
		}
		// each second
		if(Server()->Tick() % Server()->TickSpeed() == 0)
		{
			m_Latency.m_Avg = m_Latency.m_Accum / Server()->TickSpeed();
			m_Latency.m_Max = m_Latency.m_AccumMax;
			m_Latency.m_Min = m_Latency.m_AccumMin;
			m_Latency.m_Accum = 0;
			m_Latency.m_AccumMin = 1000;
			m_Latency.m_AccumMax = 0;
		}
	}

	if(Server()->GetNetErrorString(m_ClientID)[0])
	{
		m_Afk = true;

		if(Server()->GetNetErrorString(m_ClientID)[0] != 'R')
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "'%s' would have timed out, but can use timeout protection now", Server()->ClientName(m_ClientID));
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
			Server()->ResetNetErrorString(m_ClientID);
		}
		else
		{
			Server()->SetLeftDisruptively(m_ClientID);
		}
	}

	if(GameServer()->PlayerGameInstance(m_ClientID).m_Init)
	{
		if(!m_pCharacter && IsSpectating() && m_SpecMode == SPEC_FREEVIEW)
			m_ViewPos -= vec2(clamp(m_ViewPos.x - m_LatestActivity.m_TargetX, -500.0f, 500.0f), clamp(m_ViewPos.y - m_LatestActivity.m_TargetY, -400.0f, 400.0f));

		if(!m_pCharacter && IsSpectating() && m_SpecMode == SPEC_FREEVIEW)
			m_ViewPos -= vec2(clamp(m_ViewPos.x - m_LatestActivity.m_TargetX, -500.0f, 500.0f), clamp(m_ViewPos.y - m_LatestActivity.m_TargetY, -400.0f, 400.0f));

		if(!m_pCharacter && m_DieTick + Server()->TickSpeed() * 3 <= Server()->Tick() && !m_DeadSpecMode)
			Respawn();

		if(!m_pCharacter && IsSpectating() && m_pSpecFlag)
		{
			if(m_pSpecFlag->GetCarrier())
				m_SpectatorID = m_pSpecFlag->GetCarrier()->GetPlayer()->GetCID();
			else
			{
				m_ViewPos = m_pSpecFlag->m_Pos;
				m_SpectatorID = -1;
			}
		}

		if(m_pCharacter)
		{
			if(m_pCharacter->IsAlive())
			{
				ProcessPause();
				if(!m_Paused)
					m_ViewPos = m_pCharacter->m_Pos;
			}
			else if(!m_pCharacter->IsDisabled())
			{
				delete m_pCharacter;
				m_pCharacter = 0;
			}
		}
		else if(m_Spawning && m_RespawnTick <= Server()->Tick())
			TryRespawn();

		if(!m_DeadSpecMode && m_LastActionTick != Server()->Tick())
			++m_InactivityTickCounter;
	}
	else
	{
		++m_RespawnTick;
		++m_DieTick;
		++m_PreviousDieTick;
		++m_JoinTick;
		++m_LastActionTick;
		++m_TeamChangeTick;
	}

	m_TuneZoneOld = m_TuneZone; // determine needed tunings with viewpos
	int CurrentIndex = GameServer()->Collision()->GetMapIndex(m_ViewPos);
	m_TuneZone = GameServer()->Collision()->IsTune(CurrentIndex);

	if(m_TuneZone != m_TuneZoneOld) // don't send tunings all the time
	{
		GameServer()->SendTuningParams(m_ClientID, m_TuneZone);
	}

	if(m_OverrideEmoteReset >= 0 && m_OverrideEmoteReset <= Server()->Tick())
	{
		m_OverrideEmoteReset = -1;
	}

	if(m_Halloween && m_pCharacter && !m_pCharacter->IsDisabled())
	{
		if(1200 - ((Server()->Tick() - m_pCharacter->GetLastAction()) % (1200)) < 5)
		{
			GameServer()->SendEmoticon(GetCID(), EMOTICON_GHOST);
		}
	}
}

void CPlayer::PostTick()
{
	// update latency value
	if(m_PlayerFlags & PLAYERFLAG_SCOREBOARD)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->IsClientPlayer(i))
				m_aActLatency[i] = GameServer()->m_apPlayers[i]->m_Latency.m_Min;
		}
	}

	// update view pos for spectators
	if((IsSpectating() || m_DeadSpecMode) && m_SpectatorID != SPEC_FREEVIEW && GameServer()->m_apPlayers[m_SpectatorID] && GameServer()->m_apPlayers[m_SpectatorID]->GetCharacter())
		m_ViewPos = GameServer()->m_apPlayers[m_SpectatorID]->GetCharacter()->m_Pos;
}

void CPlayer::Snap(int SnappingClient)
{
#ifdef CONF_DEBUG
	if(!g_Config.m_DbgDummies || m_ClientID < MAX_CLIENTS - g_Config.m_DbgDummies)
#endif
		if(!Server()->ClientIngame(m_ClientID))
			return;

	int MappedID = m_ClientID;
	if(SnappingClient > -1 && !Server()->Translate(MappedID, SnappingClient))
		return;

	CPlayer *pSnappingPlayer = GameServer()->m_apPlayers[SnappingClient];
	int SnapAs = SnappingClient;
	if(pSnappingPlayer->IsSpectating() && pSnappingPlayer->GetSpectatorID() > SPEC_FREEVIEW)
		SnapAs = pSnappingPlayer->m_SpectatorID; // Snap as spectating player.

	CNetObj_ClientInfo *pClientInfo = static_cast<CNetObj_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, MappedID, sizeof(CNetObj_ClientInfo)));
	if(!pClientInfo)
		return;

	// MYTODO: check override names
	if(m_aOverrideName[0])
		StrToInts(&pClientInfo->m_Name0, 4, m_aOverrideName);
	else
		StrToInts(&pClientInfo->m_Name0, 4, Server()->ClientName(m_ClientID));

	if(m_aOverrideClan[0])
		StrToInts(&pClientInfo->m_Name0, 4, m_aOverrideClan);
	else
		StrToInts(&pClientInfo->m_Clan0, 3, Server()->ClientClan(m_ClientID));

	pClientInfo->m_Country = Server()->ClientCountry(m_ClientID);

	// check override skins
	if(m_OverrideTeeInfos.m_SkinName[0])
		StrToInts(&pClientInfo->m_Skin0, 6, m_OverrideTeeInfos.m_SkinName);
	else
		StrToInts(&pClientInfo->m_Skin0, 6, m_TeeInfos.m_SkinName);

	if(m_OverrideTeeInfos.m_UseCustomColor)
	{
		pClientInfo->m_UseCustomColor = m_OverrideTeeInfos.m_UseCustomColor;
		pClientInfo->m_ColorBody = m_OverrideTeeInfos.m_ColorBody;
		pClientInfo->m_ColorFeet = m_OverrideTeeInfos.m_ColorFeet;
	}
	else
	{
		pClientInfo->m_UseCustomColor = m_TeeInfos.m_UseCustomColor;
		pClientInfo->m_ColorBody = m_TeeInfos.m_ColorBody;
		pClientInfo->m_ColorFeet = m_TeeInfos.m_ColorFeet;
	}

	int ClientVersion = GetClientVersion();
	int Latency = SnappingClient == -1 ? m_Latency.m_Min : GameServer()->m_apPlayers[SnappingClient]->m_aActLatency[m_ClientID];
	int Score = m_Score;
	bool IsEndRound = false;
	bool IsEndMatch = false;
	SGameInstance Instance = GameServer()->PlayerGameInstance(m_ClientID);
	if(Instance.m_Init)
	{
		if(Instance.m_pController->IsEndRound())
			IsEndRound = true;
		if(Instance.m_pController->IsEndMatch())
			IsEndMatch = true;
	}

	if(SnappingClient < 0 || !Server()->IsSixup(SnappingClient))
	{
		CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, MappedID, sizeof(CNetObj_PlayerInfo)));
		if(!pPlayerInfo)
			return;

		pPlayerInfo->m_Latency = Latency;
		pPlayerInfo->m_Score = Score;
		pPlayerInfo->m_ClientID = MappedID;
		pPlayerInfo->m_Local = (int)(m_ClientID == SnappingClient);
		pPlayerInfo->m_Team = m_Team;

		if(m_ClientID == SnappingClient && (m_Paused || (m_DeadSpecMode && !IsEndMatch) || ((!m_pCharacter || !m_pCharacter->IsAlive()) && Controller() && Controller()->IsGamePaused())))
		{
			if(ClientVersion < VERSION_DDNET_OLD)
				pPlayerInfo->m_Local = false;
			pPlayerInfo->m_Team = TEAM_SPECTATORS;
		}

		if(GameServer()->GetDDRaceTeam(SnapAs) != GameServer()->GetDDRaceTeam(m_ClientID) && !GameServer()->m_apPlayers[SnappingClient]->ShowOthersMode())
			pPlayerInfo->m_Team = TEAM_SPECTATORS;

		bool DeadAndNoRespawn = m_RespawnDisabled && (!m_pCharacter || !m_pCharacter->IsAlive());
		if(!IsEndMatch && ((m_ClientID != SnappingClient || IsEndRound) && DeadAndNoRespawn))
			pPlayerInfo->m_Team = TEAM_SPECTATORS;
	}
	else
	{
		protocol7::CNetObj_PlayerInfo *pPlayerInfo = static_cast<protocol7::CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, MappedID, sizeof(protocol7::CNetObj_PlayerInfo)));
		if(!pPlayerInfo)
			return;

		pPlayerInfo->m_PlayerFlags = PlayerFlags_SixToSeven(m_PlayerFlags);
		if(SnappingClient != -1 && (IsSpectating() || m_DeadSpecMode) && (SnappingClient == m_SpectatorID))
			pPlayerInfo->m_PlayerFlags |= protocol7::PLAYERFLAG_WATCHING;
		if(m_RespawnDisabled && (!GetCharacter() || !GetCharacter()->IsAlive()))
			pPlayerInfo->m_PlayerFlags |= protocol7::PLAYERFLAG_DEAD;
		if(m_IsReadyToPlay)
			pPlayerInfo->m_PlayerFlags |= protocol7::PLAYERFLAG_READY;
		if(Server()->ClientAuthed(m_ClientID))
			pPlayerInfo->m_PlayerFlags |= protocol7::PLAYERFLAG_ADMIN;

		// Times are in milliseconds for 0.7
		pPlayerInfo->m_Score = Score;
		pPlayerInfo->m_Latency = Latency;
	}

	if(m_ClientID == SnappingClient && (IsSpectating() || (m_DeadSpecMode && !IsEndMatch) || IsEndRound))
	{
		if(SnappingClient < 0 || !Server()->IsSixup(SnappingClient))
		{
			CNetObj_SpectatorInfo *pSpectatorInfo = static_cast<CNetObj_SpectatorInfo *>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, m_ClientID, sizeof(CNetObj_SpectatorInfo)));
			if(!pSpectatorInfo)
				return;

			if((m_SpecMode == SPECMODE_FLAGRED || m_SpecMode == SPECMODE_FLAGBLUE) && m_pSpecFlag)
			{
				pSpectatorInfo->m_SpectatorID = MappedID;
				pSpectatorInfo->m_X = round_to_int(m_pSpecFlag->m_Pos.x);
				pSpectatorInfo->m_Y = round_to_int(m_pSpecFlag->m_Pos.y);
			}
			else if(IsEndRound)
			{
				// special case for round end (to show round end broadcast instantly)
				pSpectatorInfo->m_SpectatorID = MappedID;
				pSpectatorInfo->m_X = m_ViewPos.x;
				pSpectatorInfo->m_Y = m_ViewPos.y;
			}
			else
			{
				pSpectatorInfo->m_SpectatorID = m_SpectatorID;
				pSpectatorInfo->m_X = m_ViewPos.x;
				pSpectatorInfo->m_Y = m_ViewPos.y;
			}
		}
		else
		{
			protocol7::CNetObj_SpectatorInfo *pSpectatorInfo = static_cast<protocol7::CNetObj_SpectatorInfo *>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, m_ClientID, sizeof(protocol7::CNetObj_SpectatorInfo)));
			if(!pSpectatorInfo)
				return;

			pSpectatorInfo->m_SpecMode = m_SpecMode;
			pSpectatorInfo->m_SpectatorID = m_SpectatorID;
			pSpectatorInfo->m_X = m_ViewPos.x;
			pSpectatorInfo->m_Y = m_ViewPos.y;
		}
	}

	CNetObj_DDNetPlayer *pDDNetPlayer = static_cast<CNetObj_DDNetPlayer *>(Server()->SnapNewItem(NETOBJTYPE_DDNETPLAYER, MappedID, sizeof(CNetObj_DDNetPlayer)));
	if(!pDDNetPlayer)
		return;

	pDDNetPlayer->m_AuthLevel = Server()->GetAuthedState(m_ClientID);
	pDDNetPlayer->m_Flags = 0;
	if(m_Afk)
		pDDNetPlayer->m_Flags |= EXPLAYERFLAG_AFK;
	if(m_Paused)
		pDDNetPlayer->m_Flags |= EXPLAYERFLAG_PAUSED;

	bool ShowSpec = m_pCharacter && m_pCharacter->IsDisabled();

	if(SnappingClient >= 0)
	{
		CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];
		ShowSpec = ShowSpec && (GameServer()->GetDDRaceTeam(m_ClientID) == GameServer()->GetDDRaceTeam(SnappingClient) || pSnapPlayer->ShowOthersMode() || pSnapPlayer->IsSpectating());
	}

	if(ShowSpec)
	{
		CNetObj_SpecChar *pSpecChar = static_cast<CNetObj_SpecChar *>(Server()->SnapNewItem(NETOBJTYPE_SPECCHAR, MappedID, sizeof(CNetObj_SpecChar)));
		if(!pSpecChar)
			return;

		pSpecChar->m_X = m_pCharacter->Core()->m_Pos.x;
		pSpecChar->m_Y = m_pCharacter->Core()->m_Pos.y;
	}
}

void CPlayer::FakeSnap()
{
	if(GetClientVersion() >= VERSION_DDNET_OLD)
		return;

	if(Server()->IsSixup(m_ClientID))
		return;

	int FakeID = VANILLA_MAX_CLIENTS - 1;

	CNetObj_ClientInfo *pClientInfo = static_cast<CNetObj_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, FakeID, sizeof(CNetObj_ClientInfo)));

	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo->m_Name0, 4, " ");
	StrToInts(&pClientInfo->m_Clan0, 3, "");
	StrToInts(&pClientInfo->m_Skin0, 6, "default");

	if(!m_Paused)
		return;

	CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, FakeID, sizeof(CNetObj_PlayerInfo)));
	if(!pPlayerInfo)
		return;

	pPlayerInfo->m_Latency = m_Latency.m_Min;
	pPlayerInfo->m_Local = 1;
	pPlayerInfo->m_ClientID = FakeID;
	pPlayerInfo->m_Score = 0;
	pPlayerInfo->m_Team = TEAM_SPECTATORS;

	CNetObj_SpectatorInfo *pSpectatorInfo = static_cast<CNetObj_SpectatorInfo *>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, FakeID, sizeof(CNetObj_SpectatorInfo)));
	if(!pSpectatorInfo)
		return;

	pSpectatorInfo->m_SpectatorID = m_SpectatorID;
	pSpectatorInfo->m_X = m_ViewPos.x;
	pSpectatorInfo->m_Y = m_ViewPos.y;
}

void CPlayer::OnDisconnect()
{
	KillCharacter();

	m_Moderating = false;
}

void CPlayer::OnPredictedInput(CNetObj_PlayerInput *NewInput)
{
	// skip the input if chat is active
	if((m_PlayerFlags & PLAYERFLAG_CHATTING) && (NewInput->m_PlayerFlags & PLAYERFLAG_CHATTING))
		return;

	AfkVoteTimer(NewInput);

	m_NumInputs++;

	if(m_pCharacter && !m_Paused)
		m_pCharacter->OnPredictedInput(NewInput);
}

void CPlayer::OnDirectInput(CNetObj_PlayerInput *NewInput)
{
	if(Server()->IsSixup(m_ClientID))
		NewInput->m_PlayerFlags = PlayerFlags_SevenToSix(NewInput->m_PlayerFlags);

	if(NewInput->m_PlayerFlags)
		Server()->SetClientFlags(m_ClientID, NewInput->m_PlayerFlags);

	if(AfkTimer(NewInput->m_TargetX, NewInput->m_TargetY))
		return; // we must return if kicked, as player struct is already deleted
	AfkVoteTimer(NewInput);

	if(!m_pCharacter && IsSpectating() && m_SpectatorID == SPEC_FREEVIEW)
		m_ViewPos = vec2(NewInput->m_TargetX, NewInput->m_TargetY);

	if(NewInput->m_PlayerFlags & PLAYERFLAG_CHATTING)
	{
		// skip the input if chat is active
		if(m_PlayerFlags & PLAYERFLAG_CHATTING)
			return;

		// reset input
		if(m_pCharacter)
			m_pCharacter->ResetInput();

		m_PlayerFlags = NewInput->m_PlayerFlags;
		return;
	}

	m_PlayerFlags = NewInput->m_PlayerFlags;

	if(m_pCharacter && m_Paused)
		m_pCharacter->ResetInput();

	// check for activity
	if(NewInput->m_Direction || m_LatestActivity.m_TargetX != NewInput->m_TargetX ||
		m_LatestActivity.m_TargetY != NewInput->m_TargetY || NewInput->m_Jump ||
		NewInput->m_Fire & 1 || NewInput->m_Hook)
	{
		m_LatestActivity.m_TargetX = NewInput->m_TargetX;
		m_LatestActivity.m_TargetY = NewInput->m_TargetY;
		m_LastActionTick = Server()->Tick();
		m_InactivityTickCounter = 0;
	}
}

void CPlayer::OnPredictedEarlyInput(CNetObj_PlayerInput *NewInput)
{
	// skip the input if chat is active
	if((m_PlayerFlags & PLAYERFLAG_CHATTING) && (NewInput->m_PlayerFlags & PLAYERFLAG_CHATTING))
		return;

	if(m_pCharacter && !m_Paused)
		m_pCharacter->OnDirectInput(NewInput);

	if(!m_pCharacter && m_Team != TEAM_SPECTATORS && (NewInput->m_Fire & 1))
		Respawn();

	if(!m_pCharacter && IsSpectating() && (NewInput->m_Fire & 1))
	{
		if(!m_ActiveSpecSwitch)
		{
			m_ActiveSpecSwitch = true;
			if(m_SpecMode == SPEC_FREEVIEW)
			{
				CCharacter *pChar = (CCharacter *)GameWorld()->ClosestEntity(m_ViewPos, 6.0f * 32, CGameWorld::ENTTYPE_CHARACTER, 0);
				CFlag *pFlag = (CFlag *)GameWorld()->ClosestEntity(m_ViewPos, 6.0f * 32, CGameWorld::ENTTYPE_FLAG, 0);
				if(pChar || pFlag)
				{
					if(!pChar || (pFlag && pChar && distance(m_ViewPos, pFlag->GetPos()) < distance(m_ViewPos, pChar->GetPos())))
					{
						m_SpecMode = pFlag->GetTeam() == TEAM_RED ? SPECMODE_FLAGRED : SPECMODE_FLAGBLUE;
						m_pSpecFlag = pFlag;
						m_SpectatorID = -1;
					}
					else
					{
						m_SpecMode = SPECMODE_PLAYER;
						m_pSpecFlag = 0;
						m_SpectatorID = pChar->GetPlayer()->GetCID();
					}
				}
			}
			else
			{
				m_SpecMode = SPEC_FREEVIEW;
				m_pSpecFlag = 0;
				m_SpectatorID = -1;
			}
		}
	}
	else if(m_ActiveSpecSwitch)
		m_ActiveSpecSwitch = false;
}

int CPlayer::GetClientVersion() const
{
	return m_pGameServer->GetClientVersion(m_ClientID);
}

CCharacter *CPlayer::GetCharacter()
{
	if(m_pCharacter && m_pCharacter->IsAlive())
		return m_pCharacter;
	return 0;
}

void CPlayer::KillCharacter(int Weapon)
{
	if(m_pCharacter)
	{
		m_pCharacter->Die(m_ClientID, Weapon);

		delete m_pCharacter;
		m_pCharacter = 0;
	}
}

void CPlayer::Respawn()
{
	if(m_RespawnDisabled && m_Team != TEAM_SPECTATORS)
	{
		// enable spectate mode for dead players
		m_DeadSpecMode = true;
		m_IsReadyToPlay = true;
		m_SpecMode = SPECMODE_PLAYER;
		UpdateDeadSpecMode();
		return;
	}

	m_DeadSpecMode = false;

	if(m_Team != TEAM_SPECTATORS)
		m_Spawning = true;
}

void CPlayer::CancelSpawn()
{
	m_RespawnDisabled = true;
	m_Spawning = false;
	if(m_Team != TEAM_SPECTATORS)
	{
		m_DeadSpecMode = true;
		m_IsReadyToPlay = true;
		m_SpecMode = SPECMODE_PLAYER;
		UpdateDeadSpecMode();
		return;
	}
}

CCharacter *CPlayer::ForceSpawn(vec2 Pos)
{
	m_Spawning = false;
	m_pCharacter = new(m_ClientID) CCharacter(GameWorld());
	m_pCharacter->Spawn(this, Pos);
	m_Team = 0;
	return m_pCharacter;
}

void CPlayer::SetTeam(int Team)
{
	KillCharacter();

	m_Team = Team;
	m_LastSetTeam = Server()->Tick();
	m_LastActionTick = Server()->Tick();
	m_SpectatorID = SPEC_FREEVIEW;

	protocol7::CNetMsg_Sv_Team Msg;
	Msg.m_ClientID = m_ClientID;
	Msg.m_Team = m_Team;
	Msg.m_Silent = true; // change team is always silent for sixup since we have custom message for that.
	Msg.m_CooldownTick = m_LastSetTeam + Server()->TickSpeed() * g_Config.m_SvRoomChangeDelay;

	// Update team info for sixup
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!Server()->IsSixup(i) || !GameServer()->PlayerExists(i) || !Server()->ClientIngame(i))
			continue;

		CPlayer *pTargetPlayer = GameServer()->m_apPlayers[i];
		if(GameServer()->GetDDRaceTeam(i) != GameServer()->GetDDRaceTeam(m_ClientID) && !pTargetPlayer->ShowOthersMode())
			Msg.m_Team = TEAM_SPECTATORS;
		else
			Msg.m_Team = m_Team;

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
	}

	if(Team == TEAM_SPECTATORS)
	{
		// update spectator modes
		for(auto &pPlayer : GameServer()->m_apPlayers)
		{
			if(pPlayer && pPlayer->m_SpectatorID == m_ClientID)
				pPlayer->m_SpectatorID = SPEC_FREEVIEW;
		}
	}
}

void CPlayer::SendCurrentTeamInfo()
{
	if(!Server()->IsSixup(m_ClientID))
		return;

	// Update team info for sixup
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == m_ClientID || !GameServer()->PlayerExists(i) || !Server()->ClientIngame(i))
			continue;

		CPlayer *pPlayer = GameServer()->m_apPlayers[i];

		protocol7::CNetMsg_Sv_Team Msg;
		Msg.m_ClientID = pPlayer->m_ClientID;
		Msg.m_Team = pPlayer->m_Team;
		Msg.m_Silent = true;
		Msg.m_CooldownTick = pPlayer->m_LastSetTeam + Server()->TickSpeed() * g_Config.m_SvRoomChangeDelay;
		if(GameServer()->GetDDRaceTeam(i) != GameServer()->GetDDRaceTeam(m_ClientID) && !ShowOthersMode())
			Msg.m_Team = TEAM_SPECTATORS;

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, m_ClientID);
	}
}

bool CPlayer::SetTimerType(int TimerType)
{
	if(TimerType == TIMERTYPE_DEFAULT)
	{
		if(Server()->IsSixup(m_ClientID))
			m_TimerType = TIMERTYPE_SIXUP;
		else
			SetTimerType(g_Config.m_SvDefaultTimerType);

		return true;
	}

	if(Server()->IsSixup(m_ClientID))
	{
		if(TimerType == TIMERTYPE_SIXUP || TimerType == TIMERTYPE_NONE)
		{
			m_TimerType = TimerType;
			return true;
		}
		else
			return false;
	}

	if(TimerType == TIMERTYPE_GAMETIMER)
	{
		if(GetClientVersion() >= VERSION_DDNET_GAMETICK)
			m_TimerType = TimerType;
		else
			return false;
	}
	else if(TimerType == TIMERTYPE_GAMETIMER_AND_BROADCAST)
	{
		if(GetClientVersion() >= VERSION_DDNET_GAMETICK)
			m_TimerType = TimerType;
		else
		{
			m_TimerType = TIMERTYPE_BROADCAST;
			return false;
		}
	}
	else
		m_TimerType = TimerType;

	return true;
}

void CPlayer::TryRespawn()
{
	vec2 SpawnPos;

	if(!GameServer()->PlayerGameInstance(m_ClientID).m_Init || !Controller() || !Controller()->CanSpawn(m_Team, &SpawnPos))
		return;

	m_Spawning = false;
	m_pCharacter = new(m_ClientID) CCharacter(GameWorld());
	m_pCharacter->Spawn(this, SpawnPos);
	m_ViewPos = SpawnPos;
	GameWorld()->CreatePlayerSpawn(SpawnPos);
}

bool CPlayer::AfkTimer(int NewTargetX, int NewTargetY)
{
	/*
		afk timer (x, y = mouse coordinates)
		Since a player has to move the mouse to play, this is a better method than checking
		the player's position in the game world, because it can easily be bypassed by just locking a key.
		Frozen players could be kicked as well, because they can't move.
		It also works for spectators.
		returns true if kicked
	*/

	if(Server()->GetAuthedState(m_ClientID))
		return false; // don't kick admins
	if(g_Config.m_SvMaxAfkTime == 0)
		return false; // 0 = disabled

	if(NewTargetX != m_LastTarget_x || NewTargetY != m_LastTarget_y)
	{
		UpdatePlaytime();
		m_LastTarget_x = NewTargetX;
		m_LastTarget_y = NewTargetY;
		m_Sent1stAfkWarning = 0; // afk timer's 1st warning after 50% of sv_max_afk_time
		m_Sent2ndAfkWarning = 0;
	}
	else
	{
		if(!m_Paused)
		{
			// not playing, check how long
			if(m_Sent1stAfkWarning == 0 && m_LastPlaytime < time_get() - time_freq() * (int)(g_Config.m_SvMaxAfkTime * 0.5))
			{
				str_format(m_pAfkMsg, sizeof(m_pAfkMsg),
					"You have been afk for %d seconds now. Please note that you get kicked after not playing for %d seconds.",
					(int)(g_Config.m_SvMaxAfkTime * 0.5),
					g_Config.m_SvMaxAfkTime);
				m_pGameServer->SendChatTarget(m_ClientID, m_pAfkMsg);
				m_Sent1stAfkWarning = 1;
			}
			else if(m_Sent2ndAfkWarning == 0 && m_LastPlaytime < time_get() - time_freq() * (int)(g_Config.m_SvMaxAfkTime * 0.9))
			{
				str_format(m_pAfkMsg, sizeof(m_pAfkMsg),
					"You have been afk for %d seconds now. Please note that you get kicked after not playing for %d seconds.",
					(int)(g_Config.m_SvMaxAfkTime * 0.9),
					g_Config.m_SvMaxAfkTime);
				m_pGameServer->SendChatTarget(m_ClientID, m_pAfkMsg);
				m_Sent2ndAfkWarning = 1;
			}
			else if(m_LastPlaytime < time_get() - time_freq() * g_Config.m_SvMaxAfkTime)
			{
				m_pGameServer->Server()->Kick(m_ClientID, "Away from keyboard");
				return true;
			}
		}
	}
	return false;
}

void CPlayer::UpdatePlaytime()
{
	m_LastPlaytime = time_get();
}

void CPlayer::AfkVoteTimer(CNetObj_PlayerInput *NewTarget)
{
	if(g_Config.m_SvMaxAfkVoteTime == 0)
		return;

	if(!m_pLastTarget)
	{
		m_pLastTarget = new CNetObj_PlayerInput(*NewTarget);
		m_LastPlaytime = 0;
		m_Afk = true;
		return;
	}
	else if(mem_comp(NewTarget, m_pLastTarget, sizeof(CNetObj_PlayerInput)) != 0)
	{
		UpdatePlaytime();
		mem_copy(m_pLastTarget, NewTarget, sizeof(CNetObj_PlayerInput));
	}
	else if(m_LastPlaytime < time_get() - time_freq() * g_Config.m_SvMaxAfkVoteTime)
	{
		m_Afk = true;
		return;
	}

	m_Afk = false;
}

int CPlayer::GetDefaultEmote() const
{
	if(m_OverrideEmoteReset >= 0)
		return m_OverrideEmote;

	return m_DefEmote;
}

void CPlayer::OverrideDefaultEmote(int Emote, int Tick)
{
	m_OverrideEmote = Emote;
	m_OverrideEmoteReset = Tick;
	m_LastEyeEmote = Server()->Tick();
}

bool CPlayer::CanOverrideDefaultEmote() const
{
	return m_LastEyeEmote == 0 || m_LastEyeEmote + (int64)g_Config.m_SvEyeEmoteChangeDelay * Server()->TickSpeed() < Server()->Tick();
}

// MYTODO: combine send info across multiple info changes
void CPlayer::OverrideName(const char *pName)
{
	bool Updated = false;
	if(!pName || !pName[0])
	{
		if(m_aOverrideName[0])
		{
			m_aOverrideName[0] = 0;
			Updated = true;
		}
	}
	else
	{
		str_copy(m_aOverrideName, pName, sizeof(m_aOverrideName));
		Updated = true;
	}

	if(Updated)
		GameServer()->SendClientInfo(m_ClientID);
}

void CPlayer::OverrideClan(const char *pClan)
{
	bool Updated = false;
	if(!pClan || !pClan[0])
	{
		if(m_aOverrideClan[0])
		{
			m_aOverrideClan[0] = 0;
			Updated = true;
		}
	}
	else
	{
		str_copy(m_aOverrideClan, pClan, sizeof(m_aOverrideClan));
		Updated = true;
	}

	if(Updated)
		GameServer()->SendClientInfo(m_ClientID);
}

void CPlayer::OverrideSkin(const char *pSkinName)
{
	bool Updated = false;
	if(!pSkinName || !pSkinName[0])
	{
		if(m_OverrideTeeInfos.m_SkinName[0])
		{
			m_OverrideTeeInfos.m_SkinName[0] = 0;
			for(int p = 0; p < 6; p++)
				m_OverrideTeeInfos.m_apSkinPartNames[p][0] = 0;
			Updated = true;
		}
	}
	else
	{
		str_copy(m_OverrideTeeInfos.m_SkinName, pSkinName, sizeof(m_OverrideTeeInfos.m_SkinName));
		m_OverrideTeeInfos.SkinToSixup();
		Updated = true;
	}

	if(Updated)
		GameServer()->SendSkinInfo(m_ClientID);
}

void CPlayer::OverrideSkinColor(bool Custom, ColorHSLA Color)
{
	bool Updated = false;
	if(Custom)
	{
		for(int p = 0; p < 6; p++)
		{
			m_OverrideTeeInfos.m_aUseCustomColors[p] = m_TeeInfos.m_aUseCustomColors[p];
			m_OverrideTeeInfos.m_aSkinPartColors[p] = m_TeeInfos.m_aSkinPartColors[p];
		}
		m_OverrideTeeInfos.m_UseCustomColor = true;
		m_OverrideTeeInfos.m_ColorBody = Color.Pack(ColorHSLA::DARKEST_LGT);
		m_OverrideTeeInfos.m_ColorFeet = Color.Pack(ColorHSLA::DARKEST_LGT);
		m_OverrideTeeInfos.ColorToSixup();
		Updated = true;
	}
	else
	{
		if(m_OverrideTeeInfos.m_UseCustomColor)
		{
			m_OverrideTeeInfos.m_UseCustomColor = false;
			for(int p = 0; p < 6; p++)
				m_OverrideTeeInfos.m_aUseCustomColors[p] = false;
			m_OverrideTeeInfos.m_ColorBody = m_TeeInfos.m_ColorBody;
			m_OverrideTeeInfos.m_ColorFeet = m_TeeInfos.m_ColorFeet;
			Updated = true;
		}
	}
	if(Updated)
		GameServer()->SendSkinInfo(m_ClientID);
}

void CPlayer::ProcessPause()
{
}

int CPlayer::Pause(bool Paused, bool Force)
{
	if(!m_pCharacter)
		return 0;

	if(Paused != m_Paused)
	{
		// Update state
		m_Paused = Paused;
		m_LastPause = Server()->Tick();

		// Sixup needs a teamchange
		protocol7::CNetMsg_Sv_Team Msg;
		Msg.m_ClientID = m_ClientID;
		Msg.m_CooldownTick = Server()->Tick();
		Msg.m_Silent = true;
		Msg.m_Team = m_Paused ? protocol7::TEAM_SPECTATORS : m_Team;

		GameServer()->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, m_ClientID);
	}

	return m_Paused;
}

bool CPlayer::IsPaused()
{
	return m_Paused;
}

bool CPlayer::IsPlaying()
{
	if(m_pCharacter && m_pCharacter->IsAlive())
		return true;
	return false;
}

void CPlayer::SpectatePlayerName(const char *pName)
{
	if(!pName)
		return;

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(i != m_ClientID && Server()->ClientIngame(i) && !str_comp(pName, Server()->ClientName(i)))
		{
			m_SpectatorID = i;
			return;
		}
	}
}
