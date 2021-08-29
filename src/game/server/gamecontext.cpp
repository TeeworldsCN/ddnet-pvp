/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/tl/sorted_array.h>

#include "gamecontext.h"
#include "teeinfo.h"
#include <antibot/antibot_data.h>
#include <base/math.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/map.h>
#include <engine/server/server.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/shared/linereader.h>
#include <engine/shared/memheap.h>
#include <engine/storage.h>
#include <game/collision.h>
#include <game/gamecore.h>
#include <game/version.h>
#include <string.h>

#include <game/generated/protocol7.h>
#include <game/generated/protocolglue.h>

#include "entities/character.h"
#include "player.h"
#include "teams.h"

#include <game/localization.h>

#define TESTTYPE_NAME "TestDDPvP"

enum
{
	RESET,
	NO_RESET
};

int64 CGameContext::ms_TeamMask[3] = {0};
int64 CGameContext::ms_SpectatorMask[MAX_CLIENTS] = {0};
int64 CGameContext::ms_TeamSpectatorMask[2] = {0};

void CGameContext::Construct(int Resetting)
{
	m_Resetting = 0;
	m_pServer = 0;

	for(auto &pPlayer : m_apPlayers)
		pPlayer = 0;

	m_VoteType = VOTE_TYPE_UNKNOWN;
	m_VoteCloseTime = 0;
	m_pVoteOptionFirst = 0;
	m_pVoteOptionLast = 0;
	m_NumVoteOptions = 0;

	if(Resetting == NO_RESET)
	{
		m_pVoteOptionHeap = new CHeap();
		m_NumMutes = 0;
		m_NumVoteMutes = 0;
	}
	m_ChatResponseTargetID = -1;
	m_aDeleteTempfile[0] = 0;
}

CGameContext::CGameContext(int Resetting)
{
	Construct(Resetting);
}

CGameContext::CGameContext()
{
	Construct(NO_RESET);
}

CGameContext::~CGameContext()
{
	for(auto &pPlayer : m_apPlayers)
		delete pPlayer;
	if(!m_Resetting)
		delete m_pVoteOptionHeap;
}

void CGameContext::Clear()
{
	CHeap *pVoteOptionHeap = m_pVoteOptionHeap;
	CVoteOptionServer *pVoteOptionFirst = m_pVoteOptionFirst;
	CVoteOptionServer *pVoteOptionLast = m_pVoteOptionLast;
	int NumVoteOptions = m_NumVoteOptions;
	CTuningParams Tuning = m_Tuning;

	m_Resetting = true;
	this->~CGameContext();
	mem_zero(this, sizeof(*this));
	new(this) CGameContext(RESET);

	m_pVoteOptionHeap = pVoteOptionHeap;
	m_pVoteOptionFirst = pVoteOptionFirst;
	m_pVoteOptionLast = pVoteOptionLast;
	m_NumVoteOptions = NumVoteOptions;
	m_Tuning = Tuning;
}

class CCharacter *CGameContext::GetPlayerChar(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || !m_apPlayers[ClientID])
		return 0;
	return m_apPlayers[ClientID]->GetCharacter();
}

SGameInstance CGameContext::GameInstance(int Team)
{
	return Teams()->GetGameInstance(Team);
}

SGameInstance CGameContext::PlayerGameInstance(int ClientID)
{
	return Teams()->GetGameInstance(Teams()->m_Core.Team(ClientID));
}

int CGameContext::GetPlayerDDRTeam(int ClientID)
{
	return Teams()->m_Core.Team(ClientID);
}

bool CGameContext::ChangePlayerReadyState(CPlayer *pPlayer)
{
	if(pPlayer->m_LastReadyChangeTick && pPlayer->m_LastReadyChangeTick + Server()->TickSpeed() > Server()->Tick())
		return false;

	pPlayer->m_LastReadyChangeTick = Server()->Tick();
	SGameInstance Instance = PlayerGameInstance(pPlayer->GetCID());
	if(Instance.m_Init)
		Instance.m_pController->OnPlayerReadyChange(pPlayer);

	return true;
}

void CGameContext::FillAntibot(CAntibotRoundData *pData)
{
	if(!pData->m_Map.m_pTiles)
	{
		Collision()->FillAntibot(&pData->m_Map);
	}
	pData->m_Tick = Server()->Tick();
	mem_zero(pData->m_aCharacters, sizeof(pData->m_aCharacters));
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CAntibotCharacterData *pChar = &pData->m_aCharacters[i];
		for(auto &LatestInput : pChar->m_aLatestInputs)
		{
			LatestInput.m_TargetX = -1;
			LatestInput.m_TargetY = -1;
		}
		pChar->m_Alive = false;
		pChar->m_Pause = false;
		pChar->m_Team = -1;

		pChar->m_Pos = vec2(-1, -1);
		pChar->m_Vel = vec2(0, 0);
		pChar->m_Angle = -1;
		pChar->m_HookedPlayer = -1;
		pChar->m_SpawnTick = -1;
		pChar->m_WeaponChangeTick = -1;

		if(m_apPlayers[i])
		{
			str_copy(pChar->m_aName, Server()->ClientName(i), sizeof(pChar->m_aName));
			CCharacter *pGameChar = m_apPlayers[i]->GetCharacter();
			pChar->m_Alive = (bool)pGameChar;
			pChar->m_Pause = m_apPlayers[i]->IsPaused();
			pChar->m_Team = m_apPlayers[i]->GetTeam();
			if(pGameChar)
			{
				pGameChar->FillAntibot(pChar);
			}
		}
	}
}

void CGameContext::CallVote(int ClientID, const char *pDesc, const char *pCmd, const char *pReason, const char *pChatmsg, const char *pSixupDesc)
{
	// check if a vote is already running
	if(m_VoteCloseTime)
		return;

	int64 Now = Server()->Tick();
	CPlayer *pPlayer = m_apPlayers[ClientID];

	if(!pPlayer)
		return;

	SendChat(-1, CGameContext::CHAT_ALL, pChatmsg, -1, CHAT_SIX);
	if(!pSixupDesc)
		pSixupDesc = pDesc;

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		SGameInstance Instance = GameInstance(i);
		if(Instance.m_IsCreated && Instance.m_pController->IsVoting())
		{
			Instance.m_pController->EndVote(false);
			Instance.m_pController->SendChatTarget(-1, "This room's vote has been cancelled due to a server vote being called");
		}
	}

	m_VoteCreator = ClientID;
	StartVote(pDesc, pCmd, pReason, pSixupDesc);
	pPlayer->m_Vote = 1;
	pPlayer->m_VotePos = m_VotePos = 1;
	pPlayer->m_LastVoteCall = Now;
}

void CGameContext::UpdatePlayerLang(int ClientID, int Lang, bool IsInfoUpdate)
{
	if(m_apPlayers[ClientID]->m_Lang == Lang)
		return;
	m_apPlayers[ClientID]->m_Lang = Lang;
	char aBuf[256];
	if(IsInfoUpdate)
	{
		if(Lang != 0)
		{
			str_format(aBuf, sizeof(aBuf), "Server language has been set to '%s'.", "Chinese");
			SendChatTarget(ClientID, aBuf);
			str_format(aBuf, sizeof(aBuf), "Use /lang to change langauge.");
			SendChatTarget(ClientID, aBuf);
		}
		SendChatLocalized(ClientID, "Server language has been set to '%s'.", "简体中文");
		SendChatLocalized(ClientID, "Use /lang to change langauge.");
	}
}

const char *CGameContext::LocalizeFor(int ClientID, const char *pString, const char *pContext)
{
	if(!m_apPlayers[ClientID])
		return pString;

	CLocalizedString *pLocString = LocalizeServer(pString, pContext);
	int Lang = m_apPlayers[ClientID]->m_Lang;

	if(pLocString && pLocString->m_Langs[Lang])
		return pLocString->m_Langs[Lang];

	return pString;
}

void CGameContext::SendChatLocalizedVL(int To, int Flags, ContextualString String, va_list ap)
{
	int Start = To;
	int Limit = To + 1;
	if(To < 0)
	{
		Start = 0;
		Limit = MAX_CLIENTS;
	}

	char aBuf[512];
	int CurLang = -1;
	CLocalizedString *pString = LocalizeServer(String.m_pFormat, String.m_pContext);

	if(!pString)
	{
		CurLang = 0;
		str_vformat(aBuf, sizeof(aBuf), String.m_pFormat, ap);
	}

	for(int i = Start; i < Limit; i++)
	{
		if(!m_apPlayers[i])
			return;
		int Lang = m_apPlayers[i]->m_Lang;
		if(Lang < 0 || Lang >= 1024)
			Lang = 0;

		if(pString && CurLang != Lang)
		{
			if(pString->m_Langs[Lang])
				str_vformat(aBuf, sizeof(aBuf), pString->m_Langs[Lang], ap);
			else
				str_vformat(aBuf, sizeof(aBuf), String.m_pFormat, ap);
		}

		if(!((Server()->IsSixup(To) && (Flags & CHAT_SIXUP)) ||
			   (!Server()->IsSixup(To) && (Flags & CHAT_SIX))))
			return;

		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 0;
		Msg.m_ClientID = -1;
		Msg.m_pMessage = aBuf;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, To);
	}
}

void CGameContext::SendChatTarget(int To, const char *pText, int Flags)
{
	CNetMsg_Sv_Chat Msg;
	Msg.m_Team = 0;
	Msg.m_ClientID = -1;
	Msg.m_pMessage = pText;

	if(g_Config.m_SvDemoChat)
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NOSEND, -1);

	if(To == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!((Server()->IsSixup(i) && (Flags & CHAT_SIXUP)) ||
				   (!Server()->IsSixup(i) && (Flags & CHAT_SIX))))
				continue;

			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
		}
	}
	else
	{
		if(!((Server()->IsSixup(To) && (Flags & CHAT_SIXUP)) ||
			   (!Server()->IsSixup(To) && (Flags & CHAT_SIX))))
			return;

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, To);
	}
}

void CGameContext::SendChat(int ChatterClientID, int Team, const char *pText, int SpamProtectionClientID, int Flags)
{
	if(SpamProtectionClientID >= 0 && SpamProtectionClientID < MAX_CLIENTS)
		if(ProcessSpamProtection(SpamProtectionClientID))
			return;

	// prevent spoofing room number
	if(ChatterClientID >= 0 && ChatterClientID < MAX_CLIENTS && str_startswith(pText, "Test"))
		return;

	int Room = -1;
	if(ChatterClientID >= 0 && ChatterClientID < MAX_CLIENTS)
		Room = GetPlayerDDRTeam(ChatterClientID);

	// leave space for room number in aText "Test00]: "
	char aBuf[256], aText[256 - 7], aRoomedText[256];
	str_utf8_copy(aText, pText, sizeof(aText));
	if(ChatterClientID >= 0 && ChatterClientID < MAX_CLIENTS)
		str_format(aBuf, sizeof(aBuf), "%d:%d:%s: %s", ChatterClientID, Team, Server()->ClientName(ChatterClientID), aText);
	else if(ChatterClientID == -2)
	{
		str_format(aBuf, sizeof(aBuf), "### %s", aText);
		str_utf8_copy(aText, aBuf, sizeof(aText));
		ChatterClientID = -1;
	}
	else
		str_format(aBuf, sizeof(aBuf), "*** %s", aText);
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, Team != CHAT_ALL ? "teamchat" : "chat", aBuf);

	if(Team == CHAT_ALL)
	{
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 0;
		Msg.m_ClientID = ChatterClientID;
		Msg.m_pMessage = aText;
		if(Room >= 0)
		{
			str_format(aBuf, sizeof(aBuf), "Test%02d]: %s", Room, aText);
			str_utf8_copy(aRoomedText, aBuf, sizeof(aRoomedText));
		}

		// pack one for the recording only
		if(g_Config.m_SvDemoChat)
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NOSEND, -1);

		// send to the clients
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!m_apPlayers[i])
				continue;
			bool Send = (Server()->IsSixup(i) && (Flags & CHAT_SIXUP)) ||
				    (!Server()->IsSixup(i) && (Flags & CHAT_SIX));

			if(Room >= 0 && Room != GetPlayerDDRTeam(i))
				Msg.m_pMessage = aRoomedText;
			else
				Msg.m_pMessage = aText;

			if(!m_apPlayers[i]->m_DND && Send)
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
		}
	}
	else
	{
		CTeamsCore *Teams = &m_Teams.m_Core;
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 1;
		Msg.m_ClientID = ChatterClientID;
		Msg.m_pMessage = aText;

		// pack one for the recording only
		if(g_Config.m_SvDemoChat)
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NOSEND, -1);

		// send to the clients
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_apPlayers[i] != 0)
			{
				if(Team == CHAT_SPEC)
				{
					if(m_apPlayers[i]->GetTeam() == CHAT_SPEC)
					{
						Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
					}
				}
				else
				{
					if(Teams->Team(i) == Room && m_apPlayers[i]->GetTeam() == Team)
					{
						Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
					}
				}
			}
		}
	}
}

void CGameContext::SendEmoticon(int ClientID, int Emoticon)
{
	CNetMsg_Sv_Emoticon Msg;
	Msg.m_ClientID = ClientID;
	Msg.m_Emoticon = Emoticon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
}

void CGameContext::SendWeaponPickup(int ClientID, int Weapon)
{
	CNetMsg_Sv_WeaponPickup Msg;
	Msg.m_Weapon = Weapon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendMotd(int ClientID)
{
	CNetMsg_Sv_Motd Msg;
	Msg.m_pMessage = g_Config.m_SvMotd;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendSettings(int ClientID)
{
	if(Server()->IsSixup(ClientID))
	{
		protocol7::CNetMsg_Sv_ServerSettings Msg;
		Msg.m_KickVote = g_Config.m_SvVoteKick;
		Msg.m_KickMin = g_Config.m_SvVoteKickMin;
		Msg.m_SpecVote = g_Config.m_SvVoteSpectate;
		Msg.m_TeamLock = 0;
		Msg.m_TeamBalance = 0;
		Msg.m_PlayerSlots = g_Config.m_SvMaxClients;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
	}
}

void CGameContext::SendBroadcast(const char *pText, int ClientID, bool IsImportant)
{
	CNetMsg_Sv_Broadcast Msg;
	Msg.m_pMessage = pText;

	if(ClientID == -1)
	{
		dbg_assert(IsImportant, "broadcast messages to all players must be important");
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);

		for(auto &pPlayer : m_apPlayers)
		{
			if(pPlayer)
			{
				pPlayer->m_LastBroadcastImportance = true;
				pPlayer->m_LastBroadcast = Server()->Tick();
			}
		}
		return;
	}

	if(!m_apPlayers[ClientID])
		return;

	if(!IsImportant && m_apPlayers[ClientID]->m_LastBroadcastImportance && m_apPlayers[ClientID]->m_LastBroadcast > Server()->Tick() - Server()->TickSpeed() * 10)
		return;

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
	m_apPlayers[ClientID]->m_LastBroadcast = Server()->Tick();
	m_apPlayers[ClientID]->m_LastBroadcastImportance = IsImportant;
}

void CGameContext::SendBroadcastLocalizedVL(int ClientID, int Line, bool IsImportant, ContextualString String, va_list ap)
{
	// not finished
	// `Line` not used
	char *aBuf = new char[512];
	int CurLang = -1;
	CLocalizedString *pString = LocalizeServer(String.m_pFormat, String.m_pContext);

	if(!pString)
	{
		CurLang = 0;
		str_vformat(aBuf, sizeof(aBuf), String.m_pFormat, ap);
	}

	// send to all clients
	if(ClientID == -1)
	{
		dbg_assert(IsImportant, "broadcast messages to all players must be important");
		for(auto& pPlayer : m_apPlayers)
		{
			if(!pPlayer)
				continue;
			int Lang = pPlayer->m_Lang;
			if(Lang < 0 || Lang >= 1024)
				Lang = 0;

			if(pString && CurLang != Lang)
			{
				if(pString->m_Langs[Lang])
					str_vformat(aBuf, sizeof(aBuf), pString->m_Langs[Lang], ap);
				else
					str_vformat(aBuf, sizeof(aBuf), String.m_pFormat, ap);
			}

			CNetMsg_Sv_Chat Msg;
			Msg.m_Team = 0;
			Msg.m_ClientID = -1;
			Msg.m_pMessage = aBuf;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, pPlayer->GetCID());
		}

		for(auto& pPlayer : m_apPlayers)
		{
			if(!pPlayer)
				continue;
			pPlayer->m_LastBroadcastImportance = true;
			pPlayer->m_LastBroadcast = Server()->Tick();
		}
		delete[] aBuf;
		return;
	}

	if(!m_apPlayers[ClientID])
		return;

	if(!IsImportant && m_apPlayers[ClientID]->m_LastBroadcastImportance && m_apPlayers[ClientID]->m_LastBroadcast > Server()->Tick() - Server()->TickSpeed() * 10)
		return;

	// send to ClientID
	int Lang = m_apPlayers[ClientID]->m_Lang;
	if(Lang < 0 || Lang >= 1024)
		Lang = 0;

	if(pString && CurLang != Lang)
	{
		if(pString->m_Langs[Lang])
			str_vformat(aBuf, sizeof(aBuf), pString->m_Langs[Lang], ap);
		else
			str_vformat(aBuf, sizeof(aBuf), String.m_pFormat, ap);
	}

	CNetMsg_Sv_Chat Msg;
	Msg.m_Team = 0;
	Msg.m_ClientID = -1;
	Msg.m_pMessage = aBuf;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
	m_apPlayers[ClientID]->m_LastBroadcast = Server()->Tick();
	m_apPlayers[ClientID]->m_LastBroadcastImportance = IsImportant;
	delete[] aBuf;
}

