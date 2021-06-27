/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "gamecontext.h"
#include <engine/engine.h>
#include <engine/shared/config.h>
#include <engine/shared/console.h>
#include <engine/shared/protocol.h>
#include <game/server/teams.h>
#include <game/version.h>

#include "entities/character.h"
#include "player.h"

bool CheckClientID(int ClientID);

void CGameContext::ConCredits(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		"DDNet is run by the DDNet staff (DDNet.tw/staff)");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		"Great maps and many ideas from the great community");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		"Help and code by eeeee, HMH, east, CookieMichal, Learath2,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		"Savander, laxa, Tobii, BeaR, Wohoo, nuborn, timakro, Shiki,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		"trml, Soreu, hi_leute_gll, Lady Saavik, Chairn, heinrich5991,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		"swick, oy, necropotame, Ryozuki, Redix, d3fault, marcelherd,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		"BannZay, ACTom, SiuFuWong, PathosEthosLogos, TsFreddie,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		"Jupeyy, noby, ChillerDragon, ZombieToad, weez15, z6zzz,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		"Piepow, QingGo, RafaelFF, sctt, jao, daverck, fokkonaut,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		"Bojidar, FallenKN, ardadem, archimede67, sirius1242, Aerll,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		"trafilaw, Zwelf, Patiga, Konsti, ElXreno, MikiGamer,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		"Fireball, Banana090, axblk, yangfl & others.");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		"Based on DDRace by the DDRace developers,");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		"which is a mod of Teeworlds by the Teeworlds developers.");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		"Special thanks to the TeeworldsCN PvP community for");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "credits",
		"making this happen: Rice, Texas.C, sirius1242");
}

void CGameContext::ConInfo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info",
		"DDNet PvP Mod. Version: " GAME_VERSION);
	if(GIT_SHORTREV_HASH)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "Git revision hash: %s", GIT_SHORTREV_HASH);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info", aBuf);
	}
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info",
		"Official site: DDNet.tw");
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "info",
		"For more info: /cmdlist");
}

void CGameContext::ConList(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int ClientID = pResult->m_ClientID;
	if(!CheckClientID(ClientID))
		return;

	char zerochar = 0;
	if(pResult->NumArguments() > 0)
		pSelf->List(ClientID, pResult->GetString(0));
	else
		pSelf->List(ClientID, &zerochar);
}

void CGameContext::ConHelp(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	if(pResult->NumArguments() == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help",
			"/cmdlist will show a list of all chat commands");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help",
			"/help + any command will show you the help for this command");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help",
			"/setting will show a list of all room commands");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help",
			"/setting help + any command will show the detail of a room command");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help",
			"Example /help emote will display the help about /emote");
	}
	else
	{
		const char *pArg = pResult->GetString(0);
		const IConsole::CCommandInfo *pCmdInfo =
			pSelf->Console()->GetCommandInfo(pArg, CFGFLAG_SERVER, false);
		if(pCmdInfo)
		{
			if(pCmdInfo->m_pParams)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "Usage: %s %s", pCmdInfo->m_pName, pCmdInfo->m_pParams);
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help", aBuf);
			}

			if(pCmdInfo->m_pHelp)
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "help", pCmdInfo->m_pHelp);
		}
		else
			pSelf->Console()->Print(
				IConsole::OUTPUT_LEVEL_STANDARD,
				"help",
				"Command is either unknown or you have given a blank command without any parameters.");
	}
}

void CGameContext::ConRules(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	bool Printed = false;
	if(g_Config.m_SvDDRaceRules)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "rules",
			"Be nice.");
		Printed = true;
	}
#define _RL(n) g_Config.m_SvRulesLine##n
	char *pRuleLines[] = {
		_RL(1),
		_RL(2),
		_RL(3),
		_RL(4),
		_RL(5),
		_RL(6),
		_RL(7),
		_RL(8),
		_RL(9),
		_RL(10),
	};
	for(auto &pRuleLine : pRuleLines)
	{
		if(pRuleLine[0])
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD,
				"rules", pRuleLine);
			Printed = true;
		}
	}
	if(!Printed)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "rules",
			"No Rules Defined, Kill em all!!");
	}
}

void ToggleSpecPause(IConsole::IResult *pResult, void *pUserData)
{
}

