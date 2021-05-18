#ifndef GAME_SERVER_WEAPONS_EXPLODINGLASER_H
#define GAME_SERVER_WEAPONS_EXPLODINGLASER_H

#include "lasergun.h"

/* Laser with explosions */
class CExplodingLaser : public CLaserGun
{
private:
	int m_MaxExplosions;

public:
	CExplodingLaser(CCharacter *pOwnerChar) :
		CLaserGun(pOwnerChar) { m_MaxExplosions = 1; }

	void Fire(vec2 Direction) override;
	void SetMaxExplosions(bool Enabled) { m_MaxExplosions = Enabled; }

	// callback
	static bool LaserHit(class CLaser *pLaser, vec2 HitPoint, CCharacter *pHit, bool OutOfEnergy);
};

#endif // GAME_SERVER_WEAPONS_EXPLODINGLASER_H
