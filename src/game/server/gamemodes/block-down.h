#ifndef GAME_SERVER_GAMEMODES_BD_H
#define GAME_SERVER_GAMEMODES_BD_H
#include <game/server/gamecontroller.h>

class CGameControllerBD : public IGameController
{
public:
	CGameControllerBD();

	class CGameContext *m_pGameServer;
	virtual void DoWincheckMatch() override;
    //virtual void OnPreTick() override;
    
	// event
	virtual void OnCharacterSpawn(class CCharacter *pChr) override;
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;

	void DoTeamBalance() override { };
};

#endif // GAME_SERVER_GAMEMODES_BD_H
