/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_MASTERSERVER_H
#define ENGINE_MASTERSERVER_H

#include "kernel.h"

static const int MASTERSERVER_PORT = 8300;

enum ServerType
{
	SERVERTYPE_INVALID = -1,
	SERVERTYPE_NORMAL,
	SERVERTYPE_LEGACY
};

struct CMastersrvAddr
{
	unsigned char m_aIp[16];
	unsigned char m_aPort[2];
};

static const unsigned char SERVERBROWSE_HEARTBEAT[] = {255, 255, 255, 255, 'b', 'e', 'a', '2'};

static const unsigned char SERVERBROWSE_GETLIST[] = {255, 255, 255, 255, 'r', 'e', 'q', '2'};
static const unsigned char SERVERBROWSE_LIST[] = {255, 255, 255, 255, 'l', 'i', 's', '2'};

static const unsigned char SERVERBROWSE_GETCOUNT[] = {255, 255, 255, 255, 'c', 'o', 'u', '2'};
static const unsigned char SERVERBROWSE_COUNT[] = {255, 255, 255, 255, 's', 'i', 'z', '2'};

static const unsigned char SERVERBROWSE_GETINFO[] = {255, 255, 255, 255, 'g', 'i', 'e', '3'};
static const unsigned char SERVERBROWSE_INFO[] = {255, 255, 255, 255, 'i', 'n', 'f', '3'};

static const unsigned char SERVERBROWSE_GETINFO_64_LEGACY[] = {255, 255, 255, 255, 'f', 's', 't', 'd'};
static const unsigned char SERVERBROWSE_INFO_64_LEGACY[] = {255, 255, 255, 255, 'd', 't', 's', 'f'};

static const unsigned char SERVERBROWSE_INFO_EXTENDED[] = {255, 255, 255, 255, 'i', 'e', 'x', 't'};
static const unsigned char SERVERBROWSE_INFO_EXTENDED_MORE[] = {255, 255, 255, 255, 'i', 'e', 'x', '+'};

static const unsigned char SERVERBROWSE_FWCHECK[] = {255, 255, 255, 255, 'f', 'w', '?', '?'};
static const unsigned char SERVERBROWSE_FWRESPONSE[] = {255, 255, 255, 255, 'f', 'w', '!', '!'};
static const unsigned char SERVERBROWSE_FWOK[] = {255, 255, 255, 255, 'f', 'w', 'o', 'k'};
static const unsigned char SERVERBROWSE_FWERROR[] = {255, 255, 255, 255, 'f', 'w', 'e', 'r'};

// packet headers for the 0.5 branch

struct CMastersrvAddrLegacy
{
	unsigned char m_aIp[4];
	unsigned char m_aPort[2];
};

static const unsigned char SERVERBROWSE_HEARTBEAT_LEGACY[] = {255, 255, 255, 255, 'b', 'e', 'a', 't'};

static const unsigned char SERVERBROWSE_GETLIST_LEGACY[] = {255, 255, 255, 255, 'r', 'e', 'q', 't'};
static const unsigned char SERVERBROWSE_LIST_LEGACY[] = {255, 255, 255, 255, 'l', 'i', 's', 't'};

static const unsigned char SERVERBROWSE_GETCOUNT_LEGACY[] = {255, 255, 255, 255, 'c', 'o', 'u', 'n'};
static const unsigned char SERVERBROWSE_COUNT_LEGACY[] = {255, 255, 255, 255, 's', 'i', 'z', 'e'};

enum
{
	SERVERINFO_VANILLA = 0,
	SERVERINFO_64_LEGACY,
	SERVERINFO_EXTENDED,
	SERVERINFO_EXTENDED_MORE,
	SERVERINFO_INGAME,
};

class IMasterServer : public IInterface
{
	MACRO_INTERFACE("masterserver", 0)
public:
	enum
	{
		MAX_MASTERSERVERS = 4
	};

	virtual void Init() = 0;
	virtual void SetDefault() = 0;
	virtual int Load() = 0;
	virtual int Save() = 0;

	virtual int RefreshAddresses(int Nettype) = 0;
	virtual void Update() = 0;
	virtual bool IsRefreshing() const = 0;
	virtual NETADDR GetAddr(int Index) const = 0;
	virtual void SetCount(int Index, int Count) = 0;
	virtual int GetCount(int Index) const = 0;
	virtual const char *GetName(int Index) const = 0;
	virtual bool IsValid(int Index) const = 0;
};

class IEngineMasterServer : public IMasterServer
{
	MACRO_INTERFACE("enginemasterserver", 0)
public:
};

extern IEngineMasterServer *CreateEngineMasterServer();

#endif
