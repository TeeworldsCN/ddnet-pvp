#ifndef GAME_SERVER_WEAPONS_FREEZELASER_H
#define GAME_SERVER_WEAPONS_FREEZELASER_H

#include "lasergun.h"

/* Laser with explosions */
class CFreezeLaser : public CLaserGun
{

public:
	CFreezeLaser(CCharacter *pOwnerChar) :
		CLaserGun(pOwnerChar) { }

	void Fire(vec2 Direction) override;

	// callback
	static bool LaserHit(class CLaser *pLaser, vec2 HitPoint, CCharacter *pHit, bool OutOfEnergy);
};

#endif // GAME_SERVER_WEAPONS_FREEZELASER_H
