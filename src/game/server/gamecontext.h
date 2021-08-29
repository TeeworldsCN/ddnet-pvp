/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTEXT_H
#define GAME_SERVER_GAMECONTEXT_H

#include <engine/antibot.h>
#include <engine/console.h>
#include <engine/server.h>

#include <game/layers.h>
#include <game/server/teams.h>
// #include <game/server/weapons.h>
#include <game/localization.h>
#include <game/voting.h>

#include <base/tl/array.h>
#include <base/tl/string.h>

#include "gamecontroller.h"
#include "gameworld.h"

#include <memory>
#include <stdarg.h>

/*
	Tick
		Game Context (CGameContext::tick)
			Game World (GAMEWORLD::tick)
				Reset world if requested (GAMEWORLD::reset)
				All entities in the world (ENTITY::tick)
				All entities in the world (ENTITY::tick_defered)
				Remove entities marked for deletion (GAMEWORLD::remove_entities)
			Game Controller (GAMECONTROLLER::tick)
			All players (CPlayer::tick)


	Snap
		Game Context (CGameContext::snap)
			Game World (GAMEWORLD::snap)
				All entities in the world (ENTITY::snap)
			Game Controller (GAMECONTROLLER::snap)
			Events handler (EVENT_HANDLER::snap)
			All players (CPlayer::snap)

*/

enum
{
	NUM_TUNEZONES = 256
};

enum
{
	GAMEMSG_TEAM_SWAP = 0,
	GAMEMSG_SPEC_INVALIDID,
	GAMEMSG_TEAM_SHUFFLE,
	GAMEMSG_TEAM_BALANCE,
	GAMEMSG_CTF_DROP,
	GAMEMSG_CTF_RETURN,
	GAMEMSG_TEAM_ALL,
	GAMEMSG_TEAM_BALANCE_VICTIM,
	GAMEMSG_CTF_GRAB,
	GAMEMSG_CTF_CAPTURE,
	GAMEMSG_GAME_PAUSED
};

class CConfig;
class CHeap;
class CPlayer;
class IConsole;
class IGameController;
class IEngine;
class IStorage;
class CGameTeams;
struct CAntibotData;
struct SGameInstance;

class CGameContext : public IGameServer
{
	IServer *m_pServer;
	CConfig *m_pConfig;
	IConsole *m_pConsole;
	IEngine *m_pEngine;
	IStorage *m_pStorage;
	IAntibot *m_pAntibot;
	CLayers m_Layers;
	CCollision m_Collision;
	CGameTeams m_Teams;
	protocol7::CNetObjHandler m_NetObjHandler7;
	CNetObjHandler m_NetObjHandler;
	CTuningParams m_Tuning;
	CTuningParams m_aTuningList[NUM_TUNEZONES];
	array<string> m_aCensorlist;

	CUuid m_GameUuid;
	CPrng m_Prng;