void CGameContext::ConTogglePause(IConsole::IResult *pResult, void *pUserData)
{
	if(!CheckClientID(pResult->m_ClientID))
		return;

	if(!g_Config.m_SvPauseable)
		return;

	CGameContext *pSelf = (CGameContext *)pUserData;
	IServer *pServ = pSelf->Server();
	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(!pPlayer)
		return;

	bool PlayerPaused = pPlayer->IsPaused();
	if(pResult->NumArguments() > 0)
	{
		if(PlayerPaused && pPlayer->GetSpectatorID() != pResult->m_ClientID && pServ->ClientIngame(pPlayer->GetSpectatorID()) && !str_comp(pServ->ClientName(pPlayer->GetSpectatorID()), pResult->GetString(0)))
		{
			pPlayer->Pause(false, false);
		}
		else
		{
			pPlayer->Pause(true, false);
			pPlayer->SpectatePlayerName(pResult->GetString(0));
		}
	}
	else if(PlayerPaused)
		pPlayer->Pause(false, false);
	else
		pPlayer->Pause(true, false);
}

void CGameContext::ConDND(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(!pPlayer)
		return;

	if(pPlayer->m_DND)
	{
		pPlayer->m_DND = false;
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "dnd", "You will receive global chat and server messages");
	}
	else
	{
		pPlayer->m_DND = true;
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "dnd", "You will not receive any further global chat and server messages");
	}
}

void CGameContext::ConTimeout(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(!pPlayer)
		return;

	const char *pTimeout = pResult->NumArguments() > 0 ? pResult->GetString(0) : pPlayer->m_TimeoutCode;

	if(!pSelf->Server()->IsSixup(pResult->m_ClientID))
	{
		for(int i = 0; i < pSelf->Server()->MaxClients(); i++)
		{
			if(i == pResult->m_ClientID)
				continue;
			if(!pSelf->m_apPlayers[i])
				continue;
			if(str_comp(pSelf->m_apPlayers[i]->m_TimeoutCode, pTimeout))
				continue;
			if(pSelf->Server()->SetTimedOut(i, pResult->m_ClientID))
			{
				if(pSelf->m_apPlayers[i] && pSelf->m_apPlayers[i]->GetCharacter())
					pSelf->SendTuningParams(i, pSelf->m_apPlayers[i]->GetCharacter()->m_TuneZone);
				/*if(pSelf->Server()->IsSixup(i))
					pSelf->SendClientInfo(i, i);*/
				return;
			}
		}
	}
	else
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "print",
			"Your timeout code has been set. 0.7 clients can not reclaim their tees on timeout; however, a 0.6 client can claim your tee ");
	}

	pSelf->Server()->SetTimeoutProtected(pResult->m_ClientID);
	str_copy(pPlayer->m_TimeoutCode, pResult->GetString(0), sizeof(pPlayer->m_TimeoutCode));
}

void CGameContext::ConLockTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID))
		return;

	if(g_Config.m_SvRoom == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "lock",
			"Rooms are disabled");
		return;
	}

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(!pPlayer)
		return;
	if(pPlayer->m_LastRoomInfoChange + (int64)pSelf->Server()->TickSpeed() * g_Config.m_SvRoomChangeDelay > pSelf->Server()->Tick())
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
			"You can\'t modify your room that fast!");
		return;
	}

	int Team = pSelf->GetPlayerDDRTeam(pResult->m_ClientID);

	bool Lock = pSelf->Teams()->TeamLocked(Team);

	if(pResult->NumArguments() > 0)
		Lock = !pResult->GetInteger(0);

	if(Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"print",
			"This room can't be locked");
		return;
	}

	if(pSelf->ProcessSpamProtection(pResult->m_ClientID))
		return;

	pPlayer->m_LastRoomInfoChange = pSelf->Server()->Tick();
	char aBuf[512];
	if(Lock)
	{
		pSelf->UnlockTeam(pResult->m_ClientID, Team);
	}
	else
	{
		if(g_Config.m_SvLock == 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "lock",
				"Lock is disabled");
			return;
		}

		pSelf->Teams()->SetTeamLock(Team, true);

		str_format(aBuf, sizeof(aBuf), "'%s' locked your room.", pSelf->Server()->ClientName(pResult->m_ClientID));

		for(int i = 0; i < MAX_CLIENTS; i++)
			if(pSelf->GetPlayerDDRTeam(i) == Team)
				pSelf->SendChatTarget(i, aBuf);
	}
}

