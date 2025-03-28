#ifndef GAME_SERVER_TEEINFO_H
#define GAME_SERVER_TEEINFO_H

class CTeeInfo
{
public:
	constexpr static const float DARKEST_LGT_7 = 61 / 255.0f;

	char m_SkinName[64] = {'\0'};
	int m_UseCustomColor = 0;
	int m_ColorBody = 0;
	int m_ColorFeet = 0;

	// 0.7
	char m_apSkinPartNames[6][24] = {"", "", "", "", "", ""};
	bool m_aUseCustomColors[6] = {false, false, false, false, false, false};
	int m_aSkinPartColors[6] = {0, 0, 0, 0, 0, 0};

	CTeeInfo() = default;

	CTeeInfo(const char *pSkinName, int UseCustomColor, int ColorBody, int ColorFeet);

	// This constructor will assume all arrays are of length 6
	CTeeInfo(const char *pSkinPartNames[6], int *pUseCustomColors, int *pSkinPartColors);

	void FromSixup();
	void ToSixup();
	void SkinFromSixup();
	void SkinToSixup();
	void ColorFromSixup();
	void ColorToSixup();
};
#endif // GAME_SERVER_TEEINFO_H
