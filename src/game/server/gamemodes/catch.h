#ifndef GAME_SERVER_GAMEMODES_CATCH_H
#define GAME_SERVER_GAMEMODES_CATCH_H

#include <game/server/gamecontroller.h>

class CGameControllerCatch : public IGameController
{
private:
	// states
	int m_aCatchedBy[MAX_CLIENTS];
	int m_WinnerBonus;

public:
	CGameControllerCatch();

	void Catch(class CPlayer *pVictim, class CPlayer *pBy);
	void Release(class CPlayer *pPlayer);

	// event
	virtual void OnWorldReset() override;
	virtual void OnPlayerJoin(class CPlayer *pPlayer) override;
	virtual void OnCharacterSpawn(class CCharacter *pChr) override;
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	virtual bool CanDeadPlayerFollow(const class CPlayer *pSpectator, const class CPlayer *Target) override;
	virtual void DoWincheckMatch() override;
};

#endif // GAME_SERVER_GAMEMODES_CATCH_H
