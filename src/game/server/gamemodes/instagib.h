#ifndef GAME_SERVER_GAMEMODES_INSTAGIB_H
#define GAME_SERVER_GAMEMODES_INSTAGIB_H

#include <game/server/gamecontroller.h>

#include <game/server/entities/character.h>
#include <game/server/weapons.h>

#include "ctf.h"
#include "dm.h"
#include "tdm.h"

template<class T>
class CGameControllerInstagib : public T
{
private:
	int m_KillDamage;
	int m_StartWeapon;
	int m_WeaponMaxAmmo;
	int m_WeaponAmmoRegenTime;
	int m_WeaponAmmoRegenOnBoost;
	int m_WeaponAmmoRegenDelay;
	int m_WeaponEmptyReloadPenalty;
	int m_LaserJump;

public:
	CGameControllerInstagib();

	static void ConchainUpdateLaserJumps(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
	{
		pfnCallback(pResult, pCallbackUserData);
		if(pResult->NumArguments() >= 1)
		{
			IGameController *pThis = static_cast<IGameController *>(pUserData);
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				CPlayer *pPlayer = pThis->GetPlayerIfInRoom(i);
				if(pPlayer && pPlayer->GetCharacter())
				{
					CWeapon *pWeapon = pPlayer->GetCharacter()->GetWeapon(WEAPON_LASER);
					if(pWeapon && pWeapon->GetWeaponID() == WEAPON_ID_EXPLODINGLASER)
						((CExplodingLaser *)pWeapon)->SetMaxExplosions(pResult->GetInteger(0));
				}
			}
		}
	}

	void RegisterConfig()
	{
		INSTANCE_CONFIG_INT(&m_KillDamage, "kill_damage", 4, 1, 6, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Minimum damage required for a grenade kill");
		INSTANCE_CONFIG_INT(&m_StartWeapon, "weapon", 4, 0, 5, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Start weapon (0=hammer,1=gun,2=shotgun,3=genrade,4=laser,5=ninja)");
		INSTANCE_CONFIG_INT(&m_WeaponMaxAmmo, "max_ammo", -1, -1, 10, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Maximum amount of ammo (-1 = infinite and no regen)");
		INSTANCE_CONFIG_INT(&m_WeaponAmmoRegenTime, "ammo_regen_time", 128, 1, 2000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Ammo regen interval (in milliseconds)");
		INSTANCE_CONFIG_INT(&m_WeaponAmmoRegenOnBoost, "ammo_regen_boost", 1, 0, 1, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Whether rocket jump or laser jump gives back ammo");
		INSTANCE_CONFIG_INT(&m_WeaponAmmoRegenDelay, "ammo_regen_delay", 256, 0, 2000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Time of delay after weapon reload and before regen starts");
		INSTANCE_CONFIG_INT(&m_WeaponEmptyReloadPenalty, "empty_penalty", 1000, 0, 2000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Time to reload after firing the last ammo (in milliseconds)");
		INSTANCE_CONFIG_INT(&m_LaserJump, "laser_jump", 0, 0, 1, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Whether laser jump is enabled");

		IGameController::InstanceConsole()->Chain("laser_jump", ConchainUpdateLaserJumps, this);
	};

	// event
	virtual void OnCharacterSpawn(class CCharacter *pChr) override
	{
		T::OnCharacterSpawn(pChr);

		// remove spawn weapons
		pChr->RemoveWeapons();

		CWeapon *pStartingWeapon = nullptr;

		switch(m_StartWeapon)
		{
		case WEAPON_HAMMER:
			pChr->GiveWeapon(WEAPON_LASER, WEAPON_ID_HAMMER, -1);
			break;
		case WEAPON_GUN:
			pChr->GiveWeapon(WEAPON_LASER, WEAPON_ID_PISTOL, -1);
			break;
		case WEAPON_SHOTGUN:
			pChr->GiveWeapon(WEAPON_LASER, WEAPON_ID_SHOTGUN, -1);
			break;
		case WEAPON_GRENADE:
			pChr->GiveWeapon(WEAPON_LASER, WEAPON_ID_GRENADE, -1);
			break;
		case WEAPON_NINJA:
			pChr->GiveWeapon(WEAPON_LASER, WEAPON_ID_NINJA, -1);
			break;
		default:
			pChr->GiveWeapon(WEAPON_LASER, WEAPON_ID_EXPLODINGLASER, -1);
			break;
		}

		pStartingWeapon = pChr->GetWeapon(WEAPON_LASER);
		if(pStartingWeapon && m_WeaponMaxAmmo > -1)
		{
			pStartingWeapon->SetMaxAmmo(m_WeaponMaxAmmo);
			pStartingWeapon->SetAmmo(m_WeaponMaxAmmo);
			pStartingWeapon->SetAmmoRegenTime(m_WeaponAmmoRegenTime);
			pStartingWeapon->SetEmptyReloadPenalty(m_WeaponEmptyReloadPenalty);
			pStartingWeapon->SetAmmoRegenDelay(m_WeaponAmmoRegenDelay);
		}

		if(pStartingWeapon && pStartingWeapon->GetWeaponID() == WEAPON_ID_EXPLODINGLASER)
			((CExplodingLaser *)pStartingWeapon)->SetMaxExplosions(m_LaserJump);
	};

	virtual int OnCharacterTakeDamage(class CCharacter *pChr, vec2 &Force, int &Dmg, int From, int Weapon, int WeaponType, bool IsExplosion) override
	{
		// ignore invalid damage (i.e. friendly fire)
		if(Dmg <= 0)
			return DAMAGE_SKIP;

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

		// laser jump should only affect owner
		if(WeaponType == WEAPON_ID_EXPLODINGLASER && IsExplosion)
			return DAMAGE_SKIP;

		// keep force but no kill
		if(WeaponType == WEAPON_ID_GRENADE && Dmg < m_KillDamage)
			return DAMAGE_NO_DAMAGE | DAMAGE_NO_INDICATOR | DAMAGE_NO_HITSOUND;

		pChr->Die(From, Weapon);
		return DAMAGE_DIED | DAMAGE_NO_INDICATOR | DAMAGE_NO_PAINSOUND;
	};

	virtual bool OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number) override
	{
		if(T::OnEntity(Index, Pos, Layer, Flags, Number))
			return true;

		// bypass pickups
		if(Index >= ENTITY_ARMOR_1 && Index <= ENTITY_WEAPON_LASER)
			return true;
		return false;
	};
};

typedef CGameControllerInstagib<CGameControllerDM> CGameControllerIDM;
typedef CGameControllerInstagib<CGameControllerTDM> CGameControllerITDM;
typedef CGameControllerInstagib<CGameControllerCTF> CGameControllerICTF;

#endif // GAME_SERVER_GAMEMODES_INSTAGIB_H