void CGameContext::SendCurrentGameInfo(int ClientID, bool IsJoin)
{
	CPlayer *pPlayer = m_apPlayers[ClientID];

	// new info for others
	protocol7::CNetMsg_Sv_ClientInfo NewClientInfoMsg;
	NewClientInfoMsg.m_ClientID = ClientID;
	NewClientInfoMsg.m_Local = 0;
	NewClientInfoMsg.m_Team = pPlayer->GetTeam();
	NewClientInfoMsg.m_pName = Server()->ClientName(ClientID);
	NewClientInfoMsg.m_pClan = Server()->ClientClan(ClientID);
	NewClientInfoMsg.m_Flag = Server()->ClientFlag(ClientID);
	NewClientInfoMsg.m_Silent = true;

	for(int p = 0; p < 6; p++)
	{
		NewClientInfoMsg.m_apSkinPartNames[p] = pPlayer->m_TeeInfos.m_apSkinPartNames[p];
		NewClientInfoMsg.m_aUseCustomColors[p] = pPlayer->m_TeeInfos.m_aUseCustomColors[p];
		NewClientInfoMsg.m_aSkinPartColors[p] = pPlayer->m_TeeInfos.m_aSkinPartColors[p];
	}

	// update client infos (others before local)
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(i == ClientID || !m_apPlayers[i] || !Server()->ClientIngame(i))
			continue;

		CPlayer *pOtherPlayer = m_apPlayers[i];

		if(GetDDRaceTeam(i) != GetDDRaceTeam(ClientID) && !pOtherPlayer->ShowOthersMode())
			NewClientInfoMsg.m_Team = TEAM_SPECTATORS;
		else
			NewClientInfoMsg.m_Team = pPlayer->GetTeam();

		if(IsJoin && Server()->IsSixup(i))
			Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);

		if(Server()->IsSixup(ClientID))
		{
			// existing infos for new player
			protocol7::CNetMsg_Sv_ClientInfo ClientInfoMsg;
			ClientInfoMsg.m_ClientID = i;
			ClientInfoMsg.m_Local = 0;
			ClientInfoMsg.m_Team = pOtherPlayer->GetTeam();
			ClientInfoMsg.m_pName = Server()->ClientName(i);
			ClientInfoMsg.m_pClan = Server()->ClientClan(i);
			ClientInfoMsg.m_Flag = Server()->ClientFlag(i);
			ClientInfoMsg.m_Silent = true;

			if(GetDDRaceTeam(i) != GetDDRaceTeam(ClientID) && !pPlayer->ShowOthersMode())
				ClientInfoMsg.m_Team = TEAM_SPECTATORS;

			for(int p = 0; p < 6; p++)
			{
				ClientInfoMsg.m_apSkinPartNames[p] = pOtherPlayer->m_TeeInfos.m_apSkinPartNames[p];
				ClientInfoMsg.m_aUseCustomColors[p] = pOtherPlayer->m_TeeInfos.m_aUseCustomColors[p];
				ClientInfoMsg.m_aSkinPartColors[p] = pOtherPlayer->m_TeeInfos.m_aSkinPartColors[p];
			}

			Server()->SendPackMsg(&ClientInfoMsg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
		}
	}

	// local info
	if(Server()->IsSixup(ClientID))
	{
		NewClientInfoMsg.m_Local = 1;
		Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
	}
}

void CGameContext::StartVote(const char *pDesc, const char *pCommand, const char *pReason, const char *pSixupDesc)
{
	// reset votes
	m_VoteEnforce = VOTE_ENFORCE_UNKNOWN;
	m_VoteEnforcer = -1;
	for(auto &pPlayer : m_apPlayers)
	{
		if(pPlayer)
		{
			pPlayer->m_Vote = 0;
			pPlayer->m_VotePos = 0;
		}
	}

	// start vote
	m_VoteCloseTime = time_get() + time_freq() * g_Config.m_SvVoteTime;
	str_copy(m_aVoteDescription, pDesc, sizeof(m_aVoteDescription));
	str_copy(m_aSixupVoteDescription, pSixupDesc, sizeof(m_aSixupVoteDescription));
	str_copy(m_aVoteCommand, pCommand, sizeof(m_aVoteCommand));
	str_copy(m_aVoteReason, pReason, sizeof(m_aVoteReason));
	SendVoteSet(-1);
	m_VoteUpdate = true;
}

void CGameContext::EndVote()
{
	m_VoteCloseTime = 0;
	SendVoteSet(-1);
}

bool CGameContext::IsVoting()
{
	return m_VoteCloseTime > 0;
}

void CGameContext::SendVoteSet(int ClientID)
{
	::CNetMsg_Sv_VoteSet Msg6;
	protocol7::CNetMsg_Sv_VoteSet Msg7;

	Msg7.m_ClientID = m_VoteCreator;
	if(m_VoteCloseTime)
	{
		Msg6.m_Timeout = Msg7.m_Timeout = (m_VoteCloseTime - time_get()) / time_freq();
		Msg6.m_pDescription = m_aVoteDescription;
		Msg7.m_pDescription = m_aSixupVoteDescription;
		Msg6.m_pReason = Msg7.m_pReason = m_aVoteReason;

		int &Type = (Msg7.m_Type = protocol7::VOTE_UNKNOWN);
		if(IsKickVote())
			Type = protocol7::VOTE_START_KICK;
		else if(IsSpecVote())
			Type = protocol7::VOTE_START_SPEC;
		else if(IsOptionVote())
			Type = protocol7::VOTE_START_OP;
	}
	else
	{
		Msg6.m_Timeout = Msg7.m_Timeout = 0;
		Msg6.m_pDescription = Msg7.m_pDescription = "";
		Msg6.m_pReason = Msg7.m_pReason = "";

		int &Type = (Msg7.m_Type = protocol7::VOTE_UNKNOWN);
		if(m_VoteEnforce == VOTE_ENFORCE_NO || m_VoteEnforce == VOTE_ENFORCE_NO_ADMIN)
			Type = protocol7::VOTE_END_FAIL;
		else if(m_VoteEnforce == VOTE_ENFORCE_YES || m_VoteEnforce == VOTE_ENFORCE_YES_ADMIN)
			Type = protocol7::VOTE_END_PASS;
		else if(m_VoteEnforce == VOTE_ENFORCE_ABORT)
			Type = protocol7::VOTE_END_ABORT;

		if(m_VoteEnforce == VOTE_ENFORCE_NO_ADMIN || m_VoteEnforce == VOTE_ENFORCE_YES_ADMIN)
			Msg7.m_ClientID = -1;
	}

	if(ClientID == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!m_apPlayers[i])
				continue;
			if(!Server()->IsSixup(i))
				Server()->SendPackMsg(&Msg6, MSGFLAG_VITAL, i);
			else
				Server()->SendPackMsg(&Msg7, MSGFLAG_VITAL, i);
		}
	}
	else
	{
		if(!Server()->IsSixup(ClientID))
			Server()->SendPackMsg(&Msg6, MSGFLAG_VITAL, ClientID);
		else
			Server()->SendPackMsg(&Msg7, MSGFLAG_VITAL, ClientID);
	}
}

void CGameContext::SendVoteStatus(int ClientID, int Total, int Yes, int No)
{
	if(ClientID == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
			if(Server()->ClientIngame(i))
				SendVoteStatus(i, Total, Yes, No);
		return;
	}

	if(Total > VANILLA_MAX_CLIENTS && m_apPlayers[ClientID] && m_apPlayers[ClientID]->GetClientVersion() <= VERSION_DDRACE)
	{
		Yes = float(Yes) * VANILLA_MAX_CLIENTS / float(Total);
		No = float(No) * VANILLA_MAX_CLIENTS / float(Total);
		Total = VANILLA_MAX_CLIENTS;
	}

	CNetMsg_Sv_VoteStatus Msg = {0};
	Msg.m_Total = Total;
	Msg.m_Yes = Yes;
	Msg.m_No = No;
	Msg.m_Pass = Total - (Yes + No);

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendTuningParams(int ClientID, int Zone)
{
	if(ClientID == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(m_apPlayers[i])
			{
				if(m_apPlayers[i]->GetCharacter())
				{
					if(m_apPlayers[i]->GetCharacter()->m_TuneZone == Zone)
						SendTuningParams(i, Zone);
				}
				else if(m_apPlayers[i]->m_TuneZone == Zone)
				{
					SendTuningParams(i, Zone);
				}
			}
		}
		return;
	}

	CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
	int *pParams = 0;
	if(Zone == 0)
		pParams = (int *)&m_Tuning;
	else
		pParams = (int *)&(m_aTuningList[Zone]);

	unsigned int Last = sizeof(m_Tuning) / sizeof(int);
	if(m_apPlayers[ClientID])
	{
		int ClientVersion = m_apPlayers[ClientID]->GetClientVersion();
		if(ClientVersion < VERSION_DDNET_EXTRATUNES)
			Last = 33;
		else if(ClientVersion < VERSION_DDNET_HOOKDURATION_TUNE)
			Last = 37;
		else if(ClientVersion < VERSION_DDNET_FIREDELAY_TUNE)
			Last = 38;
	}

	for(unsigned i = 0; i < Last; i++)
	{
		if(m_apPlayers[ClientID] && m_apPlayers[ClientID]->GetCharacter())
		{
			if((i == 30) // laser_damage is removed from 0.7
				&& (Server()->IsSixup(ClientID)))
			{
				continue;
			}
			else if((i == 31) // collision
				&& (m_apPlayers[ClientID]->GetCharacter()->NeededFaketuning() & FAKETUNE_SOLO || m_apPlayers[ClientID]->GetCharacter()->NeededFaketuning() & FAKETUNE_NOCOLL))
			{
				Msg.AddInt(0);
			}
			else if((i == 32) // hooking
				&& (m_apPlayers[ClientID]->GetCharacter()->NeededFaketuning() & FAKETUNE_SOLO || m_apPlayers[ClientID]->GetCharacter()->NeededFaketuning() & FAKETUNE_NOHOOK))
			{
				Msg.AddInt(0);
			}
			else if((i == 3) // ground jump impulse
				&& m_apPlayers[ClientID]->GetCharacter()->NeededFaketuning() & FAKETUNE_NOJUMP)
			{
				Msg.AddInt(0);
			}
			else if((i == 33) // jetpack
				&& !(m_apPlayers[ClientID]->GetCharacter()->NeededFaketuning() & FAKETUNE_JETPACK))
			{
				Msg.AddInt(0);
			}
			else if((i == 36) // hammer hit
				&& m_apPlayers[ClientID]->GetCharacter()->NeededFaketuning() & FAKETUNE_NOHAMMER)
			{
				Msg.AddInt(0);
			}
			else
			{
				Msg.AddInt(pParams[i]);
			}
		}
		else
			Msg.AddInt(pParams[i]); // if everything is normal just send true tunings
	}
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::OnTick()
{
	Teams()->OnTick();
	UpdatePlayerMaps(); // MYTODO: check if this need to be ticked before controller
	DoActivityCheck();

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
		{
			// send vote options
			ProgressVoteOptions(i);

			m_apPlayers[i]->Tick();
			m_apPlayers[i]->PostTick();
		}
	}

	// update voting
	if(m_VoteCloseTime)
	{
		// abort the kick-vote on player-leave
		if(m_VoteEnforce == VOTE_ENFORCE_ABORT)
		{
			SendChat(-1, CGameContext::CHAT_ALL, "Vote aborted");
			EndVote();
		}
		else
		{
			int Total = 0, Yes = 0, No = 0;
			if(m_VoteUpdate)
			{
				// count votes
				char aaBuf[MAX_CLIENTS][NETADDR_MAXSTRSIZE] = {{0}}, *pIP = NULL;
				bool SinglePlayer = true;
				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(m_apPlayers[i])
					{
						Server()->GetClientAddr(i, aaBuf[i], NETADDR_MAXSTRSIZE);
						if(!pIP)
							pIP = aaBuf[i];
						else if(SinglePlayer && str_comp(pIP, aaBuf[i]))
							SinglePlayer = false;
					}
				}

				// remember checked players, only the first player with a specific ip will be handled
				bool aVoteChecked[MAX_CLIENTS] = {false};
				int64 Now = Server()->Tick();
				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(!m_apPlayers[i] || aVoteChecked[i])
						continue;

					// MYTODO: get rid of kick vote checks, no gonna happen here

					if((IsKickVote() || IsSpecVote()) && (m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS ||
										     (GetPlayerChar(m_VoteCreator) && GetPlayerChar(i) &&
											     GetPlayerChar(m_VoteCreator)->Team() != GetPlayerChar(i)->Team())))
						continue;

					if(m_apPlayers[i]->m_Afk && i != m_VoteCreator)
						continue;

					// can't vote in kick and spec votes in the beginning after joining
					if((IsKickVote() || IsSpecVote()) && Now < m_apPlayers[i]->m_FirstVoteTick)
						continue;

					// connecting clients with spoofed ips can clog slots without being ingame
					if(((CServer *)Server())->m_aClients[i].m_State != CServer::CClient::STATE_INGAME)
						continue;

					// don't count votes by blacklisted clients
					if(g_Config.m_SvDnsblVote && !m_pServer->DnsblWhite(i) && !SinglePlayer)
						continue;

					int ActVote = m_apPlayers[i]->m_Vote;
					int ActVotePos = m_apPlayers[i]->m_VotePos;

					// only allow IPs to vote once
					// check for more players with the same ip (only use the vote of the one who voted first)
					for(int j = i + 1; j < MAX_CLIENTS; j++)
					{
						if(!m_apPlayers[j] || aVoteChecked[j] || str_comp(aaBuf[j], aaBuf[i]) != 0)
							continue;

						// count the latest vote by this ip
						if(ActVotePos < m_apPlayers[j]->m_VotePos)
						{
							ActVote = m_apPlayers[j]->m_Vote;
							ActVotePos = m_apPlayers[j]->m_VotePos;
						}

						aVoteChecked[j] = true;
					}

					Total++;
					if(ActVote > 0)
						Yes++;
					else if(ActVote < 0)
						No++;
				}

				if(g_Config.m_SvVoteMaxTotal && Total > g_Config.m_SvVoteMaxTotal &&
					(IsKickVote() || IsSpecVote()))
					Total = g_Config.m_SvVoteMaxTotal;

				if(Yes > Total / (100.0f / g_Config.m_SvVoteYesPercentage))
					m_VoteEnforce = VOTE_ENFORCE_YES;
				else if(No >= Total - Total / (100.0f / g_Config.m_SvVoteYesPercentage))
					m_VoteEnforce = VOTE_ENFORCE_NO;

				m_VoteWillPass = Yes > (Yes + No) / (100.0f / g_Config.m_SvVoteYesPercentage);
			}

			if(time_get() > m_VoteCloseTime && !g_Config.m_SvVoteMajority)
				m_VoteEnforce = (m_VoteWillPass) ? VOTE_ENFORCE_YES : VOTE_ENFORCE_NO;

			// / Ensure minimum time for vote to end when moderating.
			if(m_VoteEnforce == VOTE_ENFORCE_YES && !(PlayerModerating() &&
									(IsKickVote() || IsSpecVote()) && time_get() < m_VoteCloseTime))
			{
				// silence voted command response
				m_ChatResponseTargetID = -1;
				Server()->SetRconCID(IServer::RCON_CID_VOTE);
				Console()->ExecuteLine(m_aVoteCommand, m_VoteCreator);
				Server()->SetRconCID(IServer::RCON_CID_SERV);
				EndVote();
				SendChatLocalized(-1, CHAT_SIX, "Vote passed");

				if(m_apPlayers[m_VoteCreator] && !IsKickVote() && !IsSpecVote())
					m_apPlayers[m_VoteCreator]->m_LastVoteCall = 0;
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_YES_ADMIN)
			{
				// silence voted command response
				m_ChatResponseTargetID = -1;
				Console()->ExecuteLine(m_aVoteCommand, m_VoteCreator);
				SendChatLocalized(-1, CHAT_SIX, "Vote passed enforced by authorized player");
				EndVote();
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_NO_ADMIN)
			{
				EndVote();
				SendChatLocalized(-1, CHAT_SIX, "Vote failed enforced by authorized player");
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_NO || (time_get() > m_VoteCloseTime && g_Config.m_SvVoteMajority))
			{
				EndVote();
				SendChatLocalized(-1, CHAT_SIX, "Vote failed");
			}
			else if(m_VoteUpdate)
			{
				m_VoteUpdate = false;
				SendVoteStatus(-1, Total, Yes, No);
			}
		}
	}
	for(int i = 0; i < m_NumMutes; i++)
	{
		if(m_aMutes[i].m_Expire <= Server()->Tick())
		{
			m_NumMutes--;
			m_aMutes[i] = m_aMutes[m_NumMutes];
		}
	}
	for(int i = 0; i < m_NumVoteMutes; i++)
	{
		if(m_aVoteMutes[i].m_Expire <= Server()->Tick())
		{
			m_NumVoteMutes--;
			m_aVoteMutes[i] = m_aVoteMutes[m_NumVoteMutes];
		}
	}

	if(Server()->Tick() % (g_Config.m_SvAnnouncementInterval * Server()->TickSpeed() * 60) == 0)
	{
		const char *Line = Server()->GetAnnouncementLine(g_Config.m_SvAnnouncementFileName);
		if(Line)
			SendChat(-1, CGameContext::CHAT_ALL, Line);
	}

	if(Collision()->m_NumSwitchers > 0)
	{
		for(int i = 0; i < Collision()->m_NumSwitchers + 1; ++i)
		{
			for(int j = 0; j < MAX_CLIENTS; ++j)
			{
				if(Collision()->m_pSwitchers[i].m_EndTick[j] <= Server()->Tick() && Collision()->m_pSwitchers[i].m_Type[j] == TILE_SWITCHTIMEDOPEN)
				{
					Collision()->m_pSwitchers[i].m_Status[j] = false;
					Collision()->m_pSwitchers[i].m_EndTick[j] = 0;
					Collision()->m_pSwitchers[i].m_Type[j] = TILE_SWITCHCLOSE;
				}
				else if(Collision()->m_pSwitchers[i].m_EndTick[j] <= Server()->Tick() && Collision()->m_pSwitchers[i].m_Type[j] == TILE_SWITCHTIMEDCLOSE)
				{
					Collision()->m_pSwitchers[i].m_Status[j] = true;
					Collision()->m_pSwitchers[i].m_EndTick[j] = 0;
					Collision()->m_pSwitchers[i].m_Type[j] = TILE_SWITCHOPEN;
				}
			}
		}
	}

#ifdef CONF_DEBUG
	if(g_Config.m_DbgDummies)
	{
		for(int i = 0; i < g_Config.m_DbgDummies; i++)
		{
			CNetObj_PlayerInput Input = {0};
			Input.m_Direction = (i & 1) ? -1 : 1;
			m_apPlayers[MAX_CLIENTS - i - 1]->OnPredictedInput(&Input);
		}
	}
#endif
}

// Server hooks
void CGameContext::OnClientDirectInput(int ClientID, void *pInput)
{
	CGameWorld *pPlayerWorld = PlayerGameInstance(ClientID).m_pWorld;
	if(pPlayerWorld && !pPlayerWorld->m_Paused)
		m_apPlayers[ClientID]->OnDirectInput((CNetObj_PlayerInput *)pInput);

	int Flags = ((CNetObj_PlayerInput *)pInput)->m_PlayerFlags;
	if((Flags & 256) || (Flags & 512))
	{
		Server()->Kick(ClientID, "please update your client or use DDNet client");
	}
}

void CGameContext::OnClientPredictedInput(int ClientID, void *pInput)
{
	CGameWorld *pPlayerWorld = PlayerGameInstance(ClientID).m_pWorld;
	if(pPlayerWorld && !pPlayerWorld->m_Paused)
		m_apPlayers[ClientID]->OnPredictedInput((CNetObj_PlayerInput *)pInput);
}

void CGameContext::OnClientPredictedEarlyInput(int ClientID, void *pInput)
{
	CGameWorld *pPlayerWorld = PlayerGameInstance(ClientID).m_pWorld;
	if(pPlayerWorld && !pPlayerWorld->m_Paused)
		m_apPlayers[ClientID]->OnPredictedEarlyInput((CNetObj_PlayerInput *)pInput);
}

struct CVoteOptionServer *CGameContext::GetVoteOption(int Index)
{
	CVoteOptionServer *pCurrent;
	for(pCurrent = m_pVoteOptionFirst;
		Index > 0 && pCurrent;
		Index--, pCurrent = pCurrent->m_pNext)
		;

	if(Index > 0)
		return 0;
	return pCurrent;
}

