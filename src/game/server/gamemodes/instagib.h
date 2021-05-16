/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_INSTAGIB_H
#define GAME_SERVER_GAMEMODES_INSTAGIB_H

#include <game/server/gamecontroller.h>

#include "ctf.h"
#include "dm.h"
#include "tdm.h"

template<class T>
class CGameControllerInstagib : public T
{
private:
	int m_KillDamage;
	int m_StartWeapon;
	int m_WeaponMaxAmmo;
	int m_WeaponAmmoRegenTime;
	int m_WeaponAmmoRegenOnBoost;
	int m_WeaponEmptyReloadPenalty;
	int m_LaserJump;

public:
	CGameControllerInstagib();
	void RegisterConfig();

	// event
	virtual void OnCharacterSpawn(class CCharacter *pChr) override;
	virtual int OnCharacterTakeDamage(class CCharacter *pChr, vec2 &Force, int &Dmg, int From, int Weapon, int WeaponType, bool IsExplosion) override;
	virtual bool OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number) override;
};

#endif // GAME_SERVER_GAMEMODES_INSTAGIB_H
