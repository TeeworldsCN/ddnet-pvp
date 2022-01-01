#include "infection-hammer.h"
#include <game/generated/server_data.h>
#include <game/server/entities/projectile.h>
#include <game/server/ModCore/BlockDown/player.h>

void CInfHammer::Fire(vec2 Direction)
{
	int ClientID = Character()->GetPlayer()->GetCID();
	GameWorld()->CreateSound(Pos(), SOUND_HAMMER_FIRE);

	GameServer()->Antibot()->OnHammerFire(ClientID);

	if(Character()->IsSolo() || Character()->m_Hit & CCharacter::DISABLE_HIT_HAMMER)
		return;

	vec2 HammerHitPos = Pos() + Direction * GetProximityRadius() * 0.75f;

	CCharacter *apEnts[MAX_CLIENTS];
	int Hits = 0;
	int Num = GameWorld()->FindEntities(HammerHitPos, GetProximityRadius() * 0.5f, (CEntity **)apEnts,
		MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

	for(int i = 0; i < Num; ++i)
	{
		CCharacter *pTarget = apEnts[i];

		//if ((pTarget == this) || GameServer()->Collision()->IntersectLine(ProjStartPos, pTarget->m_Pos, NULL, NULL))
		if((pTarget == Character() || (pTarget->IsAlive() && pTarget->IsSolo())))
			continue;

		// set his velocity to fast upward (for now)
		if(length(pTarget->m_Pos - HammerHitPos) > 0.0f)
			GameWorld()->CreateHammerHit(pTarget->m_Pos - normalize(pTarget->m_Pos - HammerHitPos) * GetProximityRadius() * 0.5f);
		else
			GameWorld()->CreateHammerHit(HammerHitPos);

		vec2 Dir;
		if(length(pTarget->m_Pos - Pos()) > 0.0f)
			Dir = normalize(pTarget->m_Pos - Pos());
		else
			Dir = vec2(0.f, -1.f);

		float Strength = Character()->CurrentTuning()->m_HammerStrength;

		vec2 Temp = pTarget->Core()->m_Vel + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f;
		Temp = ClampVel(pTarget->m_MoveRestrictions, Temp);
		Temp -= pTarget->Core()->m_Vel;

		pTarget->TakeDamage((vec2(0.f, -1.0f) + Temp) * Strength, 10,
			ClientID, WEAPON_HAMMER, GetWeaponID(), false);
		GameServer()->Antibot()->OnHammerHit(ClientID);

		Hits++;
	}

	// if we Hit anything, we have to wait for the reload
	if(Hits)
	{
		float FireDelay;
		int TuneZone = Character()->m_TuneZone;
		if(!TuneZone)
			FireDelay = GameServer()->Tuning()->m_HammerHitFireDelay;
		else
			FireDelay = GameServer()->TuningList()[TuneZone].m_HammerHitFireDelay;
		m_ReloadTimer = FireDelay * Server()->TickSpeed() / 1000;
	}
}