#include "textentity.h"

static struct SFontDot
{
	int m_Width;
	int m_Dots;
	const char *m_Data;
} s_FontDotData[256] = {
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{3, 5, "\x08\x0f\x16\x1d\x32"}, // '!'
	{3, 4, "\x07\x09\x0e\x10"}, // '"'
	{5, 20, "\x08\x0a\x0f\x11\x15\x16\x17\x18\x19\x1d\x1f\x23\x24\x25\x26\x27\x2b\x2d\x32\x34"}, // '#'
	{5, 21, "\x02\x08\x09\x0a\x0e\x10\x12\x15\x17\x1d\x1e\x1f\x25\x27\x2a\x2c\x2e\x32\x33\x34\x3a"}, // '$'
	{5, 15, "\x07\x08\x0b\x0e\x0f\x12\x18\x1e\x24\x2a\x2d\x2e\x31\x34\x35"}, // '%'
	{5, 15, "\x09\x0f\x11\x16\x18\x1d\x1e\x23\x25\x27\x2a\x2d\x32\x33\x35"}, // '&'
	{3, 2, "\x08\x0f"}, // '''
	{3, 9, "\x02\x08\x0e\x15\x1c\x23\x2a\x32\x3a"}, // '('
	{3, 9, "\x00\x08\x10\x17\x1e\x25\x2c\x32\x38"}, // ')'
	{5, 11, "\x02\x07\x09\x0b\x0f\x10\x11\x15\x17\x19\x1e"}, // '*'
	{5, 9, "\x10\x17\x1c\x1d\x1e\x1f\x20\x25\x2c"}, // '+'
	{3, 2, "\x32\x38"}, // ','
	{5, 5, "\x1c\x1d\x1e\x1f\x20"}, // '-'
	{3, 1, "\x32"}, // '.'
	{3, 7, "\x09\x10\x16\x1d\x24\x2a\x31"}, // '/'
	{5, 19, "\x08\x09\x0a\x0e\x12\x15\x18\x19\x1c\x1e\x20\x23\x24\x27\x2a\x2e\x32\x33\x34"}, // '0'
	{5, 10, "\x09\x0f\x10\x17\x1e\x25\x2c\x32\x33\x34"}, // '1'
	{5, 14, "\x08\x09\x0a\x0e\x12\x19\x1f\x25\x2b\x31\x32\x33\x34\x35"}, // '2'
	{5, 14, "\x07\x08\x09\x0a\x0b\x11\x17\x1f\x27\x2a\x2e\x32\x33\x34"}, // '3'
	{5, 14, "\x0a\x10\x11\x16\x18\x1c\x1f\x23\x24\x25\x26\x27\x2d\x34"}, // '4'
	{5, 17, "\x07\x08\x09\x0a\x0b\x0e\x15\x16\x17\x18\x20\x27\x2a\x2e\x32\x33\x34"}, // '5'
	{5, 15, "\x09\x0a\x0f\x15\x1c\x1d\x1e\x1f\x23\x27\x2a\x2e\x32\x33\x34"}, // '6'
	{5, 11, "\x07\x08\x09\x0a\x0b\x12\x18\x1e\x24\x2b\x32"}, // '7'
	{5, 17, "\x08\x09\x0a\x0e\x12\x15\x19\x1d\x1e\x1f\x23\x27\x2a\x2e\x32\x33\x34"}, // '8'
	{5, 15, "\x08\x09\x0a\x0e\x12\x15\x19\x1d\x1e\x1f\x20\x27\x2d\x32\x33"}, // '9'
	{3, 2, "\x0f\x2b"}, // ':'
	{3, 3, "\x0f\x2b\x31"}, // ';'
	{3, 5, "\x10\x16\x1c\x24\x2c"}, // '<'
	{5, 10, "\x15\x16\x17\x18\x19\x23\x24\x25\x26\x27"}, // '='
	{3, 5, "\x0e\x16\x1e\x24\x2a"}, // '>'
	{5, 10, "\x08\x09\x0a\x0e\x12\x19\x1e\x1f\x25\x33"}, // '?'
	{5, 20, "\x08\x09\x0a\x0e\x12\x15\x17\x19\x1c\x1d\x1f\x20\x23\x25\x26\x2a\x2e\x32\x33\x34"}, // '@'
	{5, 16, "\x09\x0f\x11\x15\x19\x1c\x20\x23\x24\x25\x26\x27\x2a\x2e\x31\x35"}, // 'A'
	{5, 20, "\x07\x08\x09\x0a\x0e\x12\x15\x19\x1c\x1d\x1e\x1f\x23\x27\x2a\x2e\x31\x32\x33\x34"}, // 'B'
	{5, 13, "\x08\x09\x0a\x0e\x12\x15\x1c\x23\x2a\x2e\x32\x33\x34"}, // 'C'
	{5, 18, "\x07\x08\x09\x0a\x0e\x12\x15\x19\x1c\x20\x23\x27\x2a\x2e\x31\x32\x33\x34"}, // 'D'
	{5, 18, "\x07\x08\x09\x0a\x0b\x0e\x15\x1c\x1d\x1e\x1f\x23\x2a\x31\x32\x33\x34\x35"}, // 'E'
	{5, 14, "\x07\x08\x09\x0a\x0b\x0e\x15\x1c\x1d\x1e\x1f\x23\x2a\x31"}, // 'F'
	{5, 17, "\x08\x09\x0a\x0e\x12\x15\x1c\x1e\x1f\x20\x23\x27\x2a\x2e\x32\x33\x34"}, // 'G'
	{5, 17, "\x07\x0b\x0e\x12\x15\x19\x1c\x1d\x1e\x1f\x20\x23\x27\x2a\x2e\x31\x35"}, // 'H'
	{5, 11, "\x08\x09\x0a\x10\x17\x1e\x25\x2c\x32\x33\x34"}, // 'I'
	{5, 14, "\x07\x08\x09\x0a\x0b\x12\x19\x20\x27\x2a\x2e\x32\x33\x34"}, // 'J'
	{5, 14, "\x07\x0b\x0e\x11\x15\x17\x1c\x1d\x23\x25\x2a\x2d\x31\x35"}, // 'K'
	{5, 11, "\x07\x0e\x15\x1c\x23\x2a\x31\x32\x33\x34\x35"}, // 'L'
	{5, 17, "\x07\x0b\x0e\x0f\x11\x12\x15\x17\x19\x1c\x20\x23\x27\x2a\x2e\x31\x35"}, // 'M'
	{5, 17, "\x07\x0b\x0e\x12\x15\x16\x19\x1c\x1e\x20\x23\x26\x27\x2a\x2e\x31\x35"}, // 'N'
	{5, 16, "\x08\x09\x0a\x0e\x12\x15\x19\x1c\x20\x23\x27\x2a\x2e\x32\x33\x34"}, // 'O'
	{5, 15, "\x07\x08\x09\x0a\x0e\x12\x15\x19\x1c\x1d\x1e\x1f\x23\x2a\x31"}, // 'P'
	{5, 18, "\x08\x09\x0a\x0e\x12\x15\x19\x1c\x20\x23\x25\x27\x2a\x2d\x2e\x32\x33\x34"}, // 'Q'
	{5, 18, "\x07\x08\x09\x0a\x0e\x12\x15\x19\x1c\x1d\x1e\x1f\x23\x25\x2a\x2d\x31\x35"}, // 'R'
	{5, 15, "\x08\x09\x0a\x0e\x12\x15\x1d\x1e\x1f\x27\x2a\x2e\x32\x33\x34"}, // 'S'
	{5, 11, "\x07\x08\x09\x0a\x0b\x10\x17\x1e\x25\x2c\x33"}, // 'T'
	{5, 15, "\x07\x0b\x0e\x12\x15\x19\x1c\x20\x23\x27\x2a\x2e\x32\x33\x34"}, // 'U'
	{5, 13, "\x07\x0b\x0e\x12\x15\x19\x1c\x20\x23\x27\x2b\x2d\x33"}, // 'V'
	{5, 17, "\x07\x0b\x0e\x12\x15\x17\x19\x1c\x1e\x20\x23\x25\x27\x2b\x2d\x32\x34"}, // 'W'
	{5, 11, "\x07\x0b\x0f\x11\x17\x1e\x25\x2b\x2d\x31\x35"}, // 'X'
	{5, 11, "\x07\x0b\x0e\x12\x16\x18\x1d\x1f\x25\x2c\x33"}, // 'Y'
	{5, 15, "\x07\x08\x09\x0a\x0b\x12\x18\x1e\x24\x2a\x31\x32\x33\x34\x35"}, // 'Z'
	{3, 11, "\x00\x01\x07\x0e\x15\x1c\x23\x2a\x31\x38\x39"}, // '['
	{3, 7, "\x07\x0e\x16\x1d\x24\x2c\x33"}, // '\'
	{3, 11, "\x00\x01\x08\x0f\x16\x1d\x24\x2b\x32\x38\x39"}, // ']'
	{3, 3, "\x08\x0e\x10"}, // '^'
	{5, 5, "\x31\x32\x33\x34\x35"}, // '_'
	{3, 2, "\x07\x0f"}, // '`'
	{5, 14, "\x16\x17\x18\x20\x24\x25\x26\x27\x2a\x2e\x32\x33\x34\x35"}, // 'a'
	{5, 16, "\x07\x0e\x15\x16\x17\x18\x1c\x20\x23\x27\x2a\x2e\x31\x32\x33\x34"}, // 'b'
	{5, 11, "\x16\x17\x18\x19\x1c\x23\x2a\x32\x33\x34\x35"}, // 'c'
	{5, 16, "\x0b\x12\x16\x17\x18\x19\x1c\x20\x23\x27\x2a\x2e\x32\x33\x34\x35"}, // 'd'
	{5, 14, "\x16\x17\x18\x1c\x20\x23\x24\x25\x26\x27\x2a\x32\x33\x34"}, // 'e'
	{5, 12, "\x09\x0a\x10\x16\x17\x18\x1e\x25\x2c\x32\x33\x34"}, // 'f'
	{5, 18, "\x16\x17\x18\x19\x1c\x20\x23\x27\x2a\x2e\x32\x33\x34\x35\x3c\x40\x41\x42"}, // 'g'
	{5, 14, "\x07\x0e\x15\x16\x17\x18\x1c\x20\x23\x27\x2a\x2e\x31\x35"}, // 'h'
	{5, 9, "\x09\x16\x17\x1e\x25\x2c\x32\x33\x34"}, // 'i'
	{5, 10, "\x09\x16\x17\x18\x1e\x25\x2c\x33\x3a\x40"}, // 'j'
	{5, 13, "\x07\x0e\x15\x19\x1c\x1f\x23\x24\x25\x2a\x2d\x31\x35"}, // 'k'
	{5, 10, "\x08\x09\x10\x17\x1e\x25\x2c\x32\x33\x34"}, // 'l'
	{5, 16, "\x15\x16\x17\x18\x1c\x1e\x20\x23\x25\x27\x2a\x2c\x2e\x31\x33\x35"}, // 'm'
	{5, 12, "\x15\x16\x17\x18\x1c\x20\x23\x27\x2a\x2e\x31\x35"}, // 'n'
	{5, 12, "\x16\x17\x18\x1c\x20\x23\x27\x2a\x2e\x32\x33\x34"}, // 'o'
	{5, 16, "\x15\x16\x17\x18\x1c\x20\x23\x27\x2a\x2e\x31\x32\x33\x34\x38\x3f"}, // 'p'
	{5, 16, "\x16\x17\x18\x19\x1c\x20\x23\x27\x2a\x2e\x32\x33\x34\x35\x3c\x43"}, // 'q'
	{5, 8, "\x16\x18\x19\x1d\x1e\x24\x2b\x32"}, // 'r'
	{5, 13, "\x16\x17\x18\x19\x1c\x24\x25\x26\x2e\x31\x32\x33\x34"}, // 's'
	{5, 8, "\x10\x16\x17\x18\x1e\x25\x2c\x34"}, // 't'
	{5, 12, "\x15\x19\x1c\x20\x23\x27\x2a\x2d\x2e\x32\x33\x35"}, // 'u'
	{5, 9, "\x15\x19\x1c\x20\x23\x27\x2b\x2d\x33"}, // 'v'
	{5, 13, "\x15\x19\x1c\x1e\x20\x23\x25\x27\x2b\x2c\x2d\x32\x34"}, // 'w'
	{5, 9, "\x15\x19\x1d\x1f\x25\x2b\x2d\x31\x35"}, // 'x'
	{5, 16, "\x15\x19\x1c\x20\x23\x27\x2a\x2e\x32\x33\x34\x35\x3c\x40\x41\x42"}, // 'y'
	{5, 13, "\x15\x16\x17\x18\x19\x1f\x25\x2b\x31\x32\x33\x34\x35"}, // 'z'
	{3, 11, "\x01\x02\x08\x0f\x15\x1c\x23\x2b\x32\x39\x3a"}, // '{'
	{3, 7, "\x08\x0f\x16\x1d\x24\x2b\x32"}, // '|'
	{3, 11, "\x00\x01\x08\x0f\x17\x1e\x25\x2b\x32\x38\x39"}, // '}'
	{5, 5, "\x16\x19\x1c\x1e\x1f"}, // '~'
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{3, 5, "\x08\x1d\x24\x2b\x32"}, // '¡'
	{5, 18, "\x02\x09\x0f\x10\x11\x15\x17\x19\x1c\x1e\x23\x25\x27\x2b\x2c\x2d\x33\x3a"}, // '¢'
	{5, 15, "\x09\x0a\x0f\x12\x16\x1c\x1d\x1e\x24\x2b\x31\x32\x33\x34\x35"}, // '£'
	{5, 16, "\x07\x0b\x0f\x10\x11\x15\x19\x1c\x20\x23\x27\x2b\x2c\x2d\x31\x35"}, // '¤'
	{5, 17, "\x07\x0b\x0f\x11\x17\x1c\x1d\x1e\x1f\x20\x25\x2a\x2b\x2c\x2d\x2e\x33"}, // '¥'
	{3, 6, "\x08\x0f\x16\x24\x2b\x32"}, // '¦'
	{5, 18, "\x08\x09\x0a\x0e\x12\x15\x16\x17\x1d\x1f\x25\x26\x27\x2a\x2e\x32\x33\x34"}, // '§'
	{3, 2, "\x07\x09"}, // '¨'
	{7, 21, "\x09\x0a\x0b\x0f\x13\x15\x18\x19\x1b\x1c\x1e\x22\x23\x26\x27\x29\x2b\x2f\x33\x34\x35"}, // '©'
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
};

