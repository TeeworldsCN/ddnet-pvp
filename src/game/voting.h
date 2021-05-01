/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_VOTING_H
#define GAME_VOTING_H

enum
{
	VOTE_DESC_LENGTH = 64,
	VOTE_CMD_LENGTH = 512,
	VOTE_REASON_LENGTH = 16,

	MAX_VOTE_OPTIONS = 4096 - 32, // 8192 in total, leave 32 for rooms, 4096 - 16 for each global and room-wise votes.
};

enum
{
	VOTE_ENFORCE_UNKNOWN = 0,
	VOTE_ENFORCE_NO,
	VOTE_ENFORCE_YES,
	VOTE_ENFORCE_ABORT,

	VOTE_ENFORCE_NO_ADMIN = VOTE_ENFORCE_YES + 1,
	VOTE_ENFORCE_YES_ADMIN,

	VOTE_TYPE_UNKNOWN = 0,
	VOTE_TYPE_OPTION,
	VOTE_TYPE_KICK,
	VOTE_TYPE_SPECTATE,
};

struct CVoteOptionClient
{
	CVoteOptionClient *m_pNext;
	CVoteOptionClient *m_pPrev;
	char m_aDescription[VOTE_DESC_LENGTH];
};

struct CVoteOptionServer
{
	CVoteOptionServer *m_pNext;
	CVoteOptionServer *m_pPrev;
	char m_aDescription[VOTE_DESC_LENGTH];
	char m_aCommand[1];
};

#endif