void CGameContext::ConUnlockTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID))
		return;

	if(g_Config.m_SvRoom == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "unlock",
			"Rooms are disabled");
		return;
	}

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(!pPlayer)
		return;
	if(pPlayer->m_LastRoomInfoChange + (int64)pSelf->Server()->TickSpeed() * g_Config.m_SvRoomChangeDelay > pSelf->Server()->Tick())
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
			"You can\'t modify your room that fast!");
		return;
	}

	int Team = pSelf->GetPlayerDDRTeam(pResult->m_ClientID);

	if(Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
		return;

	if(pSelf->ProcessSpamProtection(pResult->m_ClientID))
		return;

	pSelf->UnlockTeam(pResult->m_ClientID, Team);
}

void CGameContext::UnlockTeam(int ClientID, int Team)
{
	Teams()->SetTeamLock(Team, false);

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "'%s' unlocked your team.", Server()->ClientName(ClientID));

	for(int i = 0; i < MAX_CLIENTS; i++)
		if(GetPlayerDDRTeam(i) == Team)
			SendChatTarget(i, aBuf);
}

void CGameContext::ConInviteTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	const char *pName = pResult->GetString(0);

	if(g_Config.m_SvRoom == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
			"Rooms are disabled");
		return;
	}

	if(!g_Config.m_SvInvite)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "invite", "Invites are disabled");
		return;
	}

	int Team = pSelf->GetPlayerDDRTeam(pResult->m_ClientID);
	if(Team > TEAM_FLOCK && Team < TEAM_SUPER)
	{
		int Target = -1;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!str_comp(pName, pSelf->Server()->ClientName(i)))
			{
				Target = i;
				break;
			}
		}

		if(Target < 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "invite", "Player not found");
			return;
		}

		if(pSelf->Teams()->IsInvited(Team, Target))
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "invite", "Player already invited");
			return;
		}

		if(pSelf->m_apPlayers[pResult->m_ClientID] && pSelf->m_apPlayers[pResult->m_ClientID]->m_LastInvited + g_Config.m_SvInviteFrequency * pSelf->Server()->TickSpeed() > pSelf->Server()->Tick())
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "invite", "Can't invite this quickly");
			return;
		}

		pSelf->Teams()->SetClientInvited(Team, Target, true);
		pSelf->m_apPlayers[pResult->m_ClientID]->m_LastInvited = pSelf->Server()->Tick();

		char aBuf[512];
		str_format(aBuf, sizeof aBuf, "'%s' invited you to team %d.", pSelf->Server()->ClientName(pResult->m_ClientID), Team);
		pSelf->SendChatTarget(Target, aBuf);

		str_format(aBuf, sizeof aBuf, "'%s' invited '%s' to your team.", pSelf->Server()->ClientName(pResult->m_ClientID), pSelf->Server()->ClientName(Target));
		;
		for(int i = 0; i < MAX_CLIENTS; i++)
			if(pSelf->GetPlayerDDRTeam(i) == Team)
				pSelf->SendChatTarget(i, aBuf);
	}
	else
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "invite", "Can't invite players to this team");
}

void CGameContext::ConReady(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(!pPlayer)
		return;
	pSelf->ChangePlayerReadyState(pPlayer);
}