void CGameContext::ProgressVoteOptions(int ClientID)
{
	CPlayer *pPl = m_apPlayers[ClientID];

	SGameInstance Instance = PlayerGameInstance(ClientID);

	int NumRoomTitleVote = g_Config.m_SvRoomVoteTitle[0] ? 1 : 0;
	int NumRoomVotes = Teams()->m_NumRooms;
	int NumInstanceVotes = Instance.m_IsCreated ? Instance.m_pController->m_NumVoteOptions : 0;
	int GlobalVotes = m_NumVoteOptions;
	int TotalVotes = NumRoomTitleVote + NumRoomVotes + NumInstanceVotes + GlobalVotes;

	if(pPl->m_SendVoteIndex == -1)
		return; // we didn't start sending options yet

	if(pPl->m_SendVoteIndex > TotalVotes)
		return; // shouldn't happen / fail silently

	int VotesLeft = TotalVotes - pPl->m_SendVoteIndex;
	int NumRoomTitleToSend = clamp(NumRoomTitleVote - pPl->m_SendVoteIndex, 0, NumRoomTitleVote);
	int NumRoomVotesToSend = clamp(NumRoomVotes - (pPl->m_SendVoteIndex - NumRoomTitleVote), 0, NumRoomVotes);
	int NumInstanceVotesToSend = clamp(NumInstanceVotes - (pPl->m_SendVoteIndex - NumRoomTitleVote - NumRoomVotes), 0, NumInstanceVotes);
	int NumGlobalVotesToSend = clamp(GlobalVotes - (pPl->m_SendVoteIndex - NumRoomTitleVote - NumRoomVotes - NumInstanceVotes), 0, GlobalVotes);

	if(!VotesLeft)
	{
		// player has up to date vote option list
		return;
	}

	// build vote option list msg
	int CurIndex = 0;

	CNetMsg_Sv_VoteOptionListAdd OptionMsg;
	OptionMsg.m_pDescription0 = "";
	OptionMsg.m_pDescription1 = "";
	OptionMsg.m_pDescription2 = "";
	OptionMsg.m_pDescription3 = "";
	OptionMsg.m_pDescription4 = "";
	OptionMsg.m_pDescription5 = "";
	OptionMsg.m_pDescription6 = "";
	OptionMsg.m_pDescription7 = "";
	OptionMsg.m_pDescription8 = "";
	OptionMsg.m_pDescription9 = "";
	OptionMsg.m_pDescription10 = "";
	OptionMsg.m_pDescription11 = "";
	OptionMsg.m_pDescription12 = "";
	OptionMsg.m_pDescription13 = "";
	OptionMsg.m_pDescription14 = "";

	// room list title
	if(NumRoomTitleToSend)
	{
		OptionMsg.m_pDescription0 = g_Config.m_SvRoomVoteTitle;
		CurIndex++;
	}

	// room list
	int RoomIndex = NumRoomVotes - NumRoomVotesToSend;

	while(RoomIndex < NumRoomVotes && CurIndex < g_Config.m_SvSendVotesPerTick)
	{
		const char *pRoomVoteDesc = Teams()->m_aRoomVotes[RoomIndex];
		if(Teams()->m_Core.Team(ClientID) == Teams()->m_RoomNumbers[RoomIndex])
			pRoomVoteDesc = Teams()->m_aRoomVotesJoined[RoomIndex];
		switch(CurIndex)
		{
		case 0: OptionMsg.m_pDescription0 = pRoomVoteDesc; break;
		case 1: OptionMsg.m_pDescription1 = pRoomVoteDesc; break;
		case 2: OptionMsg.m_pDescription2 = pRoomVoteDesc; break;
		case 3: OptionMsg.m_pDescription3 = pRoomVoteDesc; break;
		case 4: OptionMsg.m_pDescription4 = pRoomVoteDesc; break;
		case 5: OptionMsg.m_pDescription5 = pRoomVoteDesc; break;
		case 6: OptionMsg.m_pDescription6 = pRoomVoteDesc; break;
		case 7: OptionMsg.m_pDescription7 = pRoomVoteDesc; break;
		case 8: OptionMsg.m_pDescription8 = pRoomVoteDesc; break;
		case 9: OptionMsg.m_pDescription9 = pRoomVoteDesc; break;
		case 10: OptionMsg.m_pDescription10 = pRoomVoteDesc; break;
		case 11: OptionMsg.m_pDescription11 = pRoomVoteDesc; break;
		case 12: OptionMsg.m_pDescription12 = pRoomVoteDesc; break;
		case 13: OptionMsg.m_pDescription13 = pRoomVoteDesc; break;
		case 14: OptionMsg.m_pDescription14 = pRoomVoteDesc; break;
		}

		CurIndex++;
		RoomIndex++;
	}

	// get current instance vote option by index
	CVoteOptionServer *pCurrent = NULL;
	int InstanceVoteIndex = NumInstanceVotes - NumInstanceVotesToSend;
	if(NumInstanceVotesToSend)
		pCurrent = Instance.m_pController->GetVoteOption(InstanceVoteIndex);

	while(InstanceVoteIndex < NumInstanceVotes && CurIndex < g_Config.m_SvSendVotesPerTick && pCurrent != NULL)
	{
		switch(CurIndex)
		{
		case 0: OptionMsg.m_pDescription0 = pCurrent->m_aDescription; break;
		case 1: OptionMsg.m_pDescription1 = pCurrent->m_aDescription; break;
		case 2: OptionMsg.m_pDescription2 = pCurrent->m_aDescription; break;
		case 3: OptionMsg.m_pDescription3 = pCurrent->m_aDescription; break;
		case 4: OptionMsg.m_pDescription4 = pCurrent->m_aDescription; break;
		case 5: OptionMsg.m_pDescription5 = pCurrent->m_aDescription; break;
		case 6: OptionMsg.m_pDescription6 = pCurrent->m_aDescription; break;
		case 7: OptionMsg.m_pDescription7 = pCurrent->m_aDescription; break;
		case 8: OptionMsg.m_pDescription8 = pCurrent->m_aDescription; break;
		case 9: OptionMsg.m_pDescription9 = pCurrent->m_aDescription; break;
		case 10: OptionMsg.m_pDescription10 = pCurrent->m_aDescription; break;
		case 11: OptionMsg.m_pDescription11 = pCurrent->m_aDescription; break;
		case 12: OptionMsg.m_pDescription12 = pCurrent->m_aDescription; break;
		case 13: OptionMsg.m_pDescription13 = pCurrent->m_aDescription; break;
		case 14: OptionMsg.m_pDescription14 = pCurrent->m_aDescription; break;
		}

		CurIndex++;
		pCurrent = pCurrent->m_pNext;
		InstanceVoteIndex++;
	}

	// get current global vote option by index
	pCurrent = NULL;
	if(NumGlobalVotesToSend)
		pCurrent = GetVoteOption(GlobalVotes - NumGlobalVotesToSend);

	while(CurIndex < GlobalVotes && CurIndex < g_Config.m_SvSendVotesPerTick && pCurrent != NULL)
	{
		switch(CurIndex)
		{
		case 0: OptionMsg.m_pDescription0 = pCurrent->m_aDescription; break;
		case 1: OptionMsg.m_pDescription1 = pCurrent->m_aDescription; break;
		case 2: OptionMsg.m_pDescription2 = pCurrent->m_aDescription; break;
		case 3: OptionMsg.m_pDescription3 = pCurrent->m_aDescription; break;
		case 4: OptionMsg.m_pDescription4 = pCurrent->m_aDescription; break;
		case 5: OptionMsg.m_pDescription5 = pCurrent->m_aDescription; break;
		case 6: OptionMsg.m_pDescription6 = pCurrent->m_aDescription; break;
		case 7: OptionMsg.m_pDescription7 = pCurrent->m_aDescription; break;
		case 8: OptionMsg.m_pDescription8 = pCurrent->m_aDescription; break;
		case 9: OptionMsg.m_pDescription9 = pCurrent->m_aDescription; break;
		case 10: OptionMsg.m_pDescription10 = pCurrent->m_aDescription; break;
		case 11: OptionMsg.m_pDescription11 = pCurrent->m_aDescription; break;
		case 12: OptionMsg.m_pDescription12 = pCurrent->m_aDescription; break;
		case 13: OptionMsg.m_pDescription13 = pCurrent->m_aDescription; break;
		case 14: OptionMsg.m_pDescription14 = pCurrent->m_aDescription; break;
		}

		CurIndex++;
		pCurrent = pCurrent->m_pNext;
	}

	// send msg
	OptionMsg.m_NumOptions = CurIndex;
	Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);

	pPl->m_SendVoteIndex += CurIndex;
}

void CGameContext::OnClientEnter(int ClientID)
{
	if(Server()->IsSixup(ClientID))
	{
		// /team is essential
		{
			protocol7::CNetMsg_Sv_CommandInfoRemove Msg;
			Msg.m_Name = "team";
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
		}

		for(const IConsole::CCommandInfo *pCmd = Console()->FirstCommandInfo(IConsole::ACCESS_LEVEL_USER, CFGFLAG_CHAT);
			pCmd; pCmd = pCmd->NextCommandInfo(IConsole::ACCESS_LEVEL_USER, CFGFLAG_CHAT))
		{
			if(!str_comp(pCmd->m_pName, "w") || !str_comp(pCmd->m_pName, "whisper"))
				continue;

			const char *pName = pCmd->m_pName;
			if(!str_comp(pCmd->m_pName, "r"))
				pName = "ready";

			protocol7::CNetMsg_Sv_CommandInfo Msg;
			Msg.m_Name = pName;
			Msg.m_ArgsFormat = pCmd->m_pParams;
			Msg.m_HelpText = pCmd->m_pHelp;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
		}
	}

	if(!Server()->ClientPrevIngame(ClientID))
	{
		if(g_Config.m_SvWelcome[0] != 0)
			SendChatLocalized(ClientID, g_Config.m_SvWelcome);

		IServer::CClientInfo Info;
		Server()->GetClientInfo(ClientID, &Info);
		if(Info.m_GotDDNetVersion)
		{
			if(OnClientDDNetVersionKnown(ClientID))
				return; // kicked
		}

		if(g_Config.m_SvShowOthersDefault > 0)
		{
			if(g_Config.m_SvShowOthers)
				SendChatLocalized(ClientID, "You can see other players. To disable this, type /showothers .");

			m_apPlayers[ClientID]->m_ShowOthers = g_Config.m_SvShowOthersDefault;
		}
	}
	m_VoteUpdate = true;

	// send active vote
	if(m_VoteCloseTime)
		SendVoteSet(ClientID);

	Server()->ExpireServerInfo();
	SendCurrentGameInfo(ClientID, true);
	Teams()->OnPlayerConnect(m_apPlayers[ClientID]);
}

bool CGameContext::OnClientDataPersist(int ClientID, void *pData)
{
	CPersistentClientData *pPersistent = (CPersistentClientData *)pData;
	if(!m_apPlayers[ClientID])
	{
		return false;
	}
	pPersistent->m_IsSpectator = m_apPlayers[ClientID]->GetTeam() == TEAM_SPECTATORS;
	return true;
}

void CGameContext::OnClientConnected(int ClientID, void *pData)
{
	CPersistentClientData *pPersistentData = (CPersistentClientData *)pData;
	bool Spec = false;
	if(pPersistentData)
	{
		Spec = pPersistentData->m_IsSpectator;
	}

	{
		bool Empty = true;
		for(auto &pPlayer : m_apPlayers)
		{
			if(pPlayer)
			{
				Empty = false;
				break;
			}
		}
		if(Empty)
		{
			m_NonEmptySince = Server()->Tick();
		}
	}

	// Check whether to join as spectator
	const bool AsSpec = (Spec || g_Config.m_SvTournamentMode || g_Config.m_SvRoom == 2) ? true : false;

	if(!m_apPlayers[ClientID])
		m_apPlayers[ClientID] = new(ClientID) CPlayer(this, ClientID, AsSpec);
	else
	{
		delete m_apPlayers[ClientID];
		m_apPlayers[ClientID] = new(ClientID) CPlayer(this, ClientID, AsSpec);
		//	//m_apPlayers[ClientID]->Reset();
		//	//((CServer*)Server())->m_aClients[ClientID].Reset();
		//	((CServer*)Server())->m_aClients[ClientID].m_State = 4;
	}
	//players[client_id].init(client_id);
	//players[client_id].client_id = client_id;

#ifdef CONF_DEBUG
	if(g_Config.m_DbgDummies)
	{
		if(ClientID >= MAX_CLIENTS - g_Config.m_DbgDummies)
			return;
	}
#endif

	SendMotd(ClientID);
	SendSettings(ClientID);

	Server()->ExpireServerInfo();
}

void CGameContext::OnClientDrop(int ClientID, const char *pReason)
{
	if(((CServer *)Server())->m_aClients[ClientID].m_State == CServer::CClient::STATE_INGAME)
		Teams()->OnPlayerDisconnect(m_apPlayers[ClientID], pReason);

	//(void)m_pController->CheckTeamBalance();
	m_VoteUpdate = true;

	// update spectator modes
	for(auto &pPlayer : m_apPlayers)
	{
		if(pPlayer && pPlayer->GetSpectatorID() == ClientID)
		{
			pPlayer->SetSpecMode(CPlayer::ESpecMode::FREEVIEW);
		}
	}

	delete m_apPlayers[ClientID];
	m_apPlayers[ClientID] = 0;

	// update conversation targets
	for(auto &pPlayer : m_apPlayers)
	{
		if(pPlayer && pPlayer->m_LastWhisperTo == ClientID)
			pPlayer->m_LastWhisperTo = -1;
	}

	protocol7::CNetMsg_Sv_ClientDrop Msg;
	Msg.m_ClientID = ClientID;
	Msg.m_pReason = pReason;
	Msg.m_Silent = false;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, -1);

	Server()->ExpireServerInfo();
}

void CGameContext::OnClientEngineJoin(int ClientID, bool Sixup)
{
}

void CGameContext::OnClientEngineDrop(int ClientID, const char *pReason)
{
}

bool CGameContext::OnClientDDNetVersionKnown(int ClientID)
{
	IServer::CClientInfo Info;
	Server()->GetClientInfo(ClientID, &Info);
	int ClientVersion = Info.m_DDNetVersion;
	dbg_msg("ddnet", "cid=%d version=%d", ClientID, ClientVersion);

	// Autoban unwanted clients
	if(g_Config.m_SvBannedVersions[0] != '\0' && IsVersionBanned(ClientVersion))
	{
		Server()->Kick(ClientID, "unsupported client");
		return true;
	}

	// Autoban known bot versions.
	if((ClientVersion >= 15 && ClientVersion < 100) || ClientVersion == 502)
	{
		Server()->Kick(ClientID, "unsupported client");
		return true;
	}

	CPlayer *pPlayer = m_apPlayers[ClientID];
	if(ClientVersion >= VERSION_DDNET_GAMETICK)
		pPlayer->m_TimerType = g_Config.m_SvDefaultTimerType;

	// First update the teams state.
	Teams()->SendTeamsState(ClientID);

	// And report correct tunings.
	if(ClientVersion >= VERSION_DDNET_EXTRATUNES)
		SendTuningParams(ClientID, pPlayer->m_TuneZone);

	// Tell old clients to update.
	if(ClientVersion < VERSION_DDNET_UPDATER_FIXED && g_Config.m_SvClientSuggestionOld[0] != '\0')
		SendBroadcast(g_Config.m_SvClientSuggestionOld, ClientID);

	return false;
}

