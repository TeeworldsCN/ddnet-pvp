#ifdef REGISTER_WEAPON

REGISTER_WEAPON(WEAPON_ID_HAMMER, CHammer)
REGISTER_WEAPON(WEAPON_ID_PISTOL, CPistol)
REGISTER_WEAPON(WEAPON_ID_SHOTGUN, CShotgun)
REGISTER_WEAPON(WEAPON_ID_GRENADE, CGrenade)
REGISTER_WEAPON(WEAPON_ID_LASER, CLaserGun)
REGISTER_WEAPON(WEAPON_ID_NINJA, CNinja)

REGISTER_WEAPON(WEAPON_ID_EXPLODINGLASER, CExplodingLaser)
REGISTER_WEAPON(WEAPON_ID_FREEZELASER, CFreezeLaser)
REGISTER_WEAPON(WEAPON_ID_INFHAMMER, CInfHammer)
REGISTER_WEAPON(WEAPON_ID_USELESSGUN, CULPistol)

#else

#ifndef GAME_SERVER_WEAPONS_H
#define GAME_SERVER_WEAPONS_H

#include "weapons/grenade.h"
#include "weapons/hammer.h"
#include "weapons/lasergun.h"
#include "weapons/ninja.h"
#include "weapons/pistol.h"
#include "weapons/shotgun.h"

#include "weapons/explodinglaser.h"
#include "weapons/freeze-laser.h"
#include "weapons/infection-hammer.h"
#include "weapons/useless-gun.h"

enum
{
	// DDrace
	WEAPON_ID_DDRACE = -2,
	WEAPON_ID_WORLD = -1,
	WEAPON_ID_NONE = 0,
#define REGISTER_WEAPON(TYPE, CLASS) \
	TYPE,
#include <game/server/weapons.h>
#undef REGISTER_WEAPON
};

#endif // GAME_SERVER_WEAPONS_H

#endif // REGISTER_WEAPON