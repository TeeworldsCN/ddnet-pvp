/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_LOCALIZATION_H
#define GAME_LOCALIZATION_H
#include <base/tl/sorted_array.h>
#include <base/tl/string.h>

class CLocalizedString
{
public:
	unsigned m_Hash;
	unsigned m_ContextHash;
	string m_Langs[64];

	bool operator<(const CLocalizedString &Other) const { return m_Hash < Other.m_Hash || (m_Hash == Other.m_Hash && m_ContextHash < Other.m_ContextHash); }
	bool operator<=(const CLocalizedString &Other) const { return m_Hash < Other.m_Hash || (m_Hash == Other.m_Hash && m_ContextHash <= Other.m_ContextHash); }
	bool operator==(const CLocalizedString &Other) const { return m_Hash == Other.m_Hash && m_ContextHash == Other.m_ContextHash; }
};

class CLocalizationDatabase
{
	sorted_array<CLocalizedString> m_Strings;
	int m_NumLanguages;

public:
	CLocalizationDatabase();

	int Load(const char *pFilename, class IStorage *pStorage, class IConsole *pConsole);

	void AddString(int Lang, unsigned Hash, unsigned ContextHash, const char *pOrigStr, const char *pNewStr);
	CLocalizedString *FindString(unsigned Hash, unsigned ContextHash);
};

extern CLocalizationDatabase g_Localization;

extern CLocalizedString *LocalizeServer(const char *pStr, const char *pContext = "");
#endif
