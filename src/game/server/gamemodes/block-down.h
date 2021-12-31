#ifndef GAME_SERVER_GAMEMODES_BD_H
#define GAME_SERVER_GAMEMODES_BD_H
#include <game/server/gamecontroller.h>

class CGameControllerBD : public IGameController
{
public:
	CGameControllerBD();

    //virtual void OnPreTick() override;
    
	// event
	virtual void OnCharacterSpawn(class CCharacter *pChr) override;
};

#endif // GAME_SERVER_GAMEMODES_BD_H