void CGameContext::ConJoinTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(!pPlayer)
		return;

	if(pSelf->m_VoteCloseTime && pSelf->m_VoteCreator == pResult->m_ClientID && (pSelf->IsKickVote() || pSelf->IsSpecVote()))
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"join",
			"You are running a vote please try again after the vote is done!");
		return;
	}
	else if(g_Config.m_SvRoom == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
			"Rooms are disabled");
		return;
	}
	else if(g_Config.m_SvRoomCommands == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
			"Room commands are disabled");
		return;
	}

	int NumArgs = pResult->NumArguments();

	if(NumArgs == 0)
	{
		char aBuf[512];
		str_format(
			aBuf,
			sizeof(aBuf),
			"You are in room %d",
			pSelf->GetPlayerDDRTeam(pResult->m_ClientID));
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
			aBuf);
		return;
	}

	int Team = pResult->GetInteger(0);
	int RespondingID = pSelf->m_ChatResponseTargetID;
	pSelf->m_ChatResponseTargetID = -1;
	if(pPlayer->m_LastRoomChange + (int64)pSelf->Server()->TickSpeed() * g_Config.m_SvRoomChangeDelay > pSelf->Server()->Tick())
	{
		pSelf->m_ChatResponseTargetID = RespondingID;
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
			"You can't change rooms that fast!");
	}
	else if(!pSelf->Teams()->CanSwitchTeam(pPlayer->GetCID()))
	{
		pSelf->m_ChatResponseTargetID = RespondingID;
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
			"You can't change rooms at this time.");
	}
	else if(Team > 0 && Team < MAX_CLIENTS && pSelf->Teams()->TeamLocked(Team) && !pSelf->Teams()->IsInvited(Team, pResult->m_ClientID))
	{
		pSelf->m_ChatResponseTargetID = RespondingID;
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
			g_Config.m_SvInvite ?
                                "This room is locked using /lock. Only members of the room can unlock it using /lock." :
                                "This room is locked using /lock. Only members of the room can invite you or unlock it using /lock.");
	}
	else if(const char *pError = pSelf->Teams()->SetPlayerTeam(pPlayer->GetCID(), Team, nullptr))
	{
		pSelf->m_ChatResponseTargetID = RespondingID;
		// allow join to create room and not limited by create room delay, in case there are only 1 gametype
		// if there are only 1 gametype, people won't get confused trying to create many rooms anyway
		// since it will work almost exactly like /team
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join", pError);
	}
	else
	{
		pPlayer->m_LastRoomChange = pSelf->Server()->Tick();
	}
}

void CGameContext::ConCreateTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(!pPlayer)
		return;

	if(pSelf->m_VoteCloseTime && pSelf->m_VoteCreator == pResult->m_ClientID && (pSelf->IsKickVote() || pSelf->IsSpecVote()))
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"join",
			"You are running a vote please try again after the vote is done!");
		return;
	}
	else if(g_Config.m_SvRoom == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
			"Rooms are disabled");
		return;
	}
	else if(g_Config.m_SvRoomCommands == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
			"Room commands are disabled");
		return;
	}

	int Team = pSelf->Teams()->FindAEmptyTeam();
	if(Team < 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
			"There aren't any rooms available!");
	}
	const char *pGameType = pResult->NumArguments() > 0 ? pResult->GetString(0) : nullptr;
	int RespondingID = pSelf->m_ChatResponseTargetID;
	pSelf->m_ChatResponseTargetID = -1;

	if(pPlayer->m_LastRoomChange + (int64)pSelf->Server()->TickSpeed() * g_Config.m_SvRoomChangeDelay > pSelf->Server()->Tick())
	{
		pSelf->m_ChatResponseTargetID = RespondingID;
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
			"You can't change rooms that fast!");
	}
	else if(!pSelf->Teams()->CanSwitchTeam(pPlayer->GetCID()))
	{
		pSelf->m_ChatResponseTargetID = RespondingID;
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
			"You can't change rooms at this time.");
	}
	else if(Team > 0 && Team < MAX_CLIENTS && pSelf->Teams()->TeamLocked(Team) && !pSelf->Teams()->IsInvited(Team, pResult->m_ClientID))
	{
		pSelf->m_ChatResponseTargetID = RespondingID;
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join",
			g_Config.m_SvInvite ?
                                "This room is locked using /lock. Only members of the room can unlock it using /lock." :
                                "This room is locked using /lock. Only members of the room can invite you or unlock it using /lock.");
	}
	else if(pPlayer->m_LastRoomCreation + (int64)pSelf->Server()->TickSpeed() * g_Config.m_SvRoomCreateDelay > pSelf->Server()->Tick())
	{
		pSelf->m_ChatResponseTargetID = RespondingID;
		char aBuf[128];
		int Seconds = ((pPlayer->m_LastRoomCreation + (int64)pSelf->Server()->TickSpeed() * g_Config.m_SvRoomCreateDelay) - pSelf->Server()->Tick()) / pSelf->Server()->TickSpeed();
		str_format(aBuf, sizeof(aBuf), "You have to wait %d seconds before creating a new room.", Seconds + 1);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join", aBuf);
	}
	else if(const char *pError = pSelf->Teams()->SetPlayerTeam(pPlayer->GetCID(), Team, pGameType))
	{
		pSelf->m_ChatResponseTargetID = RespondingID;
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "join", pError);
	}
	else
	{
		pPlayer->m_LastRoomChange = pSelf->Server()->Tick();
		pPlayer->m_LastRoomCreation = pSelf->Server()->Tick();
	}
}