CTextEntity::CTextEntity(CGameWorld *pGameWorld, vec2 Pos, int Type, int GapSize, int Align, char *pText, float Time) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_CUSTOM, Pos)
{
	m_Type = Type;
	m_PrevPrevPos = m_PrevPos = Pos;
	m_PrevVelocity = {0.0f, 0.0f};
	m_Velocity = {0.0f, 0.0f};
	m_GapSize = GapSize;

	if(Time > 0)
		m_LifeSpan = (int)(Time * Server()->TickSpeed());
	else
		m_LifeSpan = -1;

	m_TextLen = str_length(pText);
	m_pText = (char *)malloc(m_TextLen * sizeof(char));
	mem_copy(m_pText, pText, m_TextLen);
	m_BoxWidth = m_GapSize * 2 * (m_TextLen - 1);
	m_BoxHeight = m_GapSize * 8;
	m_NumIDs = 0;
	for(int c = 0; c < m_TextLen; c++)
	{
		m_BoxWidth += (s_FontDotData[(unsigned char)pText[c]].m_Width - 1) * m_GapSize;
		m_NumIDs += s_FontDotData[(unsigned char)pText[c]].m_Dots;
	}

	if(m_NumIDs > 0)
	{
		m_pIDs = (int *)malloc(m_NumIDs * sizeof(int));
		for(int i = 0; i < m_NumIDs; i++)
			m_pIDs[i] = Server()->SnapNewID();
	}
	else
	{
		m_pIDs = nullptr;
	}

	if(Align == ALIGN_MIDDLE)
		m_Offset = {-m_BoxWidth / 2.0f, -m_BoxHeight / 2.0f};
	else if(Align == ALIGN_RIGHT)
		m_Offset = {-m_BoxWidth, -m_BoxHeight / 2.0f};

	pGameWorld->InsertEntity(this);
}

