#include "ninja.h"
#include <game/generated/server_data.h>
#include <game/server/entities/projectile.h>

CNinja::CNinja(CCharacter *pOwnerChar) :
	CWeapon(pOwnerChar)
{
	m_MaxAmmo = g_pData->m_Weapons.m_aId[WEAPON_NINJA].m_Maxammo;
	m_AmmoRegenTime = g_pData->m_Weapons.m_aId[WEAPON_NINJA].m_Ammoregentime;
	m_FireDelay = g_pData->m_Weapons.m_aId[WEAPON_NINJA].m_Firedelay;
	m_OldVelAmount = 0;
	m_CurrentMoveTime = -1;
	m_ActivationDir = vec2(0, 0);
	m_NumObjectsHit = 0;
	m_Duration = g_pData->m_Weapons.m_Ninja.m_Duration;
}

void CNinja::Fire(vec2 Direction)
{
	m_NumObjectsHit = 0;

	m_ActivationDir = Direction;
	m_CurrentMoveTime = g_pData->m_Weapons.m_Ninja.m_Movetime * Server()->TickSpeed() / 1000;
	m_OldVelAmount = length(Character()->Core()->m_Vel);

	GameWorld()->CreateSound(Pos(), SOUND_NINJA_FIRE);
}

void CNinja::Tick()
{
	CWeapon::Tick();

	if(m_CurrentMoveTime >= 0)
		m_CurrentMoveTime--;

	if(m_CurrentMoveTime == 0)
	{
		// reset velocity
		Character()->Core()->m_Vel = m_ActivationDir * m_OldVelAmount;
	}

	if(m_CurrentMoveTime > 0)
	{
		// Set velocity
		Character()->Core()->m_Vel = m_ActivationDir * g_pData->m_Weapons.m_Ninja.m_Velocity;
		vec2 OldPos = Pos();
		GameServer()->Collision()->MoveBox(&Character()->Core()->m_Pos, &Character()->Core()->m_Vel, vec2(GetProximityRadius(), GetProximityRadius()), 0.f);

		// reset velocity so the client doesn't predict stuff
		Character()->Core()->m_Vel = vec2(0.f, 0.f);

		// check if we Hit anything along the way
		{
			CCharacter *aEnts[MAX_CLIENTS];
			vec2 Dir = Pos() - OldPos;
			float Radius = GetProximityRadius() * 2.0f;
			vec2 Center = OldPos + Dir * 0.5f;
			int Num = GameWorld()->FindEntities(Center, Radius, (CEntity **)aEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

			for(int i = 0; i < Num; ++i)
			{
				if(aEnts[i] == Character())
					continue;

				// make sure we haven't Hit this object before
				bool bAlreadyHit = false;
				for(int j = 0; j < m_NumObjectsHit; j++)
				{
					if(m_apHitObjects[j] == aEnts[i])
						bAlreadyHit = true;
				}
				if(bAlreadyHit)
					continue;

				// check so we are sufficiently close
				if(distance(aEnts[i]->m_Pos, Pos()) > (GetProximityRadius() * 2.0f))
					continue;

				// Hit a player, give him damage and stuffs...
				GameWorld()->CreateSound(aEnts[i]->m_Pos, SOUND_NINJA_HIT);
				// set his velocity to fast upward (for now)
				if(m_NumObjectsHit < 10)
					m_apHitObjects[m_NumObjectsHit++] = aEnts[i];
				aEnts[i]->TakeDamage(vec2(0, -10.0f), g_pData->m_Weapons.m_Ninja.m_pBase->m_Damage, Character()->GetPlayer()->GetCID(), WEAPON_NINJA, GetWeaponID(), false);
			}
		}
	}
}

float CNinja::PowerupProgress()
{
	return clamp((Server()->Tick() - m_WeaponAquiredTick) / (float)(m_Duration * Server()->TickSpeed() / 1000), 0.0f, 1.0f);
}

bool CNinja::IsPowerupOver()
{
	return Server()->Tick() - m_WeaponAquiredTick > m_Duration * Server()->TickSpeed() / 1000;
}

void CNinja::OnUnequip()
{
	if(m_CurrentMoveTime > 0)
		Character()->Core()->m_Vel = m_ActivationDir * m_OldVelAmount;
}

void CNinja::OnGiven(bool IsAmmoFillUp)
{
	// refresh timer on pickup
	m_WeaponAquiredTick = Server()->Tick();
}

bool CNinja::IgnoreHookDrag()
{
	return m_CurrentMoveTime >= 0;
}