void CGameContext::ConMe(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID))
		return;

	char aBuf[256 + 24];

	str_format(aBuf, 256 + 24, "'%s': %s",
		pSelf->Server()->ClientName(pResult->m_ClientID),
		pResult->GetString(0));
	if(g_Config.m_SvSlashMe)
		pSelf->SendChat(-2, CGameContext::CHAT_ALL, aBuf, pResult->m_ClientID);
	else
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"me",
			"/me is disabled on this server");
}

void CGameContext::ConConverse(IConsole::IResult *pResult, void *pUserData)
{
	// This will never be called
}

void CGameContext::ConWhisper(IConsole::IResult *pResult, void *pUserData)
{
	// This will never be called
}

void CGameContext::ConSetEyeEmote(IConsole::IResult *pResult,
	void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(!pPlayer)
		return;
	if(pResult->NumArguments() == 0)
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"emote",
			(pPlayer->m_EyeEmoteEnabled) ?
                                "You can now use the preset eye emotes." :
                                "You don't have any eye emotes, remember to bind some. (until you die)");
		return;
	}
	else if(str_comp_nocase(pResult->GetString(0), "on") == 0)
		pPlayer->m_EyeEmoteEnabled = true;
	else if(str_comp_nocase(pResult->GetString(0), "off") == 0)
		pPlayer->m_EyeEmoteEnabled = false;
	else if(str_comp_nocase(pResult->GetString(0), "toggle") == 0)
		pPlayer->m_EyeEmoteEnabled = !pPlayer->m_EyeEmoteEnabled;
	pSelf->Console()->Print(
		IConsole::OUTPUT_LEVEL_STANDARD,
		"emote",
		(pPlayer->m_EyeEmoteEnabled) ?
                        "You can now use the preset eye emotes." :
                        "You don't have any eye emotes, remember to bind some. (until you die)");
}

void CGameContext::ConEyeEmote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(g_Config.m_SvEmotionalTees == -1)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "emote",
			"Emotes are disabled.");
		return;
	}

	if(!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(!pPlayer)
		return;

	if(pResult->NumArguments() == 0)
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"emote",
			"Emote commands are: /emote surprise /emote blink /emote close /emote angry /emote happy /emote pain");
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"emote",
			"Example: /emote surprise 10 for 10 seconds or /emote surprise (default 1 second)");
	}
	else
	{
		if(!pPlayer->CanOverrideDefaultEmote())
			return;

		int EmoteType = 0;
		if(!str_comp(pResult->GetString(0), "angry"))
			EmoteType = EMOTE_ANGRY;
		else if(!str_comp(pResult->GetString(0), "blink"))
			EmoteType = EMOTE_BLINK;
		else if(!str_comp(pResult->GetString(0), "close"))
			EmoteType = EMOTE_BLINK;
		else if(!str_comp(pResult->GetString(0), "happy"))
			EmoteType = EMOTE_HAPPY;
		else if(!str_comp(pResult->GetString(0), "pain"))
			EmoteType = EMOTE_PAIN;
		else if(!str_comp(pResult->GetString(0), "surprise"))
			EmoteType = EMOTE_SURPRISE;
		else if(!str_comp(pResult->GetString(0), "normal"))
			EmoteType = EMOTE_NORMAL;
		else
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD,
				"emote", "Unknown emote... Say /emote");
			return;
		}

		int Duration = 1;
		if(pResult->NumArguments() > 1)
			Duration = clamp(pResult->GetInteger(1), 1, 86400);

		pPlayer->OverrideDefaultEmote(EmoteType, pSelf->Server()->Tick() + Duration * pSelf->Server()->TickSpeed());
	}
}

