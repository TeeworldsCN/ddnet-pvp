#ifndef GAME_SERVER_GAMEMODES_BD_H
#define GAME_SERVER_GAMEMODES_BD_H
#include <game/server/gamecontroller.h>

class CGameControllerBD : public IGameController
{
public:
	CGameControllerBD();

	int RoundOver;
	class CCharacter *pChr;
	CPlayer *pPlayer;

	virtual void DoWincheckMatch() override;
	void StartRoundBD();
    virtual void OnPreTick() override;
    
	// event
	virtual void OnCharacterSpawn(class CCharacter *pChr) override;
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;

	int NumZombies();
	int NumHumans();
	int NumPlayers();
};

#endif // GAME_SERVER_GAMEMODES_BD_H
