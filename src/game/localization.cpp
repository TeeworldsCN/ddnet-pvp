/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "localization.h"
#include <base/tl/algorithm.h>

#include <engine/console.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>

CLocalizedString *LocalizeServer(const char *pStr, const char *pContext)
{
	unsigned Hash = str_quickhash(pStr);
	unsigned ContextHash = str_quickhash(pContext);
	return g_Localization.FindString(Hash, ContextHash);
}

CLocalizationDatabase::CLocalizationDatabase()
{
	m_NumLanguages = 1; // English is 0
}

void CLocalizationDatabase::AddString(int Lang, unsigned Hash, unsigned ContextHash, const char *pOrigStr, const char *pNewStr)
{
	CLocalizedString s;
	s.m_Hash = Hash;
	s.m_ContextHash = ContextHash;
	if(pNewStr)
		s.m_Langs[Lang] = pNewStr;
	s.m_Langs[0] = pOrigStr;
	m_Strings.add(s);
}

int CLocalizationDatabase::Load(const char *pFilename, IStorage *pStorage, IConsole *pConsole)
{
	if(pFilename[0] == 0)
		return false;

	IOHANDLE IoHandle = pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!IoHandle)
		return false;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "loaded '%s'", pFilename);
	pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);

	char aContext[512];
	char aOrigin[512];
	CLineReader LineReader;
	LineReader.Init(IoHandle);
	char *pLine;
	int Line = 0;
	while((pLine = LineReader.Get()))
	{
		Line++;
		if(!str_length(pLine))
			continue;

		if(pLine[0] == '#') // skip comments
			continue;

		if(pLine[0] == '[') // context
		{
			size_t Len = str_length(pLine);
			if(Len < 1 || pLine[Len - 1] != ']')
			{
				str_format(aBuf, sizeof(aBuf), "malform context line (%d): %s", Line, pLine);
				pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);
				continue;
			}
			str_copy(aContext, pLine + 1, Len - 1);
			pLine = LineReader.Get();
		}
		else
		{
			aContext[0] = '\0';
		}

		str_copy(aOrigin, pLine, sizeof(aOrigin));
		char *pReplacement = LineReader.Get();
		Line++;
		if(!pReplacement)
		{
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", "unexpected end of file");
			break;
		}

		if(pReplacement[0] != '=' || pReplacement[1] != '=' || pReplacement[2] != ' ')
		{
			str_format(aBuf, sizeof(aBuf), "malform replacement line (%d) for '%s'", Line, aOrigin);
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);
			continue;
		}

		pReplacement += 3;
		unsigned Hash = str_quickhash(aOrigin);
		unsigned ContextHash = str_quickhash(aContext);
		CLocalizedString *String = FindString(Hash, ContextHash);
		if(String)
			String->m_Langs[m_NumLanguages] = pReplacement;
		else
			AddString(m_NumLanguages, Hash, ContextHash, aOrigin, pReplacement);
	}
	io_close(IoHandle);

	return m_NumLanguages++;
}

CLocalizedString *CLocalizationDatabase::FindString(unsigned Hash, unsigned ContextHash)
{
	CLocalizedString String;
	String.m_Hash = Hash;
	String.m_ContextHash = ContextHash;

	sorted_array<CLocalizedString>::range r = ::find_binary(m_Strings.all(), String);
	if(r.empty())
		return nullptr;

	unsigned DefaultHash = str_quickhash("");
	unsigned DefaultIndex = 0;
	for(unsigned i = 0; i < r.size() && r.index(i).m_Hash == Hash; ++i)
	{
		CLocalizedString &rStr = r.index(i);
		if(rStr.m_ContextHash == ContextHash)
			return &rStr;
		else if(rStr.m_ContextHash == DefaultHash)
			DefaultIndex = i;
	}
	return &r.index(DefaultIndex);
}

CLocalizationDatabase g_Localization;
