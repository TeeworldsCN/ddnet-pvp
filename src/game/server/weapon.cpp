#include "weapon.h"

CWeapon::CWeapon(CCharacter *pOwnerChar)
{
	m_pOwnerChar = pOwnerChar;
	m_pGameServer = pOwnerChar->GameServer();
	m_pGameWorld = pOwnerChar->GameWorld();
	m_pServer = pOwnerChar->Server();
	m_Ammo = -1;
	m_MaxAmmo = 10;
	m_FullAuto = false;
	m_AmmoRegenTime = 0;
	m_AmmoRegenStart = 0;
	m_ReloadTimer = 0;
}

void CWeapon::Tick()
{
	if(m_ReloadTimer > 0)
		m_ReloadTimer--;

	// ammo regen
	if(m_AmmoRegenTime)
	{
		if(m_ReloadTimer <= 0)
		{
			if(m_AmmoRegenStart < 0)
				m_AmmoRegenStart = Server()->Tick();

			if((Server()->Tick() - m_AmmoRegenStart) >= m_AmmoRegenTime * Server()->TickSpeed() / 1000)
			{
				// Add some ammo
				m_Ammo = minimum(m_Ammo + 1, m_MaxAmmo);
				m_AmmoRegenStart = -1;
			}
		}
		else
		{
			m_AmmoRegenStart = -1;
		}
	}
}

void CWeapon::HandleFire(vec2 Direction)
{
	if(m_Ammo == 0)
	{
		m_ReloadTimer = 125 * Server()->TickSpeed() / 1000;
		GameWorld()->CreateSound(Pos(), SOUND_WEAPON_NOAMMO);
		return;
	}

	Fire(Direction);
	if(m_Ammo > 0)
		m_Ammo -= 1;
}

vec2 CWeapon::Pos() { return Character()->m_Pos; }
float CWeapon::GetProximityRadius() { return Character()->GetProximityRadius(); }