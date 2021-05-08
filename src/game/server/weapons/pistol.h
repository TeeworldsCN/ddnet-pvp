#ifndef GAME_SERVER_WEAPONS_PITOL_H
#define GAME_SERVER_WEAPONS_PITOL_H

#include <game/server/weapon.h>

class CPistol : public CWeapon
{
public:
	CPistol(CCharacter *pOwnerChar);

	void Fire(vec2 Direction) override;
	int GetType() override { return WEAPON_GUN; }
};

#endif // GAME_SERVER_WEAPONS_PITOL_H
