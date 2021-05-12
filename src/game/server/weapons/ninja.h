#ifndef GAME_SERVER_WEAPONS_NINJA_H
#define GAME_SERVER_WEAPONS_NINJA_H

#include <game/server/weapon.h>

class CNinja : public CWeapon
{
private:
	vec2 m_ActivationDir;
	int m_CurrentMoveTime;
	float m_OldVelAmount;
	int m_NumObjectsHit;
	class CCharacter *m_apHitObjects[MAX_CLIENTS];

public:
	CNinja(CCharacter *pOwnerChar);

	void Tick() override;
	void Fire(vec2 Direction) override;
	bool IsPowerupOver() override;
	void OnUnequip() override;
	int GetType() override { return WEAPON_NINJA; }
};

#endif // GAME_SERVER_WEAPONS_NINJA_H