void *CGameContext::PreProcessMsg(int *MsgID, CUnpacker *pUnpacker, int ClientID)
{
	if(Server()->IsSixup(ClientID) && *MsgID < OFFSET_UUID)
	{
		void *pRawMsg = m_NetObjHandler7.SecureUnpackMsg(*MsgID, pUnpacker);
		if(!pRawMsg)
			return 0;

		CPlayer *pPlayer = m_apPlayers[ClientID];
		static char s_aRawMsg[1024];

		if(*MsgID == protocol7::NETMSGTYPE_CL_SAY)
		{
			protocol7::CNetMsg_Cl_Say *pMsg7 = (protocol7::CNetMsg_Cl_Say *)pRawMsg;
			// Should probably use a placement new to start the lifetime of the object to avoid future weirdness
			::CNetMsg_Cl_Say *pMsg = (::CNetMsg_Cl_Say *)s_aRawMsg;

			if(pMsg7->m_Target >= 0)
			{
				if(ProcessSpamProtection(ClientID))
					return 0;

				// Should we maybe recraft the message so that it can go through the usual path?
				WhisperID(ClientID, pMsg7->m_Target, pMsg7->m_pMessage);
				return 0;
			}

			pMsg->m_Team = pMsg7->m_Mode == protocol7::CHAT_TEAM;
			pMsg->m_pMessage = pMsg7->m_pMessage;
		}
		else if(*MsgID == protocol7::NETMSGTYPE_CL_STARTINFO)
		{
			protocol7::CNetMsg_Cl_StartInfo *pMsg7 = (protocol7::CNetMsg_Cl_StartInfo *)pRawMsg;
			::CNetMsg_Cl_StartInfo *pMsg = (::CNetMsg_Cl_StartInfo *)s_aRawMsg;

			pMsg->m_pName = pMsg7->m_pName;
			pMsg->m_pClan = pMsg7->m_pClan;
			pMsg->m_Flag = pMsg7->m_Flag;

			CTeeInfo Info(pMsg7->m_apSkinPartNames, pMsg7->m_aUseCustomColors, pMsg7->m_aSkinPartColors);
			Info.FromSixup();
			pPlayer->m_TeeInfos = Info;

			str_copy(s_aRawMsg + sizeof(*pMsg), Info.m_SkinName, sizeof(s_aRawMsg) - sizeof(*pMsg));

			pMsg->m_pSkin = s_aRawMsg + sizeof(*pMsg);
			pMsg->m_UseCustomColor = pPlayer->m_TeeInfos.m_UseCustomColor;
			pMsg->m_ColorBody = pPlayer->m_TeeInfos.m_ColorBody;
			pMsg->m_ColorFeet = pPlayer->m_TeeInfos.m_ColorFeet;
		}
		else if(*MsgID == protocol7::NETMSGTYPE_CL_SKINCHANGE)
		{
			protocol7::CNetMsg_Cl_SkinChange *pMsg = (protocol7::CNetMsg_Cl_SkinChange *)pRawMsg;
			if(g_Config.m_SvSpamprotection && pPlayer->m_LastChangeInfo &&
				pPlayer->m_LastChangeInfo + Server()->TickSpeed() * g_Config.m_SvInfoChangeDelay > Server()->Tick())
				return 0;

			pPlayer->m_LastChangeInfo = Server()->Tick();

			CTeeInfo Info(pMsg->m_apSkinPartNames, pMsg->m_aUseCustomColors, pMsg->m_aSkinPartColors);
			Info.FromSixup();
			pPlayer->m_TeeInfos = Info;

			SendSkinInfo(ClientID);

			return 0;
		}
		else if(*MsgID == protocol7::NETMSGTYPE_CL_SETSPECTATORMODE)
		{
			protocol7::CNetMsg_Cl_SetSpectatorMode *pMsg7 = (protocol7::CNetMsg_Cl_SetSpectatorMode *)pRawMsg;

			pMsg7->m_SpectatorID = clamp(pMsg7->m_SpectatorID, (int)SPEC_FREEVIEW, MAX_CLIENTS - 1);

			if((g_Config.m_SvSpamprotection && pPlayer->m_LastSetSpectatorMode && pPlayer->m_LastSetSpectatorMode + Server()->TickSpeed() / 4 > Server()->Tick()))
				return 0;

			pPlayer->m_LastSetSpectatorMode = Server()->Tick();
			pPlayer->UpdatePlaytime();
			SGameInstance Instance = PlayerGameInstance(ClientID);
			if(!pPlayer->SetSpecMode((CPlayer::ESpecMode)pMsg7->m_SpecMode, pMsg7->m_SpectatorID) && Instance.m_Init)
				Instance.m_pController->SendGameMsg(GAMEMSG_SPEC_INVALIDID, ClientID);
			return 0;
		}
		else if(*MsgID == protocol7::NETMSGTYPE_CL_READYCHANGE)
		{
			if(!ChangePlayerReadyState(pPlayer))
				return 0;
		}
		else if(*MsgID == protocol7::NETMSGTYPE_CL_SETTEAM)
		{
			protocol7::CNetMsg_Cl_SetTeam *pMsg7 = (protocol7::CNetMsg_Cl_SetTeam *)pRawMsg;
			::CNetMsg_Cl_SetTeam *pMsg = (::CNetMsg_Cl_SetTeam *)s_aRawMsg;

			pMsg->m_Team = pMsg7->m_Team;
		}
		else if(*MsgID == protocol7::NETMSGTYPE_CL_COMMAND)
		{
			protocol7::CNetMsg_Cl_Command *pMsg7 = (protocol7::CNetMsg_Cl_Command *)pRawMsg;
			::CNetMsg_Cl_Say *pMsg = (::CNetMsg_Cl_Say *)s_aRawMsg;

			str_format(s_aRawMsg + sizeof(*pMsg), sizeof(s_aRawMsg) - sizeof(*pMsg), "/%s %s", pMsg7->m_Name, pMsg7->m_Arguments);
			pMsg->m_pMessage = s_aRawMsg + sizeof(*pMsg);
			dbg_msg("debug", "line='%s'", s_aRawMsg + sizeof(*pMsg));
			pMsg->m_Team = 0;

			*MsgID = NETMSGTYPE_CL_SAY;
			return s_aRawMsg;
		}
		else if(*MsgID == protocol7::NETMSGTYPE_CL_CALLVOTE)
		{
			protocol7::CNetMsg_Cl_CallVote *pMsg7 = (protocol7::CNetMsg_Cl_CallVote *)pRawMsg;
			::CNetMsg_Cl_CallVote *pMsg = (::CNetMsg_Cl_CallVote *)s_aRawMsg;

			int Authed = Server()->GetAuthedState(ClientID);
			if(pMsg7->m_Force)
			{
				str_format(s_aRawMsg, sizeof(s_aRawMsg), "force_vote \"%s\" \"%s\" \"%s\"", pMsg7->m_Type, pMsg7->m_Value, pMsg7->m_Reason);
				Console()->SetAccessLevel(Authed == AUTHED_ADMIN ? IConsole::ACCESS_LEVEL_ADMIN : Authed == AUTHED_MOD ? IConsole::ACCESS_LEVEL_MOD :
                                                                                                                                         IConsole::ACCESS_LEVEL_HELPER);
				Console()->ExecuteLine(s_aRawMsg, ClientID, false);
				Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_ADMIN);
				return 0;
			}

			pMsg->m_Value = pMsg7->m_Value;
			pMsg->m_Reason = pMsg7->m_Reason;
			pMsg->m_Type = pMsg7->m_Type;
		}
		else if(*MsgID == protocol7::NETMSGTYPE_CL_EMOTICON)
		{
			protocol7::CNetMsg_Cl_Emoticon *pMsg7 = (protocol7::CNetMsg_Cl_Emoticon *)pRawMsg;
			::CNetMsg_Cl_Emoticon *pMsg = (::CNetMsg_Cl_Emoticon *)s_aRawMsg;

			pMsg->m_Emoticon = pMsg7->m_Emoticon;
		}
		else if(*MsgID == protocol7::NETMSGTYPE_CL_VOTE)
		{
			protocol7::CNetMsg_Cl_Vote *pMsg7 = (protocol7::CNetMsg_Cl_Vote *)pRawMsg;
			::CNetMsg_Cl_Vote *pMsg = (::CNetMsg_Cl_Vote *)s_aRawMsg;

			pMsg->m_Vote = pMsg7->m_Vote;
		}

		*MsgID = Msg_SevenToSix(*MsgID);

		return s_aRawMsg;
	}
	else
		return m_NetObjHandler.SecureUnpackMsg(*MsgID, pUnpacker);
}

void CGameContext::CensorMessage(char *pCensoredMessage, const char *pMessage, int Size)
{
	str_copy(pCensoredMessage, pMessage, Size);

	for(int i = 0; i < m_aCensorlist.size(); i++)
	{
		char *pCurLoc = pCensoredMessage;
		do
		{
			pCurLoc = (char *)str_find_nocase(pCurLoc, m_aCensorlist[i].cstr());
			if(pCurLoc)
			{
				memset(pCurLoc, '*', str_length(m_aCensorlist[i].cstr()));
				pCurLoc++;
			}
		} while(pCurLoc);
	}
}