CTextEntity::~CTextEntity()
{
	for(int i = 0; i < m_NumIDs; i++)
		Server()->SnapFreeID(m_pIDs[i]);
	if(m_pIDs)
		free(m_pIDs);
	if(m_pText)
		free(m_pText);
}

void CTextEntity::Reset()
{
	Destroy();
}

void CTextEntity::Tick()
{
	m_PrevVelocity = (m_Pos - m_PrevPrevPos) * 2.0f / (float)Server()->TickSpeed();
	m_Velocity = (m_Pos - m_PrevPos) / (float)Server()->TickSpeed();
	m_PrevPrevPos = m_PrevPos;
	m_PrevPos = m_Pos;

	if(m_LifeSpan > 0)
		m_LifeSpan--;
	if(m_LifeSpan == 0)
		Destroy();
}

bool CTextEntity::NetworkClipped(int SnappingClient)
{
	vec2 TL = m_Pos + m_Offset;
	vec2 BR = vec2(TL.x + m_BoxWidth, TL.y + m_BoxHeight);
	return NetworkRectClipped(SnappingClient, TL, BR);
}

void CTextEntity::Snap(int SnappingClient, int OtherMode)
{
	if(OtherMode)
		return;

	if(m_Type == TYPE_LASER)
		SnapLaser();
	else if(m_Type >= TYPE_GUN && m_Type <= TYPE_GRENADE)
		SnapProjectile();
	else
		SnapPickup(SnappingClient);
}

