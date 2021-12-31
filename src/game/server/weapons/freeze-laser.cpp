#include "freeze-laser.h"
#include <game/generated/server_data.h>
#include <game/server/entities/laser.h>

bool CFreezeLaser::LaserHit(CLaser *pLaser, vec2 HitPoint, CCharacter *pHit, bool OutOfEnergy)
{
	if(pHit)
	{
		if(pHit->GetPlayer()->GetCID() == pLaser->GetOwner())
			return false;

		pHit->Freeze(3.0f);
		return true;
	}

	return false;
}

void CFreezeLaser::Fire(vec2 Direction)
{
	int ClientID = Character()->GetPlayer()->GetCID();

	new CLaser(
		GameWorld(),
		WEAPON_GUN, //Type
		GetWeaponID(), //WeaponID
		ClientID, //Owner
		Pos(), //Pos
		Direction, //Dir
		g_pData->m_Weapons.m_Laser.m_Reach, // StartEnergy
		CFreezeLaser::LaserHit);

	GameWorld()->CreateSound(Character()->m_Pos, SOUND_LASER_FIRE);
}