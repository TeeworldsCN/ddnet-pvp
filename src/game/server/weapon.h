#ifndef GAME_SERVER_WEAPON_H
#define GAME_SERVER_WEAPON_H

#include <game/server/entities/character.h>
#include <game/server/player.h>

class CWeapon
{
private:
	CCharacter *m_pOwnerChar;
	class CGameContext *m_pGameServer;
	class CGameWorld *m_pGameWorld;
	class IServer *m_pServer;
	int m_WeaponTypeID;

protected:
	int m_Ammo;
	int m_MaxAmmo;
	int m_FireDelay;
	int m_AmmoRegenStart;
	int m_AmmoRegenTime;
	bool m_FullAuto;
	int m_AttackTick;
	int m_LastNoAmmoSound;
	int m_WeaponAquiredTick;
	int m_ReloadTimer;

	virtual void Fire(vec2 Direction) = 0;

public:
	CWeapon(CCharacter *pOwnerChar);
	virtual ~CWeapon(){};

	void HandleFire(vec2 Direction);

	virtual void Tick();
	virtual void TickPaused();

	CCharacter *Character() { return m_pOwnerChar; }
	class CGameContext *GameServer() { return m_pGameServer; }
	class CGameWorld *GameWorld() { return m_pGameWorld; }
	class IServer *Server() { return m_pServer; }
	vec2 Pos();
	float GetProximityRadius();

	bool IsFullAuto() { return m_FullAuto; }
	void SetAmmo(int Ammo) { m_Ammo = Ammo; }
	int GetAmmo() { return m_Ammo; }
	int GetMaxAmmo() { return m_MaxAmmo; }
	int GetAttackTick() { return m_AttackTick; }
	bool IsReloading() { return m_ReloadTimer != 0; };
	void Reload() { m_ReloadTimer = 0; };

	void SetTypeID(int Type) { m_WeaponTypeID = Type; }
	int GetTypeID() { return m_WeaponTypeID; }

	// called when equip, you can allocate snap ids here
	virtual void OnEquip(){};
	// called when unequip, you can free snap ids here, but you should also free them in destructor
	virtual void OnUnequip(){};
	// how many ammo are shown to the client
	virtual int NumAmmoIcons() { return clamp(m_Ammo, 0, 10); }
	// whether the powerup has ended, normal weapons as power up will end instantly by default
	virtual bool IsPowerupOver() { return true; }
	// custom snap that snaps with character
	virtual void Snap(int SnappingClient, int OtherMode){};
	// weapon type shown to the client
	virtual int GetType() { return WEAPON_HAMMER; }
};

#endif // GAME_SERVER_WEAPON_H