void CGameContext::OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID)
{
	void *pRawMsg = PreProcessMsg(&MsgID, pUnpacker, ClientID);

	if(!pRawMsg)
		return;

	CPlayer *pPlayer = m_apPlayers[ClientID];

	if(Server()->ClientIngame(ClientID))
	{
		CGameWorld *pPlayerWorld = PlayerGameInstance(ClientID).m_pWorld;
		if(MsgID == NETMSGTYPE_CL_SAY)
		{
			CNetMsg_Cl_Say *pMsg = (CNetMsg_Cl_Say *)pRawMsg;
			if(!str_utf8_check(pMsg->m_pMessage))
			{
				return;
			}
			bool Check = !pPlayer->m_NotEligibleForFinish && pPlayer->m_EligibleForFinishCheck + 10 * time_freq() >= time_get();
			if(Check && str_comp(pMsg->m_pMessage, "xd sure chillerbot.png is lyfe") == 0 && pMsg->m_Team == 0)
			{
				pPlayer->m_NotEligibleForFinish = true;
				dbg_msg("hack", "bot detected, cid=%d", ClientID);
				return;
			}
			int IsTeam = pMsg->m_Team;

			// trim right and set maximum length to 256 utf8-characters
			int Length = 0;
			const char *p = pMsg->m_pMessage;
			const char *pEnd = 0;
			while(*p)
			{
				const char *pStrOld = p;
				int Code = str_utf8_decode(&p);

				// check if unicode is not empty
				if(!str_utf8_isspace(Code))
				{
					pEnd = 0;
				}
				else if(pEnd == 0)
					pEnd = pStrOld;

				if(++Length >= 256)
				{
					*(const_cast<char *>(p)) = 0;
					break;
				}
			}
			if(pEnd != 0)
				*(const_cast<char *>(pEnd)) = 0;

			// drop empty and autocreated spam messages (more than 32 characters per second)
			if(Length == 0 || (pMsg->m_pMessage[0] != '/' && (g_Config.m_SvSpamprotection && pPlayer->m_LastChat && pPlayer->m_LastChat + Server()->TickSpeed() * ((31 + Length) / 32) > Server()->Tick())))
				return;

			pPlayer->UpdatePlaytime();

			if(pMsg->m_pMessage[0] == '/')
			{
				if(str_comp_nocase_num(pMsg->m_pMessage + 1, "w ", 2) == 0)
				{
					char aWhisperMsg[256];
					str_copy(aWhisperMsg, pMsg->m_pMessage + 3, 256);
					Whisper(pPlayer->GetCID(), aWhisperMsg);
				}
				else if(str_comp_nocase_num(pMsg->m_pMessage + 1, "whisper ", 8) == 0)
				{
					char aWhisperMsg[256];
					str_copy(aWhisperMsg, pMsg->m_pMessage + 9, 256);
					Whisper(pPlayer->GetCID(), aWhisperMsg);
				}
				else if(str_comp_nocase_num(pMsg->m_pMessage + 1, "c ", 2) == 0)
				{
					char aWhisperMsg[256];
					str_copy(aWhisperMsg, pMsg->m_pMessage + 3, 256);
					Converse(pPlayer->GetCID(), aWhisperMsg);
				}
				else if(str_comp_nocase_num(pMsg->m_pMessage + 1, "converse ", 9) == 0)
				{
					char aWhisperMsg[256];
					str_copy(aWhisperMsg, pMsg->m_pMessage + 10, 256);
					Converse(pPlayer->GetCID(), aWhisperMsg);
				}
				else
				{
					if(g_Config.m_SvSpamprotection && !str_startswith(pMsg->m_pMessage + 1, "timeout ") && pPlayer->m_LastCommands[0] && pPlayer->m_LastCommands[0] + Server()->TickSpeed() > Server()->Tick() && pPlayer->m_LastCommands[1] && pPlayer->m_LastCommands[1] + Server()->TickSpeed() > Server()->Tick() && pPlayer->m_LastCommands[2] && pPlayer->m_LastCommands[2] + Server()->TickSpeed() > Server()->Tick() && pPlayer->m_LastCommands[3] && pPlayer->m_LastCommands[3] + Server()->TickSpeed() > Server()->Tick())
						return;

					int64 Now = Server()->Tick();
					pPlayer->m_LastCommands[pPlayer->m_LastCommandPos] = Now;
					pPlayer->m_LastCommandPos = (pPlayer->m_LastCommandPos + 1) % 4;

					m_ChatResponseTargetID = ClientID;
					Server()->RestrictRconOutput(ClientID);
					Console()->SetFlagMask(CFGFLAG_CHAT);

					int Authed = Server()->GetAuthedState(ClientID);
					if(Authed)
						Console()->SetAccessLevel(Authed == AUTHED_ADMIN ? IConsole::ACCESS_LEVEL_ADMIN : Authed == AUTHED_MOD ? IConsole::ACCESS_LEVEL_MOD :
                                                                                                                                                         IConsole::ACCESS_LEVEL_HELPER);
					else
						Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_USER);
					Console()->SetPrintOutputLevel(m_ChatPrintCBIndex, 0);

					Console()->ExecuteLine(pMsg->m_pMessage + 1, ClientID, false);
					// m_apPlayers[ClientID] can be NULL, if the player used a
					// timeout code and replaced another client.
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "%d used %s", ClientID, pMsg->m_pMessage);
					Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "chat-command", aBuf);

					Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_ADMIN);
					Console()->SetFlagMask(CFGFLAG_SERVER);
					m_ChatResponseTargetID = -1;
					Server()->RestrictRconOutput(-1);
				}
			}
			else
			{
				SGameInstance Instance = PlayerGameInstance(pPlayer->GetCID());
				if(Instance.m_IsCreated && !Instance.m_pController->IsTeamplay() && g_Config.m_SvTournamentChat == 2)
					return;

				if(g_Config.m_SvTournamentChat == 2 || (g_Config.m_SvTournamentChat == 1 && pPlayer->GetTeam() == TEAM_SPECTATORS))
					IsTeam = true;

				int ChatTeam = IsTeam ? pPlayer->GetTeam() : CHAT_ALL;

				char aCensoredMessage[256];
				CensorMessage(aCensoredMessage, pMsg->m_pMessage, sizeof(aCensoredMessage));
				SendChat(ClientID, ChatTeam, aCensoredMessage, ClientID);
			}
		}
		else if(MsgID == NETMSGTYPE_CL_CALLVOTE)
		{
			if(m_VoteCloseTime)
				return;

			m_apPlayers[ClientID]->UpdatePlaytime();
			m_VoteType = VOTE_TYPE_UNKNOWN;
			char aChatmsg[512] = {0};
			char aDesc[VOTE_DESC_LENGTH] = {0};
			char aSixupDesc[VOTE_DESC_LENGTH] = {0};
			char aCmd[VOTE_CMD_LENGTH] = {0};
			char aReason[VOTE_REASON_LENGTH] = "No reason given";
			CNetMsg_Cl_CallVote *pMsg = (CNetMsg_Cl_CallVote *)pRawMsg;
			if(!str_utf8_check(pMsg->m_Type) || !str_utf8_check(pMsg->m_Reason) || !str_utf8_check(pMsg->m_Value))
			{
				return;
			}
			if(pMsg->m_Reason[0])
			{
				str_copy(aReason, pMsg->m_Reason, sizeof(aReason));
			}

			bool IsInstanceVote = false;
			bool IsNoConsent = false;

			if(str_comp_nocase(pMsg->m_Type, "option") == 0)
			{
				int Authed = Server()->GetAuthedState(ClientID);

				if(str_startswith(pMsg->m_Value, "⨀") || str_startswith(pMsg->m_Value, "⨂"))
				{
					// is room vote
					char aBuf[32];
					str_copy(aBuf, pMsg->m_Value, sizeof(aBuf));
					char *pColon = (char *)str_find(aBuf, ":");
					if(!pColon)
						return;

					*pColon = 0;
					int Room = str_toint(aBuf + 9); // 9 = len of "⨀ Room "
					str_format(aCmd, sizeof(aCmd), "join %d", Room);
					IsInstanceVote = false;
					IsNoConsent = true;
				}
				else
				{
					CVoteOptionServer *pOption;
					IConsole *pConsole;
					if(str_startswith(pMsg->m_Value, "☐"))
					{
						// is instance vote
						SGameInstance Instance = PlayerGameInstance(ClientID);
						if(!Instance.m_Init)
						{
							SendChatLocalized(ClientID, "Room is not ready");
							return;
						}

						IsInstanceVote = true;
						pOption = Instance.m_pController->m_pVoteOptionFirst;
						pConsole = Instance.m_pController->InstanceConsole();
					}
					else
					{
						pOption = m_pVoteOptionFirst;
						pConsole = Console();
					}

					while(pOption)
					{
						if(str_comp_nocase(pMsg->m_Value, pOption->m_aDescription) == 0)
						{
							if(!pConsole->LineIsValid(pOption->m_aCommand))
							{
								SendChatLocalized(ClientID, "Invalid option");
								return;
							}

							if(str_startswith(pOption->m_aCommand, "setting "))
							{
								IsInstanceVote = true;
								str_format(aCmd, sizeof(aCmd), "%s", pOption->m_aCommand + 8);
							}
							else
							{
								str_format(aCmd, sizeof(aCmd), "%s", pOption->m_aCommand);
							}
							str_format(aDesc, sizeof(aDesc), "%s", pOption->m_aDescription);

							if(IsInstanceVote)
								str_format(aChatmsg, sizeof(aChatmsg), "'%s' called vote to change room setting '%s' (%s)", Server()->ClientName(ClientID),
									pOption->m_aDescription, aReason);
							else
								str_format(aChatmsg, sizeof(aChatmsg), "'%s' called vote to change server option '%s' (%s)", Server()->ClientName(ClientID),
									pOption->m_aDescription, aReason);

							if(!str_find(aCmd, ";"))
							{
								const char *args = str_find(aCmd, " ");
								if(args)
									*(char *)args = '\0';

								const IConsole::CCommandInfo *pInfo = pConsole->GetCommandInfo(aCmd, CFGFLAG_NO_CONSENT, false);
								if(pInfo && pInfo->m_Flags & CFGFLAG_NO_CONSENT)
									IsNoConsent = true;

								if(args)
									*(char *)args = ' ';
							}
							break;
						}

						pOption = pOption->m_pNext;
					}

					if(!pOption)
					{
						if(Authed != AUTHED_ADMIN) // allow admins to call any vote they want
						{
							return;
						}
						else
						{
							str_format(aChatmsg, sizeof(aChatmsg), "'%s' called vote to change server option '%s'", Server()->ClientName(ClientID), pMsg->m_Value);
							str_format(aDesc, sizeof(aDesc), "%s", pMsg->m_Value);
							str_format(aCmd, sizeof(aCmd), "%s", pMsg->m_Value);
						}
					}

					m_VoteType = VOTE_TYPE_OPTION;
				}
			}
			else if(str_comp_nocase(pMsg->m_Type, "kick") == 0)
			{
				int Authed = Server()->GetAuthedState(ClientID);
				if(!Authed && time_get() < m_apPlayers[ClientID]->m_LastKickVote + (time_freq() * 5))
					return;
				else if(!Authed && time_get() < m_apPlayers[ClientID]->m_LastKickVote + (time_freq() * g_Config.m_SvVoteKickDelay))
				{
					SendChatLocalized(ClientID, "There's a %d second wait time between kick votes for each player please wait %d second(s)", g_Config.m_SvVoteKickDelay,
						(int)(((m_apPlayers[ClientID]->m_LastKickVote + (m_apPlayers[ClientID]->m_LastKickVote * time_freq())) / time_freq()) - (time_get() / time_freq())));
					m_apPlayers[ClientID]->m_LastKickVote = time_get();
					return;
				}
				//else if(!g_Config.m_SvVoteKick)
				else if((!g_Config.m_SvVoteKick || (g_Config.m_SvVoteKick == 2 && !GetDDRaceTeam(ClientID))) && !Authed) // allow admins to call kick votes even if they are forbidden
				{
					SendChatLocalized(ClientID, "Server does not allow voting to kick players");
					m_apPlayers[ClientID]->m_LastKickVote = time_get();
					return;
				}

				if(g_Config.m_SvVoteKickMin && !GetDDRaceTeam(ClientID))
				{
					char aaAddresses[MAX_CLIENTS][NETADDR_MAXSTRSIZE] = {{0}};
					for(int i = 0; i < MAX_CLIENTS; i++)
					{
						if(m_apPlayers[i])
						{
							Server()->GetClientAddr(i, aaAddresses[i], NETADDR_MAXSTRSIZE);
						}
					}
					int NumPlayers = 0;
					for(int i = 0; i < MAX_CLIENTS; ++i)
					{
						if(IsClientPlayer(i) && !GetDDRaceTeam(i))
						{
							NumPlayers++;
							for(int j = 0; j < i; j++)
							{
								if(IsClientPlayer(j) && !GetDDRaceTeam(j))
								{
									if(str_comp(aaAddresses[i], aaAddresses[j]) == 0)
									{
										NumPlayers--;
										break;
									}
								}
							}
						}
					}

					if(NumPlayers < g_Config.m_SvVoteKickMin)
					{
						SendChatLocalized(ClientID, "Kick voting requires %d players", g_Config.m_SvVoteKickMin);
						return;
					}
				}

				int KickID = str_toint(pMsg->m_Value);

				if(KickID < 0 || KickID >= MAX_CLIENTS || !m_apPlayers[KickID])
				{
					SendChatLocalized(ClientID, "Invalid client id to kick");
					return;
				}
				if(KickID == ClientID)
				{
					SendChatLocalized(ClientID, "You can't kick yourself");
					return;
				}
				if(!Server()->ReverseTranslate(KickID, ClientID))
				{
					return;
				}
				int KickedAuthed = Server()->GetAuthedState(KickID);
				if(KickedAuthed > Authed)
				{
					SendChatLocalized(ClientID, "You can't kick authorized players");
					m_apPlayers[ClientID]->m_LastKickVote = time_get();
					SendChatLocalized(KickID, "'%s' called for vote to kick you", Server()->ClientName(ClientID));
					return;
				}

				// Don't allow kicking if a player has no character
				if(!GetPlayerChar(ClientID) || !GetPlayerChar(KickID) || GetDDRaceTeam(ClientID) != GetDDRaceTeam(KickID))
				{
					SendChatLocalized(ClientID, "You can only kick players in your room");
					m_apPlayers[ClientID]->m_LastKickVote = time_get();
					return;
				}

				str_format(aChatmsg, sizeof(aChatmsg), "'%s' called for vote to kick '%s' (%s)", Server()->ClientName(ClientID), Server()->ClientName(KickID), aReason);
				str_format(aSixupDesc, sizeof(aSixupDesc), "%2d: %s", KickID, Server()->ClientName(KickID));

				if(!g_Config.m_SvVoteKickBantime || GetDDRaceTeam(KickID))
				{
					str_format(aCmd, sizeof(aCmd), "vote_kick %d kick %d Kicked by vote", KickID, KickID);
					str_format(aDesc, sizeof(aDesc), "Kick '%s'", Server()->ClientName(KickID));
				}
				else
				{
					char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
					Server()->GetClientAddr(KickID, aAddrStr, sizeof(aAddrStr));
					str_format(aCmd, sizeof(aCmd), "vote_kick %d ban %s %d Banned by vote", KickID, aAddrStr, g_Config.m_SvVoteKickBantime);
					str_format(aDesc, sizeof(aDesc), "Ban '%s'", Server()->ClientName(KickID));
				}
				IsInstanceVote = true;
				m_apPlayers[ClientID]->m_LastKickVote = time_get();
				m_VoteType = VOTE_TYPE_KICK;
				m_VoteVictim = KickID;
			}
			else if(str_comp_nocase(pMsg->m_Type, "spectate") == 0)
			{
				if(!g_Config.m_SvVoteSpectate)
				{
					SendChatLocalized(ClientID, "Server does not allow voting to move players to spectators");
					return;
				}

				int SpectateID = str_toint(pMsg->m_Value);

				if(SpectateID < 0 || SpectateID >= MAX_CLIENTS || !m_apPlayers[SpectateID] || m_apPlayers[SpectateID]->GetTeam() == TEAM_SPECTATORS)
				{
					SendChatLocalized(ClientID, "Invalid client id to move");
					return;
				}
				if(SpectateID == ClientID)
				{
					SendChatLocalized(ClientID, "You can't move yourself");
					return;
				}
				if(!Server()->ReverseTranslate(SpectateID, ClientID))
				{
					return;
				}

				if(GetDDRaceTeam(ClientID) != GetDDRaceTeam(SpectateID))
				{
					SendChatLocalized(ClientID, "You can only move players in your room to spectators");
					return;
				}

				str_format(aSixupDesc, sizeof(aSixupDesc), "%2d: %s", SpectateID, Server()->ClientName(SpectateID));
				str_format(aChatmsg, sizeof(aChatmsg), "'%s' called for vote to move '%s' to spectators (%s)", Server()->ClientName(ClientID), Server()->ClientName(SpectateID), aReason);
				str_format(aDesc, sizeof(aDesc), "Move '%s' to spectators", Server()->ClientName(SpectateID));
				str_format(aCmd, sizeof(aCmd), "server_command set_team %d -1 %d", SpectateID, g_Config.m_SvVoteSpectateRejoindelay);
				IsInstanceVote = true;
				m_VoteType = VOTE_TYPE_SPECTATE;
				m_VoteVictim = SpectateID;
			}

			if(!IsNoConsent && RateLimitPlayerVote(ClientID))
				return;

			if(aCmd[0])
			{
				if(IsInstanceVote)
				{
					SGameInstance Instance = PlayerGameInstance(ClientID);
					if(!Instance.m_IsCreated)
					{
						Console()->Print(
							IConsole::OUTPUT_LEVEL_STANDARD,
							"instancevote",
							"The room does not exist");
					}
					else
					{
						Instance.m_pController->SetVoteType(m_VoteType);
						if(m_VoteType == VOTE_TYPE_KICK || m_VoteType == VOTE_TYPE_SPECTATE)
							Instance.m_pController->SetVoteVictim(m_VoteVictim);

						if(IsNoConsent)
							Instance.m_pController->InstanceConsole()->ExecuteLine(aCmd, ClientID);
						else
							Instance.m_pController->CallVote(ClientID, aDesc, aCmd, aReason, aChatmsg, aSixupDesc[0] ? aSixupDesc : 0);
					}
				}
				else if(str_comp(aCmd, "info") != 0)
				{
					if(IsNoConsent)
					{
						m_ChatResponseTargetID = ClientID;
						Console()->ExecuteLine(aCmd, ClientID);
						m_ChatResponseTargetID = -1;
					}
					else
						CallVote(ClientID, aDesc, aCmd, aReason, aChatmsg, aSixupDesc[0] ? aSixupDesc : 0);
				}
			}
		}
		else if(MsgID == NETMSGTYPE_CL_VOTE)
		{
			if(g_Config.m_SvSpamprotection && pPlayer->m_LastVoteTry && pPlayer->m_LastVoteTry + Server()->TickSpeed() * 3 > Server()->Tick())
				return;

			int64 Now = Server()->Tick();

			pPlayer->m_LastVoteTry = Now;
			pPlayer->UpdatePlaytime();

			CNetMsg_Cl_Vote *pMsg = (CNetMsg_Cl_Vote *)pRawMsg;
			if(!pMsg->m_Vote)
				return;

			pPlayer->m_Vote = pMsg->m_Vote;
			pPlayer->m_VotePos = ++m_VotePos;

			SGameInstance Instance = PlayerGameInstance(ClientID);
			if(Instance.m_Init && Instance.m_pController->IsVoting())
				Instance.m_pController->VoteUpdate();

			if(!m_VoteCloseTime)
				return;

			m_VoteUpdate = true;
		}
		else if(MsgID == NETMSGTYPE_CL_SETTEAM && !pPlayerWorld->m_Paused)
		{
			CNetMsg_Cl_SetTeam *pMsg = (CNetMsg_Cl_SetTeam *)pRawMsg;

			if(pPlayer->GetTeam() == pMsg->m_Team || (g_Config.m_SvSpamprotection && pPlayer->m_LastSetTeam && pPlayer->m_LastSetTeam + Server()->TickSpeed() * g_Config.m_SvTeamChangeDelay > Server()->Tick()))
				return;

			if(pPlayer->m_TeamChangeTick > Server()->Tick())
			{
				pPlayer->m_LastSetTeam = Server()->Tick();
				int TimeLeft = (pPlayer->m_TeamChangeTick - Server()->Tick()) / Server()->TickSpeed();
				char aTime[32];
				str_time((int64)TimeLeft * 100, TIME_HOURS, aTime, sizeof(aTime));
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "Time to wait before changing team: %s", aTime);
				SendBroadcast(aBuf, ClientID);
				return;
			}

			SGameInstance Instance = PlayerGameInstance(ClientID);
			if(!Instance.m_Init)
			{
				SendChatLocalized(ClientID, "You can't change your team before the room is ready");
				return;
			}

			// Switch team on given client and kill/respawn him
			if(Instance.m_pController->CanJoinTeam(pMsg->m_Team, ClientID, true) && Instance.m_pController->CanChangeTeam(pPlayer, pMsg->m_Team))
			{
				if(pPlayer->IsPaused())
					SendChatLocalized(ClientID, "Use /pause first then you can kill");
				else
				{
					if(pPlayer->GetTeam() == TEAM_SPECTATORS || pMsg->m_Team == TEAM_SPECTATORS)
						m_VoteUpdate = true;
					Instance.m_pController->DoTeamChange(pPlayer, pMsg->m_Team);
					pPlayer->m_TeamChangeTick = Server()->Tick();
				}
			}
			else
			{
				SendChatLocalized(ClientID, "You can't change your team right now");
			}
		}
		else if(MsgID == NETMSGTYPE_CL_ISDDNETLEGACY)
		{
			IServer::CClientInfo Info;
			Server()->GetClientInfo(ClientID, &Info);
			if(Info.m_GotDDNetVersion)
			{
				return;
			}
			int DDNetVersion = pUnpacker->GetInt();
			if(pUnpacker->Error() || DDNetVersion < 0)
			{
				DDNetVersion = VERSION_DDRACE;
			}
			Server()->SetClientDDNetVersion(ClientID, DDNetVersion);
			OnClientDDNetVersionKnown(ClientID);
		}
		// PvP: disable clients showother settings

		// else if(MsgID == NETMSGTYPE_CL_SHOWOTHERSLEGACY)
		// {
		// 	if(g_Config.m_SvShowOthers && !g_Config.m_SvShowOthersDefault)
		// 	{
		// 		CNetMsg_Cl_ShowOthersLegacy *pMsg = (CNetMsg_Cl_ShowOthersLegacy *)pRawMsg;
		// 		pPlayer->m_ShowOthers = pMsg->m_Show;
		// 	}
		// }
		// else if(MsgID == NETMSGTYPE_CL_SHOWOTHERS)
		// {
		// 	if(g_Config.m_SvShowOthers && !g_Config.m_SvShowOthersDefault)
		// 	{
		// 		CNetMsg_Cl_ShowOthers *pMsg = (CNetMsg_Cl_ShowOthers *)pRawMsg;
		// 		pPlayer->m_ShowOthers = pMsg->m_Show;
		// 	}
		// }

		// PvP: allow spec zoom
		else if(MsgID == NETMSGTYPE_CL_SHOWDISTANCE)
		{
			CNetMsg_Cl_ShowDistance *pMsg = (CNetMsg_Cl_ShowDistance *)pRawMsg;
			pPlayer->m_ShowDistance = vec2(pMsg->m_X, pMsg->m_Y);
		}
		else if(MsgID == NETMSGTYPE_CL_SETSPECTATORMODE && !pPlayerWorld->m_Paused)
		{
			CNetMsg_Cl_SetSpectatorMode *pMsg = (CNetMsg_Cl_SetSpectatorMode *)pRawMsg;

			pMsg->m_SpectatorID = clamp(pMsg->m_SpectatorID, (int)SPEC_FOLLOW, MAX_CLIENTS - 1);

			if(pMsg->m_SpectatorID >= 0)
				if(!Server()->ReverseTranslate(pMsg->m_SpectatorID, ClientID))
					return;

			if((g_Config.m_SvSpamprotection && pPlayer->m_LastSetSpectatorMode && pPlayer->m_LastSetSpectatorMode + Server()->TickSpeed() / 4 > Server()->Tick()))
				return;

			pPlayer->m_LastSetSpectatorMode = Server()->Tick();
			pPlayer->UpdatePlaytime();

			SGameInstance Instance = PlayerGameInstance(ClientID);
			if(!pPlayer->SetSpecMode(pMsg->m_SpectatorID == -1 ? CPlayer::ESpecMode::FREEVIEW : CPlayer::ESpecMode::PLAYER, pMsg->m_SpectatorID) && Instance.m_Init)
			{
				if(pMsg->m_SpectatorID == -1 && !Server()->IsSixup(ClientID))
					SendChatLocalized(ClientID, "You can't freeview right now");
				else
					Instance.m_pController->SendGameMsg(GAMEMSG_SPEC_INVALIDID, ClientID);
			}
		}
		else if(MsgID == NETMSGTYPE_CL_CHANGEINFO)
		{
			if(g_Config.m_SvSpamprotection && pPlayer->m_LastChangeInfo && pPlayer->m_LastChangeInfo + Server()->TickSpeed() * g_Config.m_SvInfoChangeDelay > Server()->Tick())
				return;

			bool SixupNeedsUpdate = false;

			CNetMsg_Cl_ChangeInfo *pMsg = (CNetMsg_Cl_ChangeInfo *)pRawMsg;
			if(!str_utf8_check(pMsg->m_pName) || !str_utf8_check(pMsg->m_pClan) || !str_utf8_check(pMsg->m_pSkin))
			{
				return;
			}
			pPlayer->m_LastChangeInfo = Server()->Tick();
			pPlayer->UpdatePlaytime();

			// set infos
			if(Server()->WouldClientNameChange(ClientID, pMsg->m_pName) && !ProcessSpamProtection(ClientID))
			{
				char aOldName[MAX_NAME_LENGTH];
				str_copy(aOldName, Server()->ClientName(ClientID), sizeof(aOldName));

				Server()->SetClientName(ClientID, pMsg->m_pName);

				char aChatText[256];
				str_format(aChatText, sizeof(aChatText), "'%s' changed name to '%s'", aOldName, Server()->ClientName(ClientID));
				SendChat(-1, CGameContext::CHAT_ALL, aChatText);

				SixupNeedsUpdate = true;
			}

			if(str_comp(Server()->ClientClan(ClientID), pMsg->m_pClan))
				SixupNeedsUpdate = true;
			Server()->SetClientClan(ClientID, pMsg->m_pClan);

			if(Server()->ClientFlag(ClientID) != pMsg->m_Flag)
				SixupNeedsUpdate = true;
			Server()->SetClientFlag(ClientID, pMsg->m_Flag);
			if(pMsg->m_Flag >= 0 && pMsg->m_Flag < 1024)
				UpdatePlayerLang(ClientID, m_FlagLangMap[pMsg->m_Flag], true);
			else
				UpdatePlayerLang(ClientID, 0, true);

			if(str_startswith(pMsg->m_pSkin, "x_") || str_find(pMsg->m_pSkin, "_x_"))
				pPlayer->m_TeeInfos.m_SkinName[0] = 0;
			else
				str_copy(pPlayer->m_TeeInfos.m_SkinName, pMsg->m_pSkin, sizeof(pPlayer->m_TeeInfos.m_SkinName));
			pPlayer->m_TeeInfos.m_UseCustomColor = pMsg->m_UseCustomColor;
			pPlayer->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
			pPlayer->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;
			if(!Server()->IsSixup(ClientID))
				pPlayer->m_TeeInfos.ToSixup();

			if(SixupNeedsUpdate)
				SendClientInfo(ClientID);
			else
				SendSkinInfo(ClientID);

			Server()->ExpireServerInfo();
		}
		else if(MsgID == NETMSGTYPE_CL_EMOTICON && !pPlayerWorld->m_Paused)
		{
			CNetMsg_Cl_Emoticon *pMsg = (CNetMsg_Cl_Emoticon *)pRawMsg;

			if(g_Config.m_SvSpamprotection && pPlayer->m_LastEmote && pPlayer->m_LastEmote + Server()->TickSpeed() * g_Config.m_SvEmoticonDelay > Server()->Tick())
				return;

			pPlayer->m_LastEmote = Server()->Tick();
			pPlayer->UpdatePlaytime();

			SendEmoticon(ClientID, pMsg->m_Emoticon);
			CCharacter *pChr = pPlayer->GetCharacter();
			if(pChr && g_Config.m_SvEmotionalTees && pPlayer->m_EyeEmoteEnabled)
			{
				int EmoteType = EMOTE_NORMAL;
				switch(pMsg->m_Emoticon)
				{
				case EMOTICON_EXCLAMATION:
				case EMOTICON_GHOST:
				case EMOTICON_QUESTION:
				case EMOTICON_WTF:
					EmoteType = EMOTE_SURPRISE;
					break;
				case EMOTICON_DOTDOT:
				case EMOTICON_DROP:
				case EMOTICON_ZZZ:
					EmoteType = EMOTE_BLINK;
					break;
				case EMOTICON_EYES:
				case EMOTICON_HEARTS:
				case EMOTICON_MUSIC:
					EmoteType = EMOTE_HAPPY;
					break;
				case EMOTICON_OOP:
				case EMOTICON_SORRY:
				case EMOTICON_SUSHI:
					EmoteType = EMOTE_PAIN;
					break;
				case EMOTICON_DEVILTEE:
				case EMOTICON_SPLATTEE:
				case EMOTICON_ZOMG:
					EmoteType = EMOTE_ANGRY;
					break;
				default:
					break;
				}
				pChr->SetEmote(EmoteType, Server()->Tick() + 2 * Server()->TickSpeed());
			}
		}
		else if(MsgID == NETMSGTYPE_CL_KILL && !pPlayerWorld->m_Paused)
		{
			SGameInstance Instance = PlayerGameInstance(pPlayer->GetCID());

			if(!Instance.m_Init)
				return;

			Instance.m_pController->OnKill(pPlayer);
		}
	}
	if(MsgID == NETMSGTYPE_CL_STARTINFO)
	{
		if(pPlayer->m_IsReadyToEnter)
			return;

		CNetMsg_Cl_StartInfo *pMsg = (CNetMsg_Cl_StartInfo *)pRawMsg;

		if(!str_utf8_check(pMsg->m_pName))
		{
			Server()->Kick(ClientID, "name is not valid utf8");
			return;
		}
		if(!str_utf8_check(pMsg->m_pClan))
		{
			Server()->Kick(ClientID, "clan is not valid utf8");
			return;
		}
		if(!str_utf8_check(pMsg->m_pSkin))
		{
			Server()->Kick(ClientID, "skin is not valid utf8");
			return;
		}

		pPlayer->m_LastChangeInfo = Server()->Tick();

		// set start infos
		Server()->SetClientName(ClientID, pMsg->m_pName);
		Server()->SetClientClan(ClientID, pMsg->m_pClan);
		Server()->SetClientFlag(ClientID, pMsg->m_Flag);
		if(pMsg->m_Flag >= 0 && pMsg->m_Flag < 1024)
			UpdatePlayerLang(ClientID, m_FlagLangMap[pMsg->m_Flag], true);
		else
			UpdatePlayerLang(ClientID, 0, true);
		if(str_startswith(pMsg->m_pSkin, "x_") || str_find(pMsg->m_pSkin, "_x_"))
			pPlayer->m_TeeInfos.m_SkinName[0] = 0;
		else
			str_copy(pPlayer->m_TeeInfos.m_SkinName, pMsg->m_pSkin, sizeof(pPlayer->m_TeeInfos.m_SkinName));
		pPlayer->m_TeeInfos.m_UseCustomColor = pMsg->m_UseCustomColor;
		pPlayer->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
		pPlayer->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;
		if(!Server()->IsSixup(ClientID))
			pPlayer->m_TeeInfos.ToSixup();

		// send clear vote options
		CNetMsg_Sv_VoteClearOptions ClearMsg;
		Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientID);

		// begin sending vote options
		pPlayer->m_SendVoteIndex = 0;

		// send tuning parameters to client
		SendTuningParams(ClientID, pPlayer->m_TuneZone);

		// client is ready to enter
		pPlayer->m_IsReadyToEnter = true;
		CNetMsg_Sv_ReadyToEnter m;
		Server()->SendPackMsg(&m, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientID);

		Server()->ExpireServerInfo();
	}
}

bool CheckClientID2(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return false;
	return true;
}

void CGameContext::ConTuneParam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pParamName = pResult->GetString(0);
	float NewValue = pResult->GetFloat(1);

	if(pSelf->Tuning()->Set(pParamName, NewValue))
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "%s changed to %.2f", pParamName, NewValue);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
		pSelf->SendTuningParams(-1);
	}
	else
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "No such tuning parameter");
}

