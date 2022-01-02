#ifndef GAME_SERVER_WEAPONS_FREEZELASER_H
#define GAME_SERVER_WEAPONS_FREEZELASER_H

#include "lasergun.h"

/* Laser with explosions */
class CFreezeLaser : public CLaserGun
{

public:
	CFreezeLaser(CCharacter *pOwnerChar) :
		CLaserGun(pOwnerChar) 
		{
			m_AmmoRegenDelay = 50;
			m_FireDelay = 1000;
			m_FullAuto = false;
			m_Ammo = 10;
		}

	void Fire(vec2 Direction) override;

	// callback
	static bool LaserHit(class CLaser *pLaser, vec2 HitPoint, CCharacter *pHit, bool OutOfEnergy);
};

#endif // GAME_SERVER_WEAPONS_FREEZELASER_H