void CTextEntity::MoveTo(vec2 Pos)
{
	m_Pos = Pos;
}

void CTextEntity::TeleportTo(vec2 Pos)
{
	m_PrevVelocity = m_Velocity = {0.0f, 0.0f};
	m_PrevPrevPos = m_PrevPos = m_Pos = Pos;
}

void CTextEntity::SnapLaser()
{
	float XOffset = 0;
	int DotIndex = 0;
	for(int c = 0; c < m_TextLen; c++)
	{
		SFontDot Dot = s_FontDotData[(unsigned char)m_pText[c]];
		for(int d = 0; d < Dot.m_Dots; d++)
		{
			int X = (int)Dot.m_Data[d] % 7;
			int Y = (int)Dot.m_Data[d] / 7;

			vec2 Offset = vec2(XOffset + X * m_GapSize, Y * m_GapSize);

			CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_pIDs[DotIndex], sizeof(CNetObj_Laser)));
			if(!pObj)
				return;

			vec2 Position = m_Pos + m_Offset + Offset;
			pObj->m_X = (int)Position.x;
			pObj->m_Y = (int)Position.y;
			pObj->m_FromX = (int)Position.x;
			pObj->m_FromY = (int)Position.y;
			pObj->m_StartTick = Server()->Tick();
			DotIndex++;
		}
		XOffset += (Dot.m_Width + 1) * m_GapSize;
	}
}