void CGameContext::ConToggleTuneParam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pParamName = pResult->GetString(0);
	float OldValue;

	if(!pSelf->Tuning()->Get(pParamName, &OldValue))
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "No such tuning parameter");
		return;
	}

	float NewValue = fabs(OldValue - pResult->GetFloat(1)) < 0.0001f ? pResult->GetFloat(2) : pResult->GetFloat(1);

	pSelf->Tuning()->Set(pParamName, NewValue);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s changed to %.2f", pParamName, NewValue);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
	pSelf->SendTuningParams(-1);
}

void CGameContext::ConTuneReset(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	/*CTuningParams TuningParams;
	*pSelf->Tuning() = TuningParams;
	pSelf->SendTuningParams(-1);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "Tuning reset");*/
	pSelf->ResetTuning();
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "Tuning reset");
}

void CGameContext::ConTuneDump(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char aBuf[256];
	for(int i = 0; i < pSelf->Tuning()->Num(); i++)
	{
		float v;
		pSelf->Tuning()->Get(i, &v);
		str_format(aBuf, sizeof(aBuf), "%s %.2f", pSelf->Tuning()->ms_apNames[i], v);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
	}
}

void CGameContext::ConTuneZone(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int List = pResult->GetInteger(0);
	const char *pParamName = pResult->GetString(1);
	float NewValue = pResult->GetFloat(2);

	if(List >= 0 && List < NUM_TUNEZONES)
	{
		if(pSelf->TuningList()[List].Set(pParamName, NewValue))
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "%s in zone %d changed to %.2f", pParamName, List, NewValue);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
			pSelf->SendTuningParams(-1, List);
		}
		else
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "No such tuning parameter");
	}
}

void CGameContext::ConTuneDumpZone(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int List = pResult->GetInteger(0);
	char aBuf[256];
	if(List >= 0 && List < NUM_TUNEZONES)
	{
		for(int i = 0; i < pSelf->TuningList()[List].Num(); i++)
		{
			float v;
			pSelf->TuningList()[List].Get(i, &v);
			str_format(aBuf, sizeof(aBuf), "zone %d: %s %.2f", List, pSelf->TuningList()[List].ms_apNames[i], v);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
		}
	}
}

void CGameContext::ConTuneResetZone(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CTuningParams TuningParams;
	if(pResult->NumArguments())
	{
		int List = pResult->GetInteger(0);
		if(List >= 0 && List < NUM_TUNEZONES)
		{
			pSelf->TuningList()[List] = TuningParams;
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "Tunezone %d reset", List);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
			pSelf->SendTuningParams(-1, List);
		}
	}
	else
	{
		for(int i = 0; i < NUM_TUNEZONES; i++)
		{
			*(pSelf->TuningList() + i) = TuningParams;
			pSelf->SendTuningParams(-1, i);
		}
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "All Tunezones reset");
	}
}

void CGameContext::ConTuneSetZoneMsgEnter(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pResult->NumArguments())
	{
		int List = pResult->GetInteger(0);
		if(List >= 0 && List < NUM_TUNEZONES)
		{
			str_copy(pSelf->m_aaZoneEnterMsg[List], pResult->GetString(1), sizeof(pSelf->m_aaZoneEnterMsg[List]));
		}
	}
}

void CGameContext::ConTuneSetZoneMsgLeave(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pResult->NumArguments())
	{
		int List = pResult->GetInteger(0);
		if(List >= 0 && List < NUM_TUNEZONES)
		{
			str_copy(pSelf->m_aaZoneLeaveMsg[List], pResult->GetString(1), sizeof(pSelf->m_aaZoneLeaveMsg[List]));
		}
	}
}

void CGameContext::ConSwitchOpen(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Switch = pResult->GetInteger(0);

	if(pSelf->Collision()->m_NumSwitchers > 0 && Switch >= 0 && Switch < pSelf->Collision()->m_NumSwitchers + 1)
	{
		pSelf->Collision()->m_pSwitchers[Switch].m_Initial = false;
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "switch %d opened by default", Switch);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
	}
}

void CGameContext::ConChangeMap(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->Server()->ChangeMap(pResult->NumArguments() ? pResult->GetString(0) : "");
}

void CGameContext::ConBroadcast(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	char aBuf[1024];
	str_copy(aBuf, pResult->GetString(0), sizeof(aBuf));

	int i, j;
	for(i = 0, j = 0; aBuf[i]; i++, j++)
	{
		if(aBuf[i] == '\\' && aBuf[i + 1] == 'n')
		{
			aBuf[j] = '\n';
			i++;
		}
		else if(i != j)
		{
			aBuf[j] = aBuf[i];
		}
	}
	aBuf[j] = '\0';

	pSelf->SendBroadcast(aBuf, -1);
}

void CGameContext::ConSay(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendChat(-1, CGameContext::CHAT_ALL, pResult->GetString(0));
}

void CGameContext::ConSetTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int ClientID = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS - 1);
	int Team = clamp(pResult->GetInteger(1), -1, 1);
	int Delay = pResult->NumArguments() > 2 ? pResult->GetInteger(2) : 0;
	if(!pSelf->m_apPlayers[ClientID])
		return;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "moved client %d to team %d", ClientID, Team);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	pSelf->m_apPlayers[ClientID]->Pause(false, false); // reset /spec and /pause to allow rejoin
	pSelf->m_apPlayers[ClientID]->m_TeamChangeTick = pSelf->Server()->Tick() + pSelf->Server()->TickSpeed() * Delay * 60;

	SGameInstance Instance = pSelf->PlayerGameInstance(ClientID);
	if(Instance.m_Init)
		Instance.m_pController->DoTeamChange(pSelf->m_apPlayers[ClientID], Team);

	if(Team == TEAM_SPECTATORS)
		pSelf->m_apPlayers[ClientID]->Pause(false, true);
}

void CGameContext::ConAddVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pDescription = pResult->GetString(0);
	const char *pCommand = pResult->GetString(1);

	pSelf->AddVote(pDescription, pCommand);
}

void CGameContext::AddVote(const char *pDescription, const char *pCommand)
{
	if(m_NumVoteOptions == MAX_VOTE_OPTIONS)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "maximum number of vote options reached");
		return;
	}

	if(str_startswith(pDescription, "⨀") || str_startswith(pDescription, "⨂"))
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "vote description cannot starts with the reserved icon 'U+2A00' and 'U+2A02' for room list");
		return;
	}

	if(str_startswith(pDescription, "☐"))
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "vote description cannot starts with the reserved icon 'U+2610' for room options");
		return;
	}

	// check for valid option
	if(!Console()->LineIsValid(pCommand) || str_length(pCommand) >= VOTE_CMD_LENGTH)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid command '%s'", pCommand);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}
	while(*pDescription == ' ')
		pDescription++;
	if(str_length(pDescription) >= VOTE_DESC_LENGTH || *pDescription == 0)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid option '%s'", pDescription);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	// check for duplicate entry
	CVoteOptionServer *pOption = m_pVoteOptionFirst;
	while(pOption)
	{
		if(str_comp_nocase(pDescription, pOption->m_aDescription) == 0)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "option '%s' already exists", pDescription);
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}
		pOption = pOption->m_pNext;
	}

	// add the option
	++m_NumVoteOptions;
	int Len = str_length(pCommand);

	pOption = (CVoteOptionServer *)m_pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
	pOption->m_pNext = 0;
	pOption->m_pPrev = m_pVoteOptionLast;
	if(pOption->m_pPrev)
		pOption->m_pPrev->m_pNext = pOption;
	m_pVoteOptionLast = pOption;
	if(!m_pVoteOptionFirst)
		m_pVoteOptionFirst = pOption;

	str_copy(pOption->m_aDescription, pDescription, sizeof(pOption->m_aDescription));
	mem_copy(pOption->m_aCommand, pCommand, Len + 1);

	// start reloading vote option list
	// clear vote options
	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, -1);

	// reset sending of vote options
	for(auto &pPlayer : m_apPlayers)
	{
		if(pPlayer)
			pPlayer->m_SendVoteIndex = 0;
	}
}

void CGameContext::LoadLanguageFiles()
{
	mem_zero(m_FlagLangMap, sizeof(m_FlagLangMap));

	IOHANDLE File = Storage()->OpenFile("languages/index.txt", IOFLAG_READ, IStorage::TYPE_ALL);
	if(!File)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", "couldn't open index file");
		return;
	}

	char aLang[128];
	char aLangLocalized[128];
	char aFlags[512];
	char aCodes[64];
	CLineReader LineReader;
	LineReader.Init(File);
	char *pLine;
	while((pLine = LineReader.Get()))
	{
		if(!str_length(pLine) || pLine[0] == '#') // skip empty lines and comments
			continue;

		str_copy(aLang, pLine, sizeof(aLang));

		pLine = LineReader.Get();
		if(!pLine)
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", "unexpected end of index file");
			break;
		}

		if(pLine[0] != '=' || pLine[1] != '=' || pLine[2] != ' ')
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "malform replacement for index '%s'", aLang);
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);
			(void)LineReader.Get();
			continue;
		}
		str_copy(aLangLocalized, pLine + 3, sizeof(aLangLocalized));

		pLine = LineReader.Get();
		if(!pLine)
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", "unexpected end of index file");
			break;
		}

		if(pLine[0] != '=' || pLine[1] != '=' || pLine[2] != ' ')
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "malform replacement for index '%s'", aLang);
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);
			(void)LineReader.Get();
			continue;
		}
		str_copy(aFlags, pLine + 3, sizeof(aFlags));

		pLine = LineReader.Get();
		if(!pLine)
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", "unexpected end of index file");
			break;
		}

		if(pLine[0] != '=' || pLine[1] != '=' || pLine[2] != ' ')
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "malform replacement for index '%s'", aLang);
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);
			continue;
		}
		str_copy(aCodes, pLine + 3, sizeof(aCodes));

		char aFileName[128];
		str_format(aFileName, sizeof(aFileName), "languages/%s.txt", aLang);
		const int LangIndex = g_Localization.Load(aFileName, Storage(), Console());

		char *pStart = aFlags;
		while(*pStart != '\0')
		{
			char *pEnd = pStart;
			while(*pEnd != '\0' && *pEnd != ',')
				pEnd++;
			if(pStart != pEnd)
			{
				if(*pEnd != '\0')
				{
					*pEnd = '\0';
					m_FlagLangMap[str_toint(pStart)] = LangIndex;
				}
				else
				{
					m_FlagLangMap[str_toint(pStart)] = LangIndex;
					pStart = pEnd;
					break;
				}
			}
			pStart = pEnd + 1;
		}

		// MYTODO: add /lang command with code
	}
	io_close(File);
}

void CGameContext::ConRemoveVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pDescription = pResult->GetString(0);

	// check for valid option
	CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
	while(pOption)
	{
		if(str_comp_nocase(pDescription, pOption->m_aDescription) == 0)
			break;
		pOption = pOption->m_pNext;
	}
	if(!pOption)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "option '%s' does not exist", pDescription);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	// start reloading vote option list
	// clear vote options
	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	pSelf->Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, -1);

	// reset sending of vote options
	for(auto &pPlayer : pSelf->m_apPlayers)
	{
		if(pPlayer)
			pPlayer->m_SendVoteIndex = 0;
	}

	// TODO: improve this
	// remove the option
	--pSelf->m_NumVoteOptions;

	CHeap *pVoteOptionHeap = new CHeap();
	CVoteOptionServer *pVoteOptionFirst = 0;
	CVoteOptionServer *pVoteOptionLast = 0;
	int NumVoteOptions = pSelf->m_NumVoteOptions;
	for(CVoteOptionServer *pSrc = pSelf->m_pVoteOptionFirst; pSrc; pSrc = pSrc->m_pNext)
	{
		if(pSrc == pOption)
			continue;

		// copy option
		int Len = str_length(pSrc->m_aCommand);
		CVoteOptionServer *pDst = (CVoteOptionServer *)pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
		pDst->m_pNext = 0;
		pDst->m_pPrev = pVoteOptionLast;
		if(pDst->m_pPrev)
			pDst->m_pPrev->m_pNext = pDst;
		pVoteOptionLast = pDst;
		if(!pVoteOptionFirst)
			pVoteOptionFirst = pDst;

		str_copy(pDst->m_aDescription, pSrc->m_aDescription, sizeof(pDst->m_aDescription));
		mem_copy(pDst->m_aCommand, pSrc->m_aCommand, Len + 1);
	}

	// clean up
	delete pSelf->m_pVoteOptionHeap;
	pSelf->m_pVoteOptionHeap = pVoteOptionHeap;
	pSelf->m_pVoteOptionFirst = pVoteOptionFirst;
	pSelf->m_pVoteOptionLast = pVoteOptionLast;
	pSelf->m_NumVoteOptions = NumVoteOptions;
}

void CGameContext::ConForceVote(IConsole::IResult *pResult, void *pUserData)
{
	int ClientID = pResult->m_ClientID;
	if(!CheckClientID2(ClientID))
		return;

	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pType = pResult->GetString(0);
	const char *pValue = pResult->GetString(1);
	const char *pReason = pResult->NumArguments() > 2 && pResult->GetString(2)[0] ? pResult->GetString(2) : "No reason given";
	char aBuf[128] = {0};

	if(str_comp_nocase(pType, "option") == 0)
	{
		CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
		while(pOption)
		{
			if(str_comp_nocase(pValue, pOption->m_aDescription) == 0)
			{
				pSelf->SendChatLocalized(-1, aBuf, CHAT_SIX, "authorized player forced server option '%s' (%s)", pValue, pReason);
				pSelf->Console()->ExecuteLine(pOption->m_aCommand, ClientID);
				break;
			}

			pOption = pOption->m_pNext;
		}

		if(!pOption)
		{
			str_format(aBuf, sizeof(aBuf), "'%s' isn't an option on this server", pValue);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}
	}
	else if(str_comp_nocase(pType, "kick") == 0)
	{
		int KickID = str_toint(pValue);
		if(KickID < 0 || KickID >= MAX_CLIENTS || !pSelf->m_apPlayers[KickID])
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Invalid client id to kick");
			return;
		}

		if(!g_Config.m_SvVoteKickBantime)
		{
			str_format(aBuf, sizeof(aBuf), "kick %d %s", KickID, pReason);
			pSelf->Console()->ExecuteLine(aBuf);
		}
		else
		{
			char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
			pSelf->Server()->GetClientAddr(KickID, aAddrStr, sizeof(aAddrStr));
			str_format(aBuf, sizeof(aBuf), "ban %s %d %s", aAddrStr, g_Config.m_SvVoteKickBantime, pReason);
			pSelf->Console()->ExecuteLine(aBuf);
		}
	}
	else if(str_comp_nocase(pType, "spectate") == 0)
	{
		int SpectateID = str_toint(pValue);
		if(SpectateID < 0 || SpectateID >= MAX_CLIENTS || !pSelf->m_apPlayers[SpectateID] || pSelf->m_apPlayers[SpectateID]->GetTeam() == TEAM_SPECTATORS)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Invalid client id to move");
			return;
		}

		pSelf->SendChatLocalized(-1, "'%s' was moved to spectator (%s)", pSelf->Server()->ClientName(SpectateID), pReason);
		str_format(aBuf, sizeof(aBuf), "set_team %d -1 %d", SpectateID, g_Config.m_SvVoteSpectateRejoindelay);
		pSelf->Console()->ExecuteLine(aBuf);
	}
}

void CGameContext::ConClearVotes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	pSelf->Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, -1);
	pSelf->m_pVoteOptionHeap->Reset();
	pSelf->m_pVoteOptionFirst = 0;
	pSelf->m_pVoteOptionLast = 0;
	pSelf->m_NumVoteOptions = 0;

	// reset sending of vote options
	for(auto &pPlayer : pSelf->m_apPlayers)
	{
		if(pPlayer)
			pPlayer->m_SendVoteIndex = 0;
	}
}

struct CMapNameItem
{
	char m_aName[MAX_PATH_LENGTH - 4];

	bool operator<(const CMapNameItem &Other) const { return str_comp_nocase(m_aName, Other.m_aName) < 0; }
};

void CGameContext::ConVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	if(str_comp_nocase(pResult->GetString(0), "yes") == 0)
		pSelf->ForceVote(pResult->m_ClientID, true);
	else if(str_comp_nocase(pResult->GetString(0), "no") == 0)
		pSelf->ForceVote(pResult->m_ClientID, false);
}

void CGameContext::ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CGameContext *pSelf = (CGameContext *)pUserData;
		pSelf->SendMotd(-1);
	}
}

void CGameContext::ConchainUpdateRoomVotes(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CGameContext *pSelf = (CGameContext *)pUserData;
		pSelf->Teams()->UpdateVotes();
	}
}

