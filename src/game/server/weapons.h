#ifdef REGISTER_WEAPON

REGISTER_WEAPON(WEAPON_TYPE_HAMMER, CHammer)
REGISTER_WEAPON(WEAPON_TYPE_PISTOL, CPistol)
REGISTER_WEAPON(WEAPON_TYPE_SHOTGUN, CShotgun)
REGISTER_WEAPON(WEAPON_TYPE_GRENADE, CGrenade)
REGISTER_WEAPON(WEAPON_TYPE_LASER, CLaserGun)

#else

#ifndef GAME_SERVER_WEAPONS_H
#define GAME_SERVER_WEAPONS_H

#include "weapons/grenade.h"
#include "weapons/hammer.h"
#include "weapons/lasergun.h"
#include "weapons/pistol.h"
#include "weapons/shotgun.h"

enum
{
	WEAPON_TYPE_NONE = 0,
#define REGISTER_WEAPON(TYPE, CLASS) \
	TYPE,
#include <game/server/weapons.h>
#undef REGISTER_WEAPON
};

#endif // GAME_SERVER_WEAPONS_H

#endif // REGISTER_WEAPON