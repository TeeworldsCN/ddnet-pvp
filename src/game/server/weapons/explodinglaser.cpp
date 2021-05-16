
#include "explodinglaser.h"
#include <game/generated/server_data.h>
#include <game/server/entities/laser.h>

bool CExplodingLaser::LaserHit(CLaser *pLaser, vec2 HitPoint, CCharacter *pHit, bool OutOfEnergy)
{
	if(!pHit && pLaser->GetBounces() <= *(int *)pLaser->GetCustomData())
		pLaser->GameWorld()->CreateExplosion(HitPoint, pLaser->GetOwner(), WEAPON_LASER, pLaser->GetWeaponID(), g_pData->m_Weapons.m_aId[WEAPON_GRENADE].m_Damage, false);

	return CLaserGun::LaserHit(pLaser, HitPoint, pHit, OutOfEnergy);
}

void CExplodingLaser::Fire(vec2 Direction)
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
		CExplodingLaser::LaserHit,
		new int(m_MaxExplosions));

	GameWorld()->CreateSound(Character()->m_Pos, SOUND_LASER_FIRE);
}