	static void ConTuneParam(IConsole::IResult *pResult, void *pUserData);
	static void ConToggleTuneParam(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneReset(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneDump(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneZone(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneDumpZone(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneResetZone(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneSetZoneMsgEnter(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneSetZoneMsgLeave(IConsole::IResult *pResult, void *pUserData);
	static void ConSwitchOpen(IConsole::IResult *pResult, void *pUserData);
	static void ConChangeMap(IConsole::IResult *pResult, void *pUserData);
	static void ConRestart(IConsole::IResult *pResult, void *pUserData);
	static void ConBroadcast(IConsole::IResult *pResult, void *pUserData);
	static void ConSay(IConsole::IResult *pResult, void *pUserData);
	static void ConSetTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConSetTeamAll(IConsole::IResult *pResult, void *pUserData);
	static void ConAddVote(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveVote(IConsole::IResult *pResult, void *pUserData);
	static void ConForceVote(IConsole::IResult *pResult, void *pUserData);
	static void ConClearVotes(IConsole::IResult *pResult, void *pUserData);
	static void ConVote(IConsole::IResult *pResult, void *pUserData);
	static void ConVoteNo(IConsole::IResult *pResult, void *pUserData);
	static void ConDumpAntibot(IConsole::IResult *pResult, void *pUserData);
	static void ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainUpdateRoomVotes(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	// pvp commands
	static void ConClearGameTypes(IConsole::IResult *pResult, void *pUserData);
	static void ConSetDefaultGameType(IConsole::IResult *pResult, void *pUserData);
	static void ConSetDefaultGameTypeFile(IConsole::IResult *pResult, void *pUserData);
	static void ConAddGameType(IConsole::IResult *pResult, void *pUserData);
	static void ConAddGameTypeFile(IConsole::IResult *pResult, void *pUserData);
	static void ConAddMapName(IConsole::IResult *pResult, void *pUserData);
	static void ConRoomSetting(IConsole::IResult *pResult, void *pUserData);

	CGameContext(int Resetting);
	void Construct(int Resetting);
	void AddVote(const char *pDescription, const char *pCommand);
	void LoadLanguageFiles();

	bool m_Resetting;

	struct CPersistentClientData
	{
		bool m_IsSpectator;
	};

public:
	IServer *Server() const { return m_pServer; }
	CConfig *Config() { return m_pConfig; }
	IConsole *Console() { return m_pConsole; }
	IEngine *Engine() { return m_pEngine; }
	IStorage *Storage() { return m_pStorage; }
	CCollision *Collision() { return &m_Collision; }
	CGameTeams *Teams() { return &m_Teams; }
	CTuningParams *Tuning() { return &m_Tuning; }
	CTuningParams *TuningList() { return &m_aTuningList[0]; }
	IAntibot *Antibot() { return m_pAntibot; }

	CGameContext();
	~CGameContext();

	void Clear();

	CPlayer *m_apPlayers[MAX_CLIENTS];

	// helper functions
	class CCharacter *GetPlayerChar(int ClientID);
	SGameInstance GameInstance(int Team);
	SGameInstance PlayerGameInstance(int ClientID);
	int GetPlayerDDRTeam(int ClientID);
	bool ChangePlayerReadyState(CPlayer *pPlayer);

	// voting
	void StartVote(const char *pDesc, const char *pCommand, const char *pReason, const char *pSixupDesc);
	void EndVote();
	bool IsVoting();
	void SendVoteSet(int ClientID);
	void SendVoteStatus(int ClientID, int Total, int Yes, int No);

	int m_VoteCreator;
	int m_VoteType;
	int64 m_VoteCloseTime;
	bool m_VoteUpdate;
	int m_VotePos;
	char m_aVoteDescription[VOTE_DESC_LENGTH];
	char m_aSixupVoteDescription[VOTE_DESC_LENGTH];
	char m_aVoteCommand[VOTE_CMD_LENGTH];
	char m_aVoteReason[VOTE_REASON_LENGTH];
	int m_NumVoteOptions;
	int m_VoteEnforce;
	char m_aaZoneEnterMsg[NUM_TUNEZONES][256]; // 0 is used for switching from or to area without tunings
	char m_aaZoneLeaveMsg[NUM_TUNEZONES][256];

	char m_aDeleteTempfile[128];
	void DeleteTempfile();

	CHeap *m_pVoteOptionHeap;
	CVoteOptionServer *m_pVoteOptionFirst;
	CVoteOptionServer *m_pVoteOptionLast;

	enum
	{
		CHAT_ALL = -2,
		CHAT_SPEC = -1,
		CHAT_RED = 0,
		CHAT_BLUE = 1,
		CHAT_WHISPER_SEND = 2,
		CHAT_WHISPER_RECV = 3,

		CHAT_SIX = 1 << 0,
		CHAT_SIXUP = 1 << 1,
		CHAT_SIXNUP = CHAT_SIX | CHAT_SIXUP,
	};

	// localize
	char m_FlagLangMap[1024];
	void UpdatePlayerLang(int ClientID, int Lang, bool IsInfoUpdate);
	struct ContextualString
	{
		const char *m_pFormat;
		const char *m_pContext;
	};
	const char *LocalizeFor(int ClientID, const char *pString, const char *pContext = "");
	void SendChatLocalizedVL(int To, int Flags, ContextualString String, va_list ap);
	void SendChatLocalized(int To, int Flags, ContextualString String, ...)
	{
		va_list ap;
		va_start(ap, String);
		SendChatLocalizedVL(To, Flags, String, ap);
		va_end(ap);
	}
	void SendChatLocalized(int To, ContextualString String, ...)
	{
		va_list ap;
		va_start(ap, String);
		SendChatLocalizedVL(To, CHAT_SIXNUP, {String.m_pFormat, String.m_pContext}, ap);
		va_end(ap);
	};
	void SendChatLocalized(int To, int Flags, const char *pFormat, ...)
	{
		va_list ap;
		va_start(ap, pFormat);
		SendChatLocalizedVL(To, Flags, {pFormat, ""}, ap);
		va_end(ap);
	};
	void SendChatLocalized(int To, const char *pFormat, ...)
	{
		va_list ap;
		va_start(ap, pFormat);
		SendChatLocalizedVL(To, CHAT_SIXNUP, {pFormat, ""}, ap);
		va_end(ap);
	};

	// network
	void CallVote(int ClientID, const char *aDesc, const char *aCmd, const char *pReason, const char *aChatmsg, const char *pSixupDesc = 0);
	void SendChatTarget(int To, const char *pText, int Flags = CHAT_SIX | CHAT_SIXUP);
	void SendChat(int ClientID, int Team, const char *pText, int SpamProtectionClientID = -1, int Flags = CHAT_SIX | CHAT_SIXUP);
	void SendEmoticon(int ClientID, int Emoticon);
	void SendWeaponPickup(int ClientID, int Weapon);
	void SendMotd(int ClientID);
	void SendSettings(int ClientID);
	void SendBroadcast(const char *pText, int ClientID, bool IsImportant = true);
	void SendBroadcastLocalizedVL(int ClientID, int line, bool IsImportant, ContextualString String, va_list ap); 
	void SendBroadcastLocalized(int ClientID, int Line, bool IsImportant, ContextualString String, ...)
	{
		va_list ap;
		va_start(ap, String);
		SendBroadcastLocalizedVL(ClientID, Line, IsImportant, String, ap);
		va_end(ap);
	}
	void SendBroadcastLocalized(int ClientID, int Line, bool IsImportant, const char *pFormat, ...)
	{
		va_list ap;
		va_start(ap, pFormat);
		SendBroadcastLocalizedVL(ClientID, Line, IsImportant, {pFormat, ""} , ap);
		va_end(ap);
	}

	void SendCurrentGameInfo(int ClientID, bool IsJoin);

	void List(int ClientID, const char *filter);

	//
	void SendTuningParams(int ClientID, int Zone = 0);

	struct CVoteOptionServer *GetVoteOption(int Index);
	void ProgressVoteOptions(int ClientID);

	//
	void LoadMapSettings();

	// engine events
	virtual void OnInit();
	virtual void OnConsoleInit();
	virtual void OnShutdown(bool FullShutdown);

	virtual void OnTick();
	virtual void OnPreSnap();
	virtual void OnSnap(int ClientID);
	virtual void OnPostSnap();

	void *PreProcessMsg(int *MsgID, CUnpacker *pUnpacker, int ClientID);
	void CensorMessage(char *pCensoredMessage, const char *pMessage, int Size);
	virtual void OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID);

	virtual bool OnClientDataPersist(int ClientID, void *pData);
	virtual void OnClientConnected(int ClientID, void *pData);
	virtual void OnClientEnter(int ClientID);
	virtual void OnClientDrop(int ClientID, const char *pReason);
	virtual void OnClientDirectInput(int ClientID, void *pInput);
	virtual void OnClientPredictedInput(int ClientID, void *pInput);
	virtual void OnClientPredictedEarlyInput(int ClientID, void *pInput);

	virtual void OnClientEngineJoin(int ClientID, bool Sixup);
	virtual void OnClientEngineDrop(int ClientID, const char *pReason);

	virtual bool IsClientReadyToEnter(int ClientID) const;
	virtual bool IsClientPlayer(int ClientID) const;
	virtual bool IsClientActivePlayer(int ClientID) const;

	virtual bool CheckDisruptiveLeave(int ClientID);
	virtual int PersistentClientDataSize() const { return sizeof(CPersistentClientData); }

	virtual CUuid GameUuid() const;
	virtual const char *GameType() const;
	virtual const char *Version() const;
	virtual const char *NetVersion() const;

	// DDRace
	bool OnClientDDNetVersionKnown(int ClientID);
	virtual void FillAntibot(CAntibotRoundData *pData);
	int ProcessSpamProtection(int ClientID);
	int GetDDRaceTeam(int ClientID);
	// Describes the time when the first player joined the server.
	int64 m_NonEmptySince;
	int GetClientVersion(int ClientID) const;
	bool PlayerExists(int ClientID) const { return m_apPlayers[ClientID]; }
	// Returns true if someone is actively moderating.
	bool PlayerModerating() const;
	void ForceVote(int EnforcerID, bool Success);

	// Checks if player can vote and notify them about the reason
	bool RateLimitPlayerVote(int ClientID);

	void UpdatePlayerMaps();
	void DoActivityCheck();

	void SendClientInfo(int ClientID);
	void SendSkinInfo(int ClientID);

	// languages
	void LoadLanguages();
	void UnloadLanguages();

private:
	bool m_VoteWillPass;

	//DDRace Console Commands

	static void ConKillPlayer(IConsole::IResult *pResult, void *pUserData);

	static void ConNinja(IConsole::IResult *pResult, void *pUserData);
	static void ConEndlessHook(IConsole::IResult *pResult, void *pUserData);
	static void ConUnEndlessHook(IConsole::IResult *pResult, void *pUserData);
	static void ConUnSolo(IConsole::IResult *pResult, void *pUserData);
	static void ConUnDeep(IConsole::IResult *pResult, void *pUserData);
	static void ConUnSuper(IConsole::IResult *pResult, void *pUserData);
	static void ConSuper(IConsole::IResult *pResult, void *pUserData);
	static void ConShotgun(IConsole::IResult *pResult, void *pUserData);
	static void ConGrenade(IConsole::IResult *pResult, void *pUserData);
	static void ConLaser(IConsole::IResult *pResult, void *pUserData);
	static void ConJetpack(IConsole::IResult *pResult, void *pUserData);
	static void ConWeapons(IConsole::IResult *pResult, void *pUserData);
	static void ConUnShotgun(IConsole::IResult *pResult, void *pUserData);
	static void ConUnGrenade(IConsole::IResult *pResult, void *pUserData);
	static void ConUnLaser(IConsole::IResult *pResult, void *pUserData);
	static void ConUnJetpack(IConsole::IResult *pResult, void *pUserData);
	static void ConUnWeapons(IConsole::IResult *pResult, void *pUserData);
	static void ConAddWeapon(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveWeapon(IConsole::IResult *pResult, void *pUserData);

	void MoveCharacter(int ClientID, int X, int Y, bool Raw = false);
	static void ConGoLeft(IConsole::IResult *pResult, void *pUserData);
	static void ConGoRight(IConsole::IResult *pResult, void *pUserData);
	static void ConGoUp(IConsole::IResult *pResult, void *pUserData);
	static void ConGoDown(IConsole::IResult *pResult, void *pUserData);
	static void ConMove(IConsole::IResult *pResult, void *pUserData);
	static void ConMoveRaw(IConsole::IResult *pResult, void *pUserData);

	static void ConToTeleporter(IConsole::IResult *pResult, void *pUserData);
	static void ConToCheckTeleporter(IConsole::IResult *pResult, void *pUserData);
	static void ConTeleport(IConsole::IResult *pResult, void *pUserData);

	static void ConCredits(IConsole::IResult *pResult, void *pUserData);
	static void ConInfo(IConsole::IResult *pResult, void *pUserData);
	static void ConHelp(IConsole::IResult *pResult, void *pUserData);
	static void ConRules(IConsole::IResult *pResult, void *pUserData);
	static void ConTogglePause(IConsole::IResult *pResult, void *pUserData);
	static void ConUTF8(IConsole::IResult *pResult, void *pUserData);
	static void ConDND(IConsole::IResult *pResult, void *pUserData);
	static void ConTimeout(IConsole::IResult *pResult, void *pUserData);
	static void ConBroadTime(IConsole::IResult *pResult, void *pUserData);
	static void ConJoinOrCreateTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConJoinTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConCreateTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConLockTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConUnlockTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConInviteTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConMe(IConsole::IResult *pResult, void *pUserData);
	static void ConWhisper(IConsole::IResult *pResult, void *pUserData);
	static void ConConverse(IConsole::IResult *pResult, void *pUserData);
	static void ConSetEyeEmote(IConsole::IResult *pResult, void *pUserData);
	static void ConToggleBroadcast(IConsole::IResult *pResult, void *pUserData);
	static void ConEyeEmote(IConsole::IResult *pResult, void *pUserData);
	static void ConShowOthers(IConsole::IResult *pResult, void *pUserData);

	static void ConVoteMute(IConsole::IResult *pResult, void *pUserData);
	static void ConVoteUnmute(IConsole::IResult *pResult, void *pUserData);
	static void ConVoteMutes(IConsole::IResult *pResult, void *pUserData);
	static void ConMute(IConsole::IResult *pResult, void *pUserData);
	static void ConMuteID(IConsole::IResult *pResult, void *pUserData);
	static void ConMuteIP(IConsole::IResult *pResult, void *pUserData);
	static void ConUnmute(IConsole::IResult *pResult, void *pUserData);
	static void ConMutes(IConsole::IResult *pResult, void *pUserData);
	static void ConModerate(IConsole::IResult *pResult, void *pUserData);

	static void ConList(IConsole::IResult *pResult, void *pUserData);
	static void ConUninvite(IConsole::IResult *pResult, void *pUserData);

	static void ConReady(IConsole::IResult *pResult, void *pUserData);

	// instance console
	static void ConInstanceCommand(IConsole::IResult *pResult, void *pUserData);

	enum
	{
		MAX_MUTES = 32,
		MAX_VOTE_MUTES = 32,
	};
	struct CMute
	{
		NETADDR m_Addr;
		int m_Expire;
		char m_aReason[128];
	};

	CMute m_aMutes[MAX_MUTES];
	int m_NumMutes;
	CMute m_aVoteMutes[MAX_VOTE_MUTES];
	int m_NumVoteMutes;
	bool TryMute(const NETADDR *pAddr, int Secs, const char *pReason);
	void Mute(const NETADDR *pAddr, int Secs, const char *pDisplayName, const char *pReason = "");
	bool TryVoteMute(const NETADDR *pAddr, int Secs);
	bool VoteMute(const NETADDR *pAddr, int Secs, const char *pDisplayName, int AuthedID);
	bool VoteUnmute(const NETADDR *pAddr, const char *pDisplayName, int AuthedID);
	void Whisper(int ClientID, char *pStr);
	void WhisperID(int ClientID, int VictimID, const char *pMessage);
	void Converse(int ClientID, char *pStr);
	bool IsVersionBanned(int Version);
	void UnlockTeam(int ClientID, int Team);

public:
	CLayers *Layers() { return &m_Layers; }

	int m_VoteVictim;
	int m_VoteEnforcer;

	inline bool IsOptionVote() const { return m_VoteType == VOTE_TYPE_OPTION; };
	inline bool IsKickVote() const { return m_VoteType == VOTE_TYPE_KICK; };
	inline bool IsSpecVote() const { return m_VoteType == VOTE_TYPE_SPECTATE; };

	static void SendChatResponse(const char *pLine, void *pUser);
	static void SendChatResponseAll(const char *pLine, void *pUser);
	virtual void OnSetAuthed(int ClientID, int Level);
	virtual bool PlayerCollision();
	virtual bool PlayerHooking();
	virtual float PlayerJetpack();

	void ResetTuning();

	int m_ChatResponseTargetID;
	int m_ChatPrintCBIndex;

	static int64 ms_TeamMask[3];
	static int64 ms_SpectatorMask[MAX_CLIENTS];
	static int64 ms_TeamSpectatorMask[2];
};

inline int64 CmaskAll() { return -1LL; }
inline int64 CmaskOne(int ClientID) { return 1LL << ClientID; }
inline int64 CmaskViewer(int ClientID) { return CGameContext::ms_SpectatorMask[ClientID]; }
inline int64 CmaskOneAndViewer(int ClientID) { return 1LL << ClientID | CmaskViewer(ClientID); }
inline int64 CmaskTeam(int Team) { return CGameContext::ms_TeamMask[Team + 1]; }
inline int64 CmaskTeamViewer(int Team) { return CGameContext::ms_TeamSpectatorMask[Team]; }
inline int64 CmaskTeamAndViewer(int Team) { return CGameContext::ms_TeamMask[Team + 1] | CmaskTeamViewer(Team); }
inline int64 CmaskSet(int64 Mask, int ClientID) { return Mask | CmaskOne(ClientID); }
inline int64 CmaskUnset(int64 Mask, int ClientID) { return Mask & ~CmaskOne(ClientID); }
inline int64 CmaskAllExceptOne(int ClientID) { return ~CmaskOne(ClientID); }
inline bool CmaskIsSet(int64 Mask, int ClientID) { return (Mask & CmaskOne(ClientID)) != 0; }
#endif