void CGameContext::OnConsoleInit()
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConfig = Kernel()->RequestInterface<IConfigManager>()->Values();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pEngine = Kernel()->RequestInterface<IEngine>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();

	m_ChatPrintCBIndex = Console()->RegisterPrintCallback(0, SendChatResponse, this);

	Console()->Register("tune", "s[tuning] i[value]", CFGFLAG_SERVER | CFGFLAG_GAME, ConTuneParam, this, "Tune variable to value");
	Console()->Register("toggle_tune", "s[tuning] i[value 1] i[value 2]", CFGFLAG_SERVER | CFGFLAG_GAME, ConToggleTuneParam, this, "Toggle tune variable");
	Console()->Register("tune_reset", "", CFGFLAG_SERVER, ConTuneReset, this, "Reset tuning");
	Console()->Register("tune_dump", "", CFGFLAG_SERVER, ConTuneDump, this, "Dump tuning");
	Console()->Register("tune_zone", "i[zone] s[tuning] i[value]", CFGFLAG_SERVER | CFGFLAG_GAME, ConTuneZone, this, "Tune in zone a variable to value");
	Console()->Register("tune_zone_dump", "i[zone]", CFGFLAG_SERVER, ConTuneDumpZone, this, "Dump zone tuning in zone x");
	Console()->Register("tune_zone_reset", "?i[zone]", CFGFLAG_SERVER, ConTuneResetZone, this, "reset zone tuning in zone x or in all zones");
	Console()->Register("tune_zone_enter", "i[zone] r[message]", CFGFLAG_SERVER | CFGFLAG_GAME, ConTuneSetZoneMsgEnter, this, "which message to display on zone enter; use 0 for normal area");
	Console()->Register("tune_zone_leave", "i[zone] r[message]", CFGFLAG_SERVER | CFGFLAG_GAME, ConTuneSetZoneMsgLeave, this, "which message to display on zone leave; use 0 for normal area");
	Console()->Register("switch_open", "i[switch]", CFGFLAG_SERVER | CFGFLAG_GAME, ConSwitchOpen, this, "Whether a switch is deactivated by default (otherwise activated)");
	Console()->Register("change_map", "?r[map]", CFGFLAG_SERVER | CFGFLAG_STORE, ConChangeMap, this, "Change map");

	Console()->Register("broadcast", "r[message]", CFGFLAG_SERVER, ConBroadcast, this, "Broadcast message");
	Console()->Register("say", "r[message]", CFGFLAG_SERVER, ConSay, this, "Say in chat");
	Console()->Register("set_team", "i[id] i[team-id] ?i[delay in minutes]", CFGFLAG_SERVER, ConSetTeam, this, "Set team of player to team");

	Console()->Register("add_vote", "s[name] r[command]", CFGFLAG_SERVER, ConAddVote, this, "Add a voting option");
	Console()->Register("remove_vote", "r[name]", CFGFLAG_SERVER, ConRemoveVote, this, "remove a voting option");
	Console()->Register("force_vote", "s[name] s[command] ?r[reason]", CFGFLAG_SERVER, ConForceVote, this, "Force a voting option");
	Console()->Register("clear_votes", "", CFGFLAG_SERVER, ConClearVotes, this, "Clears the voting options");
	Console()->Register("vote", "r['yes'|'no']", CFGFLAG_SERVER, ConVote, this, "Force a vote to yes/no");
	Console()->Register("dump_antibot", "", CFGFLAG_SERVER, ConDumpAntibot, this, "Dumps the antibot status");

	Console()->Register("clear_gametypes", "", CFGFLAG_SERVER, ConClearGameTypes, this, "Set a default gametype for room 0. The default game type won't be avalible for room id >1");
	Console()->Register("lobby_gametype", "s[gametype] ?r[settings]", CFGFLAG_SERVER, ConSetDefaultGameType, this, "Set a default gametype for room 0. The default game type won't be avalible for room id >1");
	Console()->Register("lobby_gametypefile", "s[gametype] r[filename]", CFGFLAG_SERVER, ConSetDefaultGameTypeFile, this, "Set a default gametype for room 0. The default game type won't be avalible for room id >1");
	Console()->Register("add_gametype", "s[name] ?s[gametype] ?r[settings]", CFGFLAG_SERVER, ConAddGameType, this, "Register an gametype for rooms. First register will be the default for room 0");
	Console()->Register("add_gametypefile", "s[name] s[gametype] r[filename]", CFGFLAG_SERVER, ConAddGameTypeFile, this, "Register an gametype for rooms. First register will be the default for room 0");
	Console()->Register("mega_add_mapname", "r[name]", CFGFLAG_SERVER, ConAddMapName, this, "Mega map sub map names. Add it in order of map indexes, starting from map 1.");
	Console()->Register("room_setting", "i[room] ?r[settings]", CFGFLAG_SERVER, ConRoomSetting, this, "Invoke a command in a specified room");

	Console()->Chain("sv_motd", ConchainSpecialMotdupdate, this);
	Console()->Chain("sv_room", ConchainUpdateRoomVotes, this);
	Console()->Chain("sv_roomlist_votes", ConchainUpdateRoomVotes, this);
	Console()->Chain("sv_roomlist_vote_title", ConchainUpdateRoomVotes, this);

#define CONSOLE_COMMAND(name, params, flags, callback, userdata, help) m_pConsole->Register(name, params, flags, callback, userdata, help);
#include <game/ddracecommands.h>
#define CHAT_COMMAND(name, params, flags, callback, userdata, help) m_pConsole->Register(name, params, CFGFLAG_CHAT | flags, callback, userdata, help);
#include <game/ddracechat.h>

	// load localization
	LoadLanguageFiles();
}

void CGameContext::OnInit(/*class IKernel *pKernel*/)
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConfig = Kernel()->RequestInterface<IConfigManager>()->Values();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pEngine = Kernel()->RequestInterface<IEngine>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pAntibot = Kernel()->RequestInterface<IAntibot>();
	m_pAntibot->RoundStart(this);

	m_GameUuid = RandomUuid();

	uint64 aSeed[2];
	secure_random_fill(aSeed, sizeof(aSeed));
	m_Prng.Seed(aSeed);

	DeleteTempfile();

	for(int i = 0; i < NUM_NETOBJTYPES; i++)
		Server()->SnapSetStaticsize(i, m_NetObjHandler.GetObjSize(i));

	m_Layers.Init(Kernel());
	m_Collision.Init(&m_Layers, &m_Prng);

	char aMapName[128];
	int MapSize;
	SHA256_DIGEST MapSha256;
	int MapCrc;
	Server()->GetMapInfo(aMapName, sizeof(aMapName), &MapSize, &MapSha256, &MapCrc);

	// Reset Tunezones
	CTuningParams TuningParams;
	for(int i = 0; i < NUM_TUNEZONES; i++)
	{
		TuningList()[i] = TuningParams;
		TuningList()[i].Set("gun_curvature", 1.25);
		TuningList()[i].Set("gun_speed", 2200);
		TuningList()[i].Set("shotgun_curvature", 1.25);
		TuningList()[i].Set("shotgun_speed", 2750);
		TuningList()[i].Set("shotgun_speeddiff", 0.80);
	}

	for(int i = 0; i < NUM_TUNEZONES; i++)
	{
		// Send no text by default when changing tune zones.
		m_aaZoneEnterMsg[i][0] = 0;
		m_aaZoneLeaveMsg[i][0] = 0;
	}
	// Reset Tuning
	if(g_Config.m_SvTuneReset)
	{
		ResetTuning();
	}
	else
	{
		Tuning()->Set("gun_speed", 2200);
		Tuning()->Set("gun_curvature", 1.25);
		Tuning()->Set("shotgun_speed", 2750);
		Tuning()->Set("shotgun_speeddiff", 0.80);
		Tuning()->Set("shotgun_curvature", 1.25);
	}

	if(g_Config.m_SvDDRaceTuneReset)
	{
		g_Config.m_SvHit = 1;
		g_Config.m_SvEndlessDrag = 0;
		g_Config.m_SvOldTeleportHook = 0;
		g_Config.m_SvOldTeleportWeapons = 0;
		g_Config.m_SvTeleportHoldHook = 0;

		if(Collision()->m_NumSwitchers > 0)
			for(int i = 0; i < Collision()->m_NumSwitchers + 1; ++i)
				Collision()->m_pSwitchers[i].m_Initial = true;
	}

	Console()->ExecuteFile(g_Config.m_SvResetFile, -1);

	Teams()->ClearMaps();
	LoadMapSettings();
	m_Teams.Init(this);

	const char *pCensorFilename = "censorlist.txt";
	IOHANDLE File = Storage()->OpenFile(pCensorFilename, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!File)
	{
		dbg_msg("censorlist", "failed to open '%s'", pCensorFilename);
	}
	else
	{
		CLineReader LineReader;
		LineReader.Init(File);
		char *pLine;
		while((pLine = LineReader.Get()))
		{
			m_aCensorlist.add(pLine);
		}
		io_close(File);
	}

	// create all entities from the game layer
	CMapItemLayerTilemap *pTileMap = m_Layers.GameLayer();
	CTile *pTiles = (CTile *)Kernel()->RequestInterface<IMap>()->GetData(pTileMap->m_Data);

	CTile *pFront = 0;
	CSwitchTile *pSwitch = 0;
	CSpeedupTile *pSpeedup = 0;
	if(m_Layers.FrontLayer())
		pFront = (CTile *)Kernel()->RequestInterface<IMap>()->GetData(m_Layers.FrontLayer()->m_Front);
	if(m_Layers.SwitchLayer())
		pSwitch = (CSwitchTile *)Kernel()->RequestInterface<IMap>()->GetData(m_Layers.SwitchLayer()->m_Switch);
	if(m_Layers.SpeedupLayer())
		pSpeedup = (CSpeedupTile *)Kernel()->RequestInterface<IMap>()->GetData(m_Layers.SpeedupLayer()->m_Speedup);

	for(int y = 0; y < pTileMap->m_Height; y++)
	{
		for(int x = 0; x < pTileMap->m_Width; x++)
		{
			int Index = pTiles[y * pTileMap->m_Width + x].m_Index;

			if(Index == TILE_NPC)
			{
				m_Tuning.Set("player_collision", 0);
				dbg_msg("game_layer", "found no collision tile");
			}
			else if(Index == TILE_EHOOK)
			{
				g_Config.m_SvEndlessDrag = 1;
				dbg_msg("game_layer", "found unlimited hook time tile");
			}
			else if(Index == TILE_NOHIT)
			{
				g_Config.m_SvHit = 0;
				dbg_msg("game_layer", "found no weapons hitting others tile");
			}
			else if(Index == TILE_NPH)
			{
				m_Tuning.Set("player_hooking", 0);
				dbg_msg("game_layer", "found no player hooking tile");
			}

			if(Index >= ENTITY_OFFSET)
			{
				vec2 Pos(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
				//m_pController->OnEntity(Index-ENTITY_OFFSET, Pos);
				int MapIndex = 0;
				if(pSpeedup)
				{
					CSpeedupTile SpeedupTile = pSpeedup[y * pTileMap->m_Width + x];
					bool IsMegaMapIndex = SpeedupTile.m_Type == TILE_MEGAMAP_INDEX;
					MapIndex = IsMegaMapIndex ? SpeedupTile.m_MaxSpeed : 0;
				}
				Teams()->OnEntity(Index - ENTITY_OFFSET, Pos, LAYER_GAME, pTiles[y * pTileMap->m_Width + x].m_Flags, MapIndex);
			}

			if(pFront)
			{
				Index = pFront[y * pTileMap->m_Width + x].m_Index;
				if(Index == TILE_NPC)
				{
					m_Tuning.Set("player_collision", 0);
					dbg_msg("front_layer", "found no collision tile");
				}
				else if(Index == TILE_EHOOK)
				{
					g_Config.m_SvEndlessDrag = 1;
					dbg_msg("front_layer", "found unlimited hook time tile");
				}
				else if(Index == TILE_NOHIT)
				{
					g_Config.m_SvHit = 0;
					dbg_msg("front_layer", "found no weapons hitting others tile");
				}
				else if(Index == TILE_NPH)
				{
					m_Tuning.Set("player_hooking", 0);
					dbg_msg("front_layer", "found no player hooking tile");
				}
				if(Index >= ENTITY_OFFSET)
				{
					vec2 Pos(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
					int MapIndex = 0;
					if(pSpeedup)
					{
						CSpeedupTile SpeedupTile = pSpeedup[y * pTileMap->m_Width + x];
						bool IsMegaMapIndex = SpeedupTile.m_Type == TILE_MEGAMAP_INDEX;
						MapIndex = IsMegaMapIndex ? SpeedupTile.m_MaxSpeed : 0;
					}
					Teams()->OnEntity(Index - ENTITY_OFFSET, Pos, LAYER_FRONT, pFront[y * pTileMap->m_Width + x].m_Flags, MapIndex);
				}
			}
			if(pSwitch)
			{
				Index = pSwitch[y * pTileMap->m_Width + x].m_Type;
				// TODO: Add off by default door here
				// if (Index == TILE_DOOR_OFF)
				if(Index >= ENTITY_OFFSET)
				{
					vec2 Pos(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
					int MapIndex = 0;
					if(pSpeedup)
					{
						CSpeedupTile SpeedupTile = pSpeedup[y * pTileMap->m_Width + x];
						bool IsMegaMapIndex = SpeedupTile.m_Type == TILE_MEGAMAP_INDEX;
						MapIndex = IsMegaMapIndex ? SpeedupTile.m_MaxSpeed : 0;
					}
					Teams()->OnEntity(Index - ENTITY_OFFSET, Pos, LAYER_SWITCH, pSwitch[y * pTileMap->m_Width + x].m_Flags, MapIndex, pSwitch[y * pTileMap->m_Width + x].m_Number);
				}
			}
		}
	}

#ifdef CONF_DEBUG
	if(g_Config.m_DbgDummies)
	{
		for(int i = 0; i < g_Config.m_DbgDummies; i++)
		{
			OnClientConnected(MAX_CLIENTS - i - 1, 0);
		}
	}
#endif

	m_aVoteCommand[0] = 0;
}

void CGameContext::DeleteTempfile()
{
	if(m_aDeleteTempfile[0] != 0)
	{
		Storage()->RemoveFile(m_aDeleteTempfile, IStorage::TYPE_SAVE);
		m_aDeleteTempfile[0] = 0;
	}
}

void CGameContext::OnShutdown(bool FullShutdown)
{
	Antibot()->RoundEnd();

	DeleteTempfile();
	Console()->ResetServerGameSettings();
	Collision()->Dest();
	Clear();

	if(FullShutdown)
	{
		m_Teams.SetDefaultGameType(nullptr, nullptr, false);
		m_Teams.ClearGameTypes();
	}
}

void CGameContext::LoadMapSettings()
{
	IMap *pMap = Kernel()->RequestInterface<IMap>();
	int Start, Num;
	pMap->GetType(MAPITEMTYPE_INFO, &Start, &Num);
	for(int i = Start; i < Start + Num; i++)
	{
		int ItemID;
		CMapItemInfoSettings *pItem = (CMapItemInfoSettings *)pMap->GetItem(i, 0, &ItemID);
		int ItemSize = pMap->GetItemSize(i);
		if(!pItem || ItemID != 0)
			continue;

		if(ItemSize < (int)sizeof(CMapItemInfoSettings))
			break;
		if(!(pItem->m_Settings > -1))
			break;

		int Size = pMap->GetDataSize(pItem->m_Settings);
		char *pSettings = (char *)pMap->GetData(pItem->m_Settings);
		char *pNext = pSettings;
		while(pNext < pSettings + Size)
		{
			int StrSize = str_length(pNext) + 1;
			Console()->ExecuteLine(pNext, IConsole::CLIENT_ID_GAME);
			pNext += StrSize;
		}
		pMap->UnloadData(pItem->m_Settings);
		break;
	}

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "maps/%s.map.cfg", g_Config.m_SvMap);
	Console()->ExecuteFile(aBuf, IConsole::CLIENT_ID_NO_GAME);
}

void CGameContext::OnSnap(int ClientID)
{
	// add tuning to demo
	CTuningParams StandardTuning;
	if(ClientID == -1 && Server()->DemoRecorder_IsRecording() && mem_comp(&StandardTuning, &m_Tuning, sizeof(CTuningParams)) != 0)
	{
		CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
		int *pParams = (int *)&m_Tuning;
		for(unsigned i = 0; i < sizeof(m_Tuning) / sizeof(int); i++)
			Msg.AddInt(pParams[i]);
		Server()->SendMsg(&Msg, MSGFLAG_RECORD | MSGFLAG_NOSEND, ClientID);
	}

	Teams()->OnSnap(ClientID);

	for(auto &pPlayer : m_apPlayers)
	{
		if(pPlayer)
			pPlayer->Snap(ClientID);
	}

	if(ClientID > -1)
		m_apPlayers[ClientID]->FakeSnap();
}
void CGameContext::OnPreSnap() {}
void CGameContext::OnPostSnap()
{
	Teams()->OnPostSnap();
}

bool CGameContext::IsClientReadyToEnter(int ClientID) const
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_IsReadyToEnter ? true : false;
}

bool CGameContext::IsClientPlayer(int ClientID) const
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->GetTeam() != TEAM_SPECTATORS;
}

bool CGameContext::IsClientActivePlayer(int ClientID) const
{
	return m_apPlayers[ClientID] && !m_apPlayers[ClientID]->m_Afk && m_apPlayers[ClientID]->GetTeam() != TEAM_SPECTATORS;
}

bool CGameContext::CheckDisruptiveLeave(int ClientID)
{
	if(m_apPlayers[ClientID])
	{
		SGameInstance Instance = PlayerGameInstance(ClientID);
		if(Instance.m_Init)
			return Instance.m_pController->IsDisruptiveLeave(m_apPlayers[ClientID]);
	}

	return false;
}

CUuid CGameContext::GameUuid() const { return m_GameUuid; }
const char *CGameContext::GameType() const { return g_Config.m_SvTestingCommands ? TESTTYPE_NAME : CGameTeams::GameTypeName(); }
const char *CGameContext::Version() const { return GAME_VERSION; }
const char *CGameContext::NetVersion() const { return GAME_NETVERSION; }

IGameServer *CreateGameServer() { return new CGameContext; }

void CGameContext::SendChatResponseAll(const char *pLine, void *pUser)
{
	CGameContext *pSelf = (CGameContext *)pUser;

	static volatile int ReentryGuard = 0;
	const char *pLineOrig = pLine;

	if(ReentryGuard)
		return;
	ReentryGuard++;

	if(*pLine == '[')
		do
			pLine++;
		while((pLine - 2 < pLineOrig || *(pLine - 2) != ':') && *pLine != 0); // remove the category (e.g. [Console]: No Such Command)

	pSelf->SendChat(-1, CHAT_ALL, pLine);

	ReentryGuard--;
}

void CGameContext::SendChatResponse(const char *pLine, void *pUser)
{
	CGameContext *pSelf = (CGameContext *)pUser;
	int ClientID = pSelf->m_ChatResponseTargetID;

	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return;

	const char *pLineOrig = pLine;

	static volatile int ReentryGuard = 0;

	if(ReentryGuard)
		return;
	ReentryGuard++;

	if(pLine[0] == '[')
	{
		// Remove time and category: [20:39:00][Console]
		pLine = str_find(pLine, "]: ");
		if(pLine)
			pLine += 3;
		else
			pLine = pLineOrig;
	}

	pSelf->SendChatTarget(ClientID, pLine);

	ReentryGuard--;
}

bool CGameContext::PlayerCollision()
{
	float Temp;
	m_Tuning.Get("player_collision", &Temp);
	return Temp != 0.0f;
}

bool CGameContext::PlayerHooking()
{
	float Temp;
	m_Tuning.Get("player_hooking", &Temp);
	return Temp != 0.0f;
}

float CGameContext::PlayerJetpack()
{
	float Temp;
	m_Tuning.Get("player_jetpack", &Temp);
	return Temp;
}

void CGameContext::OnSetAuthed(int ClientID, int Level)
{
	if(m_apPlayers[ClientID])
	{
		char aBuf[512], aIP[NETADDR_MAXSTRSIZE];
		Server()->GetClientAddr(ClientID, aIP, sizeof(aIP));
		str_format(aBuf, sizeof(aBuf), "ban %s %d Banned by vote", aIP, g_Config.m_SvVoteKickBantime);
		if(!str_comp_nocase(m_aVoteCommand, aBuf) && Level > Server()->GetAuthedState(m_VoteCreator))
		{
			m_VoteEnforce = VOTE_ENFORCE_NO_ADMIN;
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "CGameContext", "Vote aborted by authorized login.");
		}
	}
}

