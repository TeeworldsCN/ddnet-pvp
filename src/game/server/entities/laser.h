/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_LASER_H
#define GAME_SERVER_ENTITIES_LASER_H

#include <game/server/entity.h>

typedef bool (*FLaserImpactCallback)(class CLaser *pLaser, vec2 HitPoint, CCharacter *pHit, bool OutOfEnergy);

class CLaser : public CEntity
{
public:
	CLaser(
		CGameWorld *pGameWorld,
		int WeaponType,
		int Owner,
		vec2 Pos,
		vec2 Direction,
		float StartEnergy,
		FLaserImpactCallback Callback = nullptr);

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient, int OtherMode);

protected:
	bool HitCharacter(vec2 From, vec2 To);
	void DoBounce();

private:
	vec2 m_From;
	vec2 m_Dir;
	vec2 m_TelePos;
	bool m_WasTele;
	int m_Bounces;
	int m_EvalTick;
	int m_Owner;
	FLaserImpactCallback m_Callback;

	// DDRace

	int m_Hit;
	bool m_IsSolo;
	vec2 m_PrevPos;
	int m_Type;
	int m_TuneZone;

	// Hitdata
	int64 m_HitMask;
	int m_OwnerIsSafe;
	int m_NumHits;

public:
	int GetOwner() { return m_Owner; }
	float m_Energy;
};

#endif
