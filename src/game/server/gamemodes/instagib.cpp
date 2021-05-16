/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "instagib.h"

#include <game/server/entities/character.h>
#include <game/server/weapons.h>

template<class T>
void CGameControllerInstagib<T>::RegisterConfig()
{
	INSTANCE_CONFIG_INT(&m_KillDamage, "kill_damage", 4, 1, 6, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Minimum damage required for a kill");
	INSTANCE_CONFIG_INT(&m_StartWeapon, "grenade", 0, 0, 1, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Start weapon (0 = laser, 1 = grenade)");
	INSTANCE_CONFIG_INT(&m_WeaponMaxAmmo, "max_ammo", -1, -1, 10, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Maximum amount of ammo (-1 = infinite and no regen)");
	INSTANCE_CONFIG_INT(&m_WeaponAmmoRegenTime, "ammo_regen_time", 128, 1, 2000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Ammo regen interval (in milliseconds)");
	INSTANCE_CONFIG_INT(&m_WeaponAmmoRegenOnBoost, "ammo_regen_boost", 1, 0, 1, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Whether rocket jump or laser jump gives back ammo");
	INSTANCE_CONFIG_INT(&m_WeaponEmptyReloadPenalty, "empty_penalty", 1000, 0, 2000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Time to reload after firing the last ammo (in milliseconds)");
	INSTANCE_CONFIG_INT(&m_LaserJump, "laser_jump", 0, 0, 1, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Whether laser jump is enabled");
}

template<class T>
void CGameControllerInstagib<T>::OnCharacterSpawn(CCharacter *pChr)
{
	pChr->IncreaseHealth(10);

	CWeapon *pStartingWeapon = nullptr;

	if(m_StartWeapon)
		pChr->GiveWeapon(WEAPON_LASER, WEAPON_ID_GRENADE, -1);
	else
		pChr->GiveWeapon(WEAPON_LASER, WEAPON_ID_EXPLODINGLASER, -1);

	pStartingWeapon = pChr->GetWeapon(WEAPON_LASER);
	if(pStartingWeapon && m_WeaponMaxAmmo > -1)
	{
		pStartingWeapon->SetMaxAmmo(m_WeaponMaxAmmo);
		pStartingWeapon->SetAmmo(m_WeaponMaxAmmo);
		pStartingWeapon->SetAmmoRegenTime(m_WeaponAmmoRegenTime);
		pStartingWeapon->SetEmptyReloadPenalty(m_WeaponEmptyReloadPenalty);
	}

	if(pStartingWeapon && pStartingWeapon->GetWeaponID() == WEAPON_ID_EXPLODINGLASER)
		((CExplodingLaser *)pStartingWeapon)->SetMaxExplosions(m_LaserJump);
}

template<class T>
int CGameControllerInstagib<T>::OnCharacterTakeDamage(CCharacter *pChr, vec2 &Force, int &Dmg, int From, int Weapon, int WeaponType, bool IsExplosion)
{
	// no self damage
	if(pChr->GetPlayer()->GetCID() == From)
	{
		if(IsExplosion)
		{
			if(m_WeaponAmmoRegenOnBoost && length(Force) > 11.0f)
			{
				CWeapon *pWeapon = pChr->GetWeapon(WEAPON_LASER);
				if(pWeapon)
					pWeapon->GiveAmmo(1);
			}

			return DAMAGE_NO_DAMAGE | DAMAGE_NO_INDICATOR | DAMAGE_NO_PAINSOUND;
		}
		return DAMAGE_NO_DAMAGE | DAMAGE_NO_INDICATOR;
	}

	if((WeaponType == WEAPON_ID_EXPLODINGLASER && !IsExplosion) || (WeaponType == WEAPON_ID_GRENADE && Dmg >= m_KillDamage))
	{
		pChr->Die(From, Weapon);
		return DAMAGE_DIED | DAMAGE_NO_INDICATOR | DAMAGE_NO_PAINSOUND;
	}

	// laser jump should only affect owner
	if(WeaponType == WEAPON_ID_EXPLODINGLASER && IsExplosion)
		return DAMAGE_SKIP;

	return DAMAGE_NO_DAMAGE | DAMAGE_NO_INDICATOR | DAMAGE_NO_HITSOUND;
}

template<class T>
bool CGameControllerInstagib<T>::OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number)
{
	if(T::OnEntity(Index, Pos, Layer, Flags, Number))
		return true;

	// bypass pickups
	if(Index >= ENTITY_ARMOR_1 && Index <= ENTITY_WEAPON_LASER)
		return true;
	return false;
}

CGameControllerInstagib<CGameControllerDM>::CGameControllerInstagib() :
	CGameControllerDM()
{
	m_pGameType = "iDM";
	RegisterConfig();
}

CGameControllerInstagib<CGameControllerTDM>::CGameControllerInstagib() :
	CGameControllerTDM()
{
	m_pGameType = "iTDM";
	RegisterConfig();
}

CGameControllerInstagib<CGameControllerCTF>::CGameControllerInstagib() :
	CGameControllerCTF()
{
	m_pGameType = "iCTF";
	RegisterConfig();
}