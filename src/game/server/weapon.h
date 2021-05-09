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
	int m_AmmoRegenStart;
	int m_AmmoRegenTime;
	bool m_FullAuto;

	int m_ReloadTimer;

	virtual void Fire(vec2 Direction) = 0;

public:
	CWeapon(CCharacter *pOwnerChar);

	void HandleFire(vec2 Direction);

	virtual void Tick();
	CCharacter *Character() { return m_pOwnerChar; }
	class CGameContext *GameServer() { return m_pGameServer; }
	class CGameWorld *GameWorld() { return m_pGameWorld; }
	class IServer *Server() { return m_pServer; }
	vec2 Pos();
	float GetProximityRadius();

	virtual void OnEquip(){};
	virtual void OnUnequip(){};

	// custom snap that snaps with character
	virtual void Snap(int SnappingClient, int OtherMode){};
	virtual int GetType() { return WEAPON_HAMMER; }

	virtual bool IsFullAuto() { return m_FullAuto; }

	void SetAmmo(int Ammo) { m_Ammo = Ammo; }
	int GetAmmo() { return m_Ammo; }
	int NumAmmoIcons() { return clamp(m_Ammo, 0, 10); }
	bool IsReloading() { return m_ReloadTimer != 0; };
	void Reload() { m_ReloadTimer = 0; };
	bool IsEmpty() { return m_Ammo == 0; };

	void SetTypeID(int Type) { m_WeaponTypeID = Type; }
	int GetTypeID() { return m_WeaponTypeID; }
};

#endif // GAME_SERVER_WEAPON_H