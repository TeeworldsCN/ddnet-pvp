#ifndef GAME_SERVER_WEAPONS_SHOTGUN_H
#define GAME_SERVER_WEAPONS_SHOTGUN_H

#include <game/server/weapon.h>

class CShotgun : public CWeapon
{
public:
	CShotgun(CCharacter *pOwnerChar);

	void Fire(vec2 Direction) override;
	int GetType() override { return WEAPON_SHOTGUN; }

	// callback
	static bool BulletCollide(class CProjectile *pProj, vec2 Pos, CCharacter *pHit, bool EndOfLife);
};

#endif // GAME_SERVER_WEAPONS_SHOTGUN_H
