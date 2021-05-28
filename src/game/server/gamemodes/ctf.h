/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_CTF_H
#define GAME_SERVER_GAMEMODES_CTF_H
#include <game/server/entity.h>
#include <game/server/gamecontroller.h>

class CGameControllerCTF : public IGameController
{
	// balancing
	virtual bool CanBeMovedOnBalance(CPlayer *pPlayer) const override;

	// game
	class CFlag *m_apFlags[2];
	virtual void DoWincheckMatch() override;

protected:
	// game
	virtual bool GetFlagState(SFlagState *pState) override;

public:
	CGameControllerCTF();

	// event
	virtual void OnInit() override;
	virtual void OnCharacterSpawn(class CCharacter *pChr) override;
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	virtual void OnFlagReset(class CFlag *pFlag) override;
	virtual bool OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number) override;

	// general
	virtual void OnPostTick() override;
};

#endif
