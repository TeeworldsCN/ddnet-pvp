/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_LMS_H
#define GAME_SERVER_GAMEMODES_LMS_H
#include <game/server/gamecontroller.h>

class CGameControllerLMS : public IGameController
{
private:
	int m_SpawnArmor;

public:
	CGameControllerLMS();

	// event
	virtual void OnWorldReset() override;
	virtual void OnCharacterSpawn(class CCharacter *pChr) override;
	virtual bool OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number) override;
	// game
	virtual void DoWincheckRound() override;
};

#endif