void CGameContext::ConShowOthers(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!CheckClientID(pResult->m_ClientID))
		return;

	CPlayer *pPlayer = pSelf->m_apPlayers[pResult->m_ClientID];
	if(!pPlayer)
		return;
	if(g_Config.m_SvShowOthers)
	{
		if(pResult->NumArguments())
			pPlayer->m_ShowOthers = pResult->GetInteger(0);
		else
		{
			if(g_Config.m_SvShowOthersDefault == 2)
				pPlayer->m_ShowOthers = pPlayer->m_ShowOthers == 1 ? g_Config.m_SvShowOthersDefault : 1;
			else
				pPlayer->m_ShowOthers = !pPlayer->m_ShowOthers;
		}

		pPlayer->m_SpecTeam = pPlayer->m_ShowOthers > 0;
	}
	else
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			"showotherschat",
			"Showing players from other teams is disabled");
}

void CGameContext::ConInstanceCommand(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int ClientID = pResult->m_ClientID;
	if(!CheckClientID(ClientID))
		return;

	int Room = pSelf->GetPlayerDDRTeam(ClientID);
	char aInstanceBuf[32];
	str_format(aInstanceBuf, sizeof(aInstanceBuf), "instance %d", Room);

	int Auth = pSelf->Server()->GetAuthedState(ClientID);

	if(g_Config.m_SvRoomSetting == 0 && Auth < AUTHED_MOD)
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			aInstanceBuf,
			"/setting is disabled");
		return;
	}

	SGameInstance Instance = pSelf->PlayerGameInstance(ClientID);
	if(!Instance.m_Init)
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			aInstanceBuf,
			"The room is not ready");
		return;
	}

	if(Instance.m_pWorld->Team() == 0 && Auth < AUTHED_MOD)
	{
		pSelf->Console()->Print(
			IConsole::OUTPUT_LEVEL_STANDARD,
			aInstanceBuf,
			"You can't execute command in lobby room / room 0");
		return;
	}

	if(pResult->NumArguments())
	{
		const char *pLine = pResult->GetString(0);
		char aBuf[256];
		Instance.m_pController->InstanceConsole()->SetFlagMask(CFGFLAG_CHAT);

		const char *pArgs = str_find(pLine, " ");
		char aCommand[128];
		if(pArgs)
			str_copy(aCommand, pLine, minimum((int)sizeof(aCommand), (int)(pArgs - pLine + 1)));
		else
			str_copy(aCommand, pLine, sizeof(aCommand));

		const IConsole::CCommandInfo *pCmdInfo =
			Instance.m_pController->InstanceConsole()->GetCommandInfo(aCommand, CFGFLAG_CHAT, false);

		if(!pCmdInfo)
		{
			str_format(aBuf, sizeof(aBuf), "The command '%s' does not exist", aCommand);
			Instance.m_pController->SendChatTarget(ClientID, aBuf);
			return;
		}

		if(pCmdInfo->m_Flags & CFGFLAG_NO_CONSENT)
			Instance.m_pController->InstanceConsole()->ExecuteLine(pLine, ClientID, false);
		else
		{
			if(g_Config.m_SvRoomSetting != 1 && Auth < AUTHED_MOD)
			{
				if(pSelf->m_VoteCloseTime > 0)
				{
					str_format(aBuf, sizeof(aBuf), "You can't call a setting vote now, because a server vote is in progress.");
					pSelf->Console()->Print(
						IConsole::OUTPUT_LEVEL_STANDARD,
						aInstanceBuf,
						"You can't execute command in lobby room / room 0");
				}
				else
				{
					str_format(aBuf, sizeof(aBuf), "'%s' called vote to change room setting '%s'", pSelf->Server()->ClientName(ClientID), pLine);
					Instance.m_pController->CallVote(ClientID, pLine, pLine, "/setting", aBuf, pLine);
				}
			}
			else
			{
				str_format(aBuf, sizeof(aBuf), "'%s' called '%s'", pSelf->Server()->ClientName(ClientID), pLine);
				Instance.m_pController->SendChatTarget(-1, aBuf);
				Instance.m_pController->InstanceConsole()->ExecuteLine(pLine, ClientID, false);
			}
		}
	}
	else
	{
		Instance.m_pController->InstanceConsole()->ExecuteLine("cmdlist");
	}
}

bool CheckClientID(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return false;
	return true;
}
