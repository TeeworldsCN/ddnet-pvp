#ifndef GAME_SERVER_WEAPONS_USELESSGUN_H
#define GAME_SERVER_WEAPONS_USELESSGUN_H

#include <game/server/weapons/pistol.h>

class CULPistol : public CPistol
{
public:
	CULPistol(CCharacter *pOwnerChar):
        CPistol(pOwnerChar)
        {
            
        }

	void Fire(vec2 Direction) override;
	int GetType() override { return WEAPON_GUN; }

	// callback
	static bool BulletCollide(class CProjectile *pProj, vec2 Pos, CCharacter *pHit, bool EndOfLife);
	static bool BulletCollideTeamDamage(class CProjectile *pProj, vec2 Pos, CCharacter *pHit, bool EndOfLife);
};

#endif // GAME_SERVER_WEAPONS_PITOL_H