void CTextEntity::SnapProjectile()
{
	float XOffset = 0;
	int DotIndex = 0;
	float Delta = 2.0f / (float)Server()->TickSpeed();
	int VelX = 0;
	int VelY = 0;

	if(length(m_PrevVelocity) != 0.0f)
	{
		float Curvature = 0;
		float Speed = 0;
		GetProjectileProperties(&Curvature, &Speed);
		vec2 Direction = normalize(m_PrevVelocity);
		vec2 ClientPos = CalcPos(m_PrevPrevPos, Direction, Curvature, Speed, Delta);
		vec2 ClientVel = (ClientPos - m_PrevPrevPos) * Delta;

		if(fabs(ClientVel.x) < 1e-6)
			VelX = 0;
		else
			VelX = (int)(Direction.x * (m_PrevVelocity.x / ClientVel.x) * 100.0f);

		if(fabs(ClientVel.y) < 1e-6)
			VelY = 0;
		else
			VelY = (int)(Direction.y * (m_PrevVelocity.y / ClientVel.y) * 100.0f);
	}

	for(int c = 0; c < m_TextLen; c++)
	{
		SFontDot Dot = s_FontDotData[(unsigned char)m_pText[c]];
		for(int d = 0; d < Dot.m_Dots; d++)
		{
			int X = (int)Dot.m_Data[d] % 7;
			int Y = (int)Dot.m_Data[d] / 7;

			vec2 Position = m_PrevPrevPos + m_Offset + vec2(XOffset + X * m_GapSize, Y * m_GapSize);

			CNetObj_Projectile *pProj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_pIDs[DotIndex], sizeof(CNetObj_Projectile)));
			if(!pProj)
				return;

			pProj->m_StartTick = Server()->Tick() - 2;
			pProj->m_Type = m_Type - (TYPE_GUN - WEAPON_GUN);

			pProj->m_X = round_to_int(Position.x);
			pProj->m_Y = round_to_int(Position.y);
			pProj->m_VelX = VelX;
			pProj->m_VelY = VelY;

			DotIndex++;
		}
		XOffset += (Dot.m_Width + 1) * m_GapSize;
	}
}

