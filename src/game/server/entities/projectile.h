/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_PROJECTILE_H
#define GAME_SERVER_ENTITIES_PROJECTILE_H

#include <game/server/entity.h>

typedef bool (*FProjectileImpactCallback)(class CProjectile *pProj, vec2 Pos, CCharacter *pHit, bool EndOfLife);
class CProjectile : public CEntity
{
public:
	CProjectile(
		CGameWorld *pGameWorld,
		int Type,
		int Owner,
		vec2 Pos,
		vec2 Dir,
		float Radius,
		int Span,
		FProjectileImpactCallback Callback = nullptr,
		int Layer = 0,
		int Number = 0);

	void GetProjectileProperties(float *pCurvature, float *pSpeed);
	vec2 GetPos(float Time);
	void FillInfo(CNetObj_Projectile *pProj);

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient, int OtherMode);

private:
	vec2 m_Direction;
	int m_LifeSpan;
	int m_Owner;
	int m_Type;
	float m_Radius;
	int m_StartTick;
	FProjectileImpactCallback m_Callback;

	// DDRace
	int m_Hit;
	bool m_IsSolo;
	int m_Bouncing;
	int m_TuneZone;

	int64 m_HitMask;

public:
	int GetOwner() { return m_Owner; }
	void SetBouncing(int Value);
	bool FillExtraInfo(CNetObj_DDNetProjectile *pProj);
};

#endif