int CGameContext::ProcessSpamProtection(int ClientID)
{
	if(!m_apPlayers[ClientID])
		return 0;
	if(g_Config.m_SvSpamprotection && m_apPlayers[ClientID]->m_LastChat && m_apPlayers[ClientID]->m_LastChat + Server()->TickSpeed() * g_Config.m_SvChatDelay > Server()->Tick())
		return 1;
	else if(g_Config.m_SvDnsblChat && Server()->DnsblBlack(ClientID))
	{
		SendChatLocalized(ClientID, "Players are not allowed to chat from VPNs at this time");
		return 1;
	}
	else
		m_apPlayers[ClientID]->m_LastChat = Server()->Tick();

	NETADDR Addr;
	Server()->GetClientAddr(ClientID, &Addr);

	int Muted = 0;
	if(m_apPlayers[ClientID]->m_JoinTick > m_NonEmptySince + 10 * Server()->TickSpeed())
		Muted = (m_apPlayers[ClientID]->m_JoinTick + Server()->TickSpeed() * g_Config.m_SvChatInitialDelay - Server()->Tick()) / Server()->TickSpeed();
	if(Muted <= 0)
	{
		for(int i = 0; i < m_NumMutes && Muted <= 0; i++)
		{
			if(!net_addr_comp_noport(&Addr, &m_aMutes[i].m_Addr))
				Muted = (m_aMutes[i].m_Expire - Server()->Tick()) / Server()->TickSpeed();
		}
	}

	if(Muted > 0)
	{
		SendChatLocalized(ClientID, "You are not permitted to talk for the next %d seconds.", Muted);
		return 1;
	}

	if((m_apPlayers[ClientID]->m_ChatScore += g_Config.m_SvChatPenalty) > g_Config.m_SvChatThreshold)
	{
		Mute(&Addr, g_Config.m_SvSpamMuteDuration, Server()->ClientName(ClientID));
		m_apPlayers[ClientID]->m_ChatScore = 0;
		return 1;
	}

	return 0;
}

int CGameContext::GetDDRaceTeam(int ClientID)
{
	return Teams()->m_Core.Team(ClientID);
}

void CGameContext::ResetTuning()
{
	CTuningParams TuningParams;
	m_Tuning = TuningParams;
	Tuning()->Set("gun_speed", 2200);
	Tuning()->Set("gun_curvature", 1.25);
	Tuning()->Set("shotgun_speed", 2750);
	Tuning()->Set("shotgun_speeddiff", 0.80);
	Tuning()->Set("shotgun_curvature", 1.25);
	SendTuningParams(-1);
}

void CGameContext::Whisper(int ClientID, char *pStr)
{
	char *pName;
	char *pMessage;
	int Error = 0;

	if(ProcessSpamProtection(ClientID))
		return;

	pStr = str_skip_whitespaces(pStr);

	int Victim;

	// add token
	if(*pStr == '"')
	{
		pStr++;

		pName = pStr;
		char *pDst = pStr; // we might have to process escape data
		while(1)
		{
			if(pStr[0] == '"')
				break;
			else if(pStr[0] == '\\')
			{
				if(pStr[1] == '\\')
					pStr++; // skip due to escape
				else if(pStr[1] == '"')
					pStr++; // skip due to escape
			}
			else if(pStr[0] == 0)
				Error = 1;

			*pDst = *pStr;
			pDst++;
			pStr++;
		}

		// write null termination
		*pDst = 0;

		pStr++;

		for(Victim = 0; Victim < MAX_CLIENTS; Victim++)
			if(str_comp(pName, Server()->ClientName(Victim)) == 0)
				break;
	}
	else
	{
		pName = pStr;
		while(1)
		{
			if(pStr[0] == 0)
			{
				Error = 1;
				break;
			}
			if(pStr[0] == ' ')
			{
				pStr[0] = 0;
				for(Victim = 0; Victim < MAX_CLIENTS; Victim++)
					if(str_comp(pName, Server()->ClientName(Victim)) == 0)
						break;

				pStr[0] = ' ';

				if(Victim < MAX_CLIENTS)
					break;
			}
			pStr++;
		}
	}

	if(pStr[0] != ' ')
	{
		Error = 1;
	}

	*pStr = 0;
	pStr++;

	pMessage = pStr;

	char aBuf[256];

	if(Error)
	{
		SendChatLocalized(ClientID, "Invalid whisper");
		return;
	}

	if(Victim >= MAX_CLIENTS || !CheckClientID2(Victim))
	{
		SendChatLocalized(ClientID, "No player with name '%s' found", pName);
		return;
	}

	WhisperID(ClientID, Victim, pMessage);
}

void CGameContext::WhisperID(int ClientID, int VictimID, const char *pMessage)
{
	if(!CheckClientID2(ClientID))
		return;

	if(!CheckClientID2(VictimID))
		return;

	SGameInstance Instance = PlayerGameInstance(ClientID);
	if(Instance.m_IsCreated && !Instance.m_pController->IsTeamplay() && g_Config.m_SvTournamentChat == 2)
		return;

	if(g_Config.m_SvTournamentChat > 0)
	{
		if(m_apPlayers[ClientID]->GetTeam() == TEAM_SPECTATORS || m_apPlayers[VictimID]->GetTeam() == TEAM_SPECTATORS)
			return;
		if(g_Config.m_SvTournamentChat == 2 && m_apPlayers[ClientID]->GetTeam() != m_apPlayers[VictimID]->GetTeam())
			return;
		if(m_apPlayers[ClientID]->GetTeam() != TEAM_SPECTATORS && GetPlayerDDRTeam(ClientID) != GetPlayerDDRTeam(VictimID))
			return;
	}

	if(m_apPlayers[ClientID])
		m_apPlayers[ClientID]->m_LastWhisperTo = VictimID;

	char aCensoredMessage[256];
	CensorMessage(aCensoredMessage, pMessage, sizeof(aCensoredMessage));

	char aBuf[256];

	if(Server()->IsSixup(ClientID))
	{
		protocol7::CNetMsg_Sv_Chat Msg;
		Msg.m_ClientID = ClientID;
		Msg.m_Mode = protocol7::CHAT_WHISPER;
		Msg.m_pMessage = aCensoredMessage;
		Msg.m_TargetID = VictimID;

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
	}
	else if(GetClientVersion(ClientID) >= VERSION_DDNET_WHISPER)
	{
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = CHAT_WHISPER_SEND;
		Msg.m_ClientID = VictimID;
		Msg.m_pMessage = aCensoredMessage;
		if(g_Config.m_SvDemoChat)
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
		else
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "[→ %s] %s", Server()->ClientName(VictimID), aCensoredMessage);
		SendChatTarget(ClientID, aBuf);
	}

	if(Server()->IsSixup(VictimID))
	{
		protocol7::CNetMsg_Sv_Chat Msg;
		Msg.m_ClientID = ClientID;
		Msg.m_Mode = protocol7::CHAT_WHISPER;
		Msg.m_pMessage = aCensoredMessage;
		Msg.m_TargetID = VictimID;

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, VictimID);
	}
	else if(GetClientVersion(VictimID) >= VERSION_DDNET_WHISPER)
	{
		CNetMsg_Sv_Chat Msg2;
		Msg2.m_Team = CHAT_WHISPER_RECV;
		Msg2.m_ClientID = ClientID;
		Msg2.m_pMessage = aCensoredMessage;
		if(g_Config.m_SvDemoChat)
			Server()->SendPackMsg(&Msg2, MSGFLAG_VITAL, VictimID);
		else
			Server()->SendPackMsg(&Msg2, MSGFLAG_VITAL | MSGFLAG_NORECORD, VictimID);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "[← %s] %s", Server()->ClientName(ClientID), aCensoredMessage);
		SendChatTarget(VictimID, aBuf);
	}
}

void CGameContext::Converse(int ClientID, char *pStr)
{
	CPlayer *pPlayer = m_apPlayers[ClientID];
	if(!pPlayer)
		return;

	if(ProcessSpamProtection(ClientID))
		return;

	if(pPlayer->m_LastWhisperTo < 0)
		SendChatLocalized(ClientID, "You do not have an ongoing conversation. Whisper to someone to start one");
	else
	{
		WhisperID(ClientID, pPlayer->m_LastWhisperTo, pStr);
	}
}

bool CGameContext::IsVersionBanned(int Version)
{
	char aVersion[16];
	str_format(aVersion, sizeof(aVersion), "%d", Version);

	return str_in_list(g_Config.m_SvBannedVersions, ",", aVersion);
}

void CGameContext::List(int ClientID, const char *pFilter)
{
	int Total = 0;
	char aBuf[256];
	int Bufcnt = 0;
	if(pFilter[0])
		SendChatLocalized(ClientID, "Listing players with '%s' in name:", pFilter);
	else
		SendChatLocalized(ClientID, "Listing all players:");
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
		{
			Total++;
			const char *pName = Server()->ClientName(i);
			if(str_find_nocase(pName, pFilter) == NULL)
				continue;
			if(Bufcnt + str_length(pName) + 4 > 256)
			{
				SendChatTarget(ClientID, aBuf);
				Bufcnt = 0;
			}
			if(Bufcnt != 0)
			{
				str_format(&aBuf[Bufcnt], sizeof(aBuf) - Bufcnt, ", %s", pName);
				Bufcnt += 2 + str_length(pName);
			}
			else
			{
				str_format(&aBuf[Bufcnt], sizeof(aBuf) - Bufcnt, "%s", pName);
				Bufcnt += str_length(pName);
			}
		}
	}
	if(Bufcnt != 0)
		SendChatTarget(ClientID, aBuf);
	SendChatLocalized(ClientID, "%d players online", Total);
}

int CGameContext::GetClientVersion(int ClientID) const
{
	IServer::CClientInfo Info = {0};
	Server()->GetClientInfo(ClientID, &Info);
	return Info.m_DDNetVersion;
}

bool CGameContext::PlayerModerating() const
{
	for(const auto &pPlayer : m_apPlayers)
	{
		if(pPlayer && pPlayer->m_Moderating)
			return true;
	}
	return false;
}

void CGameContext::ForceVote(int EnforcerID, bool Success)
{
	// try to enforce vote in enforcer's room
	SGameInstance Instance = PlayerGameInstance(EnforcerID);
	if(Instance.m_IsCreated)
		Instance.m_pController->ForceVote(EnforcerID, Success);

	// check if there is a vote running
	if(!m_VoteCloseTime)
		return;

	m_VoteEnforce = Success ? VOTE_ENFORCE_YES_ADMIN : VOTE_ENFORCE_NO_ADMIN;
	m_VoteEnforcer = EnforcerID;

	char aBuf[256];
	const char *pOption = Success ? "yes" : "no";
	SendChatLocalized(-1, "authorized player forced vote %s", pOption);
	str_format(aBuf, sizeof(aBuf), "forcing vote %s", pOption);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

bool CGameContext::RateLimitPlayerVote(int ClientID)
{
	int64 Now = Server()->Tick();
	int64 TickSpeed = Server()->TickSpeed();
	CPlayer *pPlayer = m_apPlayers[ClientID];

	if(g_Config.m_SvRconVote && !Server()->GetAuthedState(ClientID))
	{
		SendChatLocalized(ClientID, "You can only vote after logging in.");
		return true;
	}

	if(g_Config.m_SvDnsblVote && Server()->DistinctClientCount() > 1)
	{
		if(m_pServer->DnsblPending(ClientID))
		{
			SendChatLocalized(ClientID, "You are not allowed to vote because we're currently checking for VPNs. Try again in ~30 seconds.");
			return true;
		}
		else if(m_pServer->DnsblBlack(ClientID))
		{
			SendChatLocalized(ClientID, "You are not allowed to vote because you appear to be using a VPN. Try connecting without a VPN or contacting an admin if you think this is a mistake.");
			return true;
		}
	}

	if(g_Config.m_SvSpamprotection && pPlayer->m_LastVoteTry && pPlayer->m_LastVoteTry + TickSpeed * 3 > Now)
		return true;

	pPlayer->m_LastVoteTry = Now;
	if(m_VoteCloseTime)
	{
		SendChatLocalized(ClientID, "Wait for current vote to end before calling a new one.");
		return true;
	}

	if(Now < pPlayer->m_FirstVoteTick)
	{
		SendChatLocalized(ClientID, "You must wait %d seconds before making your first vote.", (int)((pPlayer->m_FirstVoteTick - Now) / TickSpeed) + 1);
		return true;
	}

	int TimeLeft = pPlayer->m_LastVoteCall + TickSpeed * g_Config.m_SvVoteDelay - Now;
	if(pPlayer->m_LastVoteCall && TimeLeft > 0)
	{
		SendChatLocalized(ClientID, "You must wait %d seconds before making another vote.", (int)(TimeLeft / TickSpeed) + 1);
		return true;
	}

	NETADDR Addr;
	Server()->GetClientAddr(ClientID, &Addr);
	int VoteMuted = 0;
	for(int i = 0; i < m_NumVoteMutes && !VoteMuted; i++)
		if(!net_addr_comp_noport(&Addr, &m_aVoteMutes[i].m_Addr))
			VoteMuted = (m_aVoteMutes[i].m_Expire - Server()->Tick()) / Server()->TickSpeed();
	if(VoteMuted > 0)
	{
		SendChatLocalized(ClientID, "You are not permitted to vote for the next %d seconds.", VoteMuted);
		return true;
	}
	return false;
}

bool distCompare(std::pair<float, int> a, std::pair<float, int> b)
{
	return (a.first < b.first);
}

void CGameContext::UpdatePlayerMaps()
{
	if(Server()->Tick() % g_Config.m_SvMapUpdateRate != 0)
		return;

	std::pair<float, int> Dist[MAX_CLIENTS];
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!Server()->ClientIngame(i))
			continue;
		int *pMap = Server()->GetIdMap(i);

		// compute distances
		for(int j = 0; j < MAX_CLIENTS; j++)
		{
			Dist[j].second = j;
			if(!Server()->ClientIngame(j) || !m_apPlayers[j])
			{
				Dist[j].first = 1e10;
				continue;
			}
			CCharacter *ch = m_apPlayers[j]->GetCharacter();
			if(!ch)
			{
				Dist[j].first = 1e9;
				continue;
			}
			// copypasted chunk from character.cpp Snap() follows
			CCharacter *SnapChar = GetPlayerChar(i);
			if(SnapChar && !SnapChar->m_Super &&
				!m_apPlayers[i]->IsPaused() && m_apPlayers[i]->GetTeam() != -1 &&
				!ch->CanCollide(i) &&
				(!m_apPlayers[i] ||
					m_apPlayers[i]->GetClientVersion() == VERSION_VANILLA ||
					(m_apPlayers[i]->GetClientVersion() >= VERSION_DDRACE &&
						(m_apPlayers[i]->ShowOthersMode() == CPlayer::SHOWOTHERS_OFF))))
				Dist[j].first = 1e8;
			else
				Dist[j].first = 0;

			Dist[j].first += distance(m_apPlayers[i]->m_ViewPos, m_apPlayers[j]->GetCharacter()->m_Pos);
		}

		// always send the player himself
		Dist[i].first = 0;

		// compute reverse map
		int rMap[MAX_CLIENTS];
		for(int &j : rMap)
		{
			j = -1;
		}
		for(int j = 0; j < VANILLA_MAX_CLIENTS; j++)
		{
			if(pMap[j] == -1)
				continue;
			if(Dist[pMap[j]].first > 5e9)
				pMap[j] = -1;
			else
				rMap[pMap[j]] = j;
		}

		std::nth_element(&Dist[0], &Dist[VANILLA_MAX_CLIENTS - 1], &Dist[MAX_CLIENTS], distCompare);

		int Mapc = 0;
		int Demand = 0;
		for(int j = 0; j < VANILLA_MAX_CLIENTS - 1; j++)
		{
			int k = Dist[j].second;
			if(rMap[k] != -1 || Dist[j].first > 5e9)
				continue;
			while(Mapc < VANILLA_MAX_CLIENTS && pMap[Mapc] != -1)
				Mapc++;
			if(Mapc < VANILLA_MAX_CLIENTS - 1)
				pMap[Mapc] = k;
			else
				Demand++;
		}
		for(int j = MAX_CLIENTS - 1; j > VANILLA_MAX_CLIENTS - 2; j--)
		{
			int k = Dist[j].second;
			if(rMap[k] != -1 && Demand-- > 0)
				pMap[rMap[k]] = -1;
		}
		pMap[VANILLA_MAX_CLIENTS - 1] = -1; // player with empty name to say chat msgs
	}
}

void CGameContext::DoActivityCheck()
{
	if(Config()->m_SvInactiveKickTime == 0)
		return;

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
#ifdef CONF_DEBUG
		if(g_Config.m_DbgDummies)
		{
			if(i >= MAX_CLIENTS - g_Config.m_DbgDummies)
				break;
		}
#endif

		SGameInstance Instance = PlayerGameInstance(i);
		if(!Instance.m_Init)
			continue;

		if(IsClientPlayer(i) &&
			Server()->GetAuthedState(i) == AUTHED_NO && (m_apPlayers[i]->m_InactivityTickCounter > Config()->m_SvInactiveKickTime * Server()->TickSpeed() * 60))
		{
			if(m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS)
			{
				if(Config()->m_SvInactiveKickSpec && !Instance.m_pController->IsDisruptiveLeave(m_apPlayers[i]))
					Server()->Kick(i, "Kicked for inactivity");
			}
			else
			{
				switch(Config()->m_SvInactiveKick)
				{
				case 0:
				case 1:
				{
					// move player to spectator if it is not disruptive
					if(!Instance.m_pController->IsDisruptiveLeave(m_apPlayers[i]))
						Instance.m_pController->DoTeamChange(m_apPlayers[i], TEAM_SPECTATORS);
				}
				break;
				case 2:
				{
					// kick the player
					if(!Instance.m_pController->IsDisruptiveLeave(m_apPlayers[i]))
						Server()->Kick(i, "Kicked for inactivity");
				}
				}
			}
		}
	}
}

void CGameContext::SendClientInfo(int ClientID)
{
	CPlayer *pPlayer = m_apPlayers[ClientID];
	if(!pPlayer)
		return;

	protocol7::CNetMsg_Sv_ClientDrop Drop;
	Drop.m_ClientID = ClientID;
	Drop.m_pReason = "";
	Drop.m_Silent = true;

	protocol7::CNetMsg_Sv_ClientInfo Info;
	Info.m_ClientID = ClientID;

	Info.m_pName = Server()->ClientName(ClientID);
	Info.m_pClan = Server()->ClientClan(ClientID);
	Info.m_Flag = Server()->ClientFlag(ClientID);
	Info.m_Local = 0;
	Info.m_Silent = true;
	Info.m_Team = pPlayer->GetTeam();

	for(int p = 0; p < 6; p++)
	{
		Info.m_apSkinPartNames[p] = pPlayer->m_TeeInfos.m_apSkinPartNames[p];
		Info.m_aSkinPartColors[p] = pPlayer->m_TeeInfos.m_aSkinPartColors[p];
		Info.m_aUseCustomColors[p] = pPlayer->m_TeeInfos.m_aUseCustomColors[p];
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i != ClientID)
		{
			Server()->SendPackMsg(&Drop, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
			Server()->SendPackMsg(&Info, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
		}
	}
}

void CGameContext::SendSkinInfo(int ClientID)
{
	CPlayer *pPlayer = m_apPlayers[ClientID];
	if(!pPlayer)
		return;

	protocol7::CNetMsg_Sv_SkinChange Msg;
	Msg.m_ClientID = ClientID;
	for(int p = 0; p < 6; p++)
	{
		Msg.m_apSkinPartNames[p] = pPlayer->m_TeeInfos.m_apSkinPartNames[p];
		Msg.m_aSkinPartColors[p] = pPlayer->m_TeeInfos.m_aSkinPartColors[p];
		Msg.m_aUseCustomColors[p] = pPlayer->m_TeeInfos.m_aUseCustomColors[p];
	}

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, -1);
}