void CTextEntity::SnapPickup(int SnappingClient)
{
	int PickupType = POWERUP_HEALTH;
	if(m_Type == TYPE_HEART)
		PickupType = POWERUP_HEALTH;
	else if(m_Type == TYPE_ARMOR)
		PickupType = POWERUP_ARMOR;

	float XOffset = 0;
	int DotIndex = 0;
	int Size = Server()->IsSixup(SnappingClient) ? 3 * 4 : sizeof(CNetObj_Pickup);

	for(int c = 0; c < m_TextLen; c++)
	{
		SFontDot Dot = s_FontDotData[(unsigned char)m_pText[c]];
		for(int d = 0; d < Dot.m_Dots; d++)
		{
			int X = (int)Dot.m_Data[d] % 7;
			int Y = (int)Dot.m_Data[d] / 7;

			vec2 Position = m_Pos + m_Offset + vec2(XOffset + X * m_GapSize, Y * m_GapSize);

			CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_pIDs[DotIndex], Size));
			if(!pP)
				return;

			pP->m_X = (int)Position.x;
			pP->m_Y = (int)Position.y;
			pP->m_Type = PickupType;

			if(!Server()->IsSixup(SnappingClient))
				pP->m_Subtype = -1;
			DotIndex++;
		}
		XOffset += (Dot.m_Width + 1) * m_GapSize;
	}
}

void CTextEntity::GetProjectileProperties(float *pCurvature, float *pSpeed, int TuneZone)
{
	int Type = m_Type - (TYPE_GUN - WEAPON_GUN);
	switch(Type)
	{
	case WEAPON_GRENADE:
		if(!TuneZone)
		{
			*pCurvature = GameServer()->Tuning()->m_GrenadeCurvature;
			*pSpeed = GameServer()->Tuning()->m_GrenadeSpeed;
		}
		else
		{
			*pCurvature = GameServer()->TuningList()[TuneZone].m_GrenadeCurvature;
			*pSpeed = GameServer()->TuningList()[TuneZone].m_GrenadeSpeed;
		}

		break;

	case WEAPON_SHOTGUN:
		if(!TuneZone)
		{
			*pCurvature = GameServer()->Tuning()->m_ShotgunCurvature;
			*pSpeed = GameServer()->Tuning()->m_ShotgunSpeed;
		}
		else
		{
			*pCurvature = GameServer()->TuningList()[TuneZone].m_ShotgunCurvature;
			*pSpeed = GameServer()->TuningList()[TuneZone].m_ShotgunSpeed;
		}

		break;

	case WEAPON_GUN:
		if(!TuneZone)
		{
			*pCurvature = GameServer()->Tuning()->m_GunCurvature;
			*pSpeed = GameServer()->Tuning()->m_GunSpeed;
		}
		else
		{
			*pCurvature = GameServer()->TuningList()[TuneZone].m_GunCurvature;
			*pSpeed = GameServer()->TuningList()[TuneZone].m_GunSpeed;
		}
		break;
	}
}
