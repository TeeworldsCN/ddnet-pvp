/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_CONFIG_H
#define ENGINE_SHARED_CONFIG_H

#include <base/detect.h>
#include <engine/config.h>
#include <engine/console.h>

#define CONFIG_FILE "settings_ddnet.cfg"
#define AUTOEXEC_FILE "autoexec.cfg"
#define AUTOEXEC_CLIENT_FILE "autoexec_client.cfg"
#define AUTOEXEC_SERVER_FILE "autoexec_server.cfg"

class CConfig
{
public:
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc) int m_##Name;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc) unsigned m_##Name;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc) char m_##Name[Len]; // Flawfinder: ignore
#include "config_variables.h"
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_STR
};

extern CConfig g_Config;

enum
{
	CFGFLAG_SAVE = 1 << 0,
	CFGFLAG_CLIENT = 1 << 1,
	CFGFLAG_SERVER = 1 << 2,
	CFGFLAG_STORE = 1 << 3,
	CFGFLAG_MASTER = 1 << 4,
	CFGFLAG_ECON = 1 << 5,
	// DDRace

	CMDFLAG_TEST = 1 << 6,
	CFGFLAG_CHAT = 1 << 7,
	CFGFLAG_GAME = 1 << 8,
	CFGFLAG_NONTEEHISTORIC = 1 << 9,
	CFGFLAG_COLLIGHT = 1 << 10,
	CFGFLAG_COLALPHA = 1 << 11,
	CFGFLAG_INSTANCE = 1 << 12,
};

class CConfigManager : public IConfigManager
{
	enum
	{
		MAX_CALLBACKS = 16
	};

	struct CCallback
	{
		SAVECALLBACKFUNC m_pfnFunc;
		void *m_pUserData;
	};

	class IStorage *m_pStorage;
	IOHANDLE m_ConfigFile;
	bool m_Failed;
	CCallback m_aCallbacks[MAX_CALLBACKS];
	int m_NumCallbacks;

public:
	CConfigManager();

	virtual void Init();
	virtual void Reset();
	virtual bool Save();
	virtual CConfig *Values() { return &g_Config; }

	virtual void RegisterCallback(SAVECALLBACKFUNC pfnFunc, void *pUserData);

	virtual void WriteLine(const char *pLine);
};

struct CIntVariableData
{
	IConsole *m_pConsole;
	int *m_pVariable;
	int m_Min;
	int m_Max;
	int m_OldValue;
};

struct CColVariableData
{
	IConsole *m_pConsole;
	unsigned *m_pVariable;
	bool m_Light;
	bool m_Alpha;
	unsigned m_OldValue;
};

struct CStrVariableData
{
	IConsole *m_pConsole;
	char *m_pStr;
	int m_MaxSize;
	char *m_pOldValue;
};

static void IntVariableCommand(IConsole::IResult *pResult, void *pUserData)
{
	CIntVariableData *pData = (CIntVariableData *)pUserData;

	if(pResult->NumArguments())
	{
		int Val = pResult->GetInteger(0);

		// do clamping
		if(pData->m_Min != pData->m_Max)
		{
			if(Val < pData->m_Min)
				Val = pData->m_Min;
			if(pData->m_Max != 0 && Val > pData->m_Max)
				Val = pData->m_Max;
		}

		*(pData->m_pVariable) = Val;
		if(pResult->m_ClientID != IConsole::CLIENT_ID_GAME)
			pData->m_OldValue = Val;
	}
	else
	{
		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), "Value: %d", *(pData->m_pVariable));
		pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}
}

static void ColVariableCommand(IConsole::IResult *pResult, void *pUserData)
{
	CColVariableData *pData = (CColVariableData *)pUserData;

	if(pResult->NumArguments())
	{
		ColorHSLA Col = pResult->GetColor(0, pData->m_Light);
		int Val = Col.Pack(pData->m_Light ? 0.5f : 0.0f, pData->m_Alpha);

		*(pData->m_pVariable) = Val;
		if(pResult->m_ClientID != IConsole::CLIENT_ID_GAME)
			pData->m_OldValue = Val;
	}
	else
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "Value: %u", *(pData->m_pVariable));
		pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);

		ColorHSLA Hsla(*(pData->m_pVariable), true);
		if(pData->m_Light)
			Hsla = Hsla.UnclampLighting();
		str_format(aBuf, sizeof(aBuf), "H: %d°, S: %d%%, L: %d%%", round_truncate(Hsla.h * 360), round_truncate(Hsla.s * 100), round_truncate(Hsla.l * 100));
		pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);

		ColorRGBA Rgba = color_cast<ColorRGBA>(Hsla);
		str_format(aBuf, sizeof(aBuf), "R: %d, G: %d, B: %d, #%06X", round_truncate(Rgba.r * 255), round_truncate(Rgba.g * 255), round_truncate(Rgba.b * 255), Rgba.Pack(false));
		pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);

		if(pData->m_Alpha)
		{
			str_format(aBuf, sizeof(aBuf), "A: %d%%", round_truncate(Hsla.a * 100));
			pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
		}
	}
}

static void StrVariableCommand(IConsole::IResult *pResult, void *pUserData)
{
	CStrVariableData *pData = (CStrVariableData *)pUserData;

	if(pResult->NumArguments())
	{
		const char *pString = pResult->GetString(0);
		if(!str_utf8_check(pString))
		{
			char aTemp[4];
			int Length = 0;
			while(*pString)
			{
				int Size = str_utf8_encode(aTemp, static_cast<unsigned char>(*pString++));
				if(Length + Size < pData->m_MaxSize)
				{
					mem_copy(pData->m_pStr + Length, aTemp, Size);
					Length += Size;
				}
				else
					break;
			}
			pData->m_pStr[Length] = 0;
		}
		else
			str_copy(pData->m_pStr, pString, pData->m_MaxSize);

		if(pResult->m_ClientID != IConsole::CLIENT_ID_GAME)
			str_copy(pData->m_pOldValue, pData->m_pStr, pData->m_MaxSize);
	}
	else
	{
		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "Value: %s", pData->m_pStr);
		pData->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}
}

#endif
