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
		int WeaponType,
		int WeaponID,
		int Owner,
		vec2 Pos,
		vec2 Direction,
		float HitRadius,
		int LifeSpan,
		FProjectileImpactCallback Callback = nullptr,
		SEntityCustomData CustomData = {nullptr, nullptr},
		int Layer = 0,
		int Number = 0);
	~CProjectile();

	void GetProjectileProperties(float *pCurvature, float *pSpeed);
	vec2 GetPos(float Time);
	void FillInfo(CNetObj_Projectile *pProj);

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void TickPaused() override;
	virtual bool NetworkClipped(int SnappingClient) override;
	virtual void Snap(int SnappingClient, int OtherMode) override;
	virtual void Destroy() override;

private:
	vec2 m_Direction;
	int m_TotalLifeSpan;
	int m_Owner;
	int m_StartTick;
	int m_WeaponID;
	FProjectileImpactCallback m_Callback;
	int m_ID;

	// DDRace
	int m_TuneZone; //TODO: make curvature and property

	// Hitdata
	int64 m_HitMask;
	int m_OwnerIsSafe;
	int m_NumHits;

	SEntityCustomData m_CustomData;

public:
	int m_Type;
	float m_Radius;
	int m_LifeSpan;

	// DDRace
	int m_Hit;
	bool m_IsSolo;
	int m_Bouncing;
	int GetOwner() { return m_Owner; }
	int GetWeaponID() { return m_WeaponID; }
	void SetBouncing(int Value);
	bool FillExtraInfo(CNetObj_DDNetProjectile *pProj);

	void *GetCustomData() { return m_CustomData.m_pData; }
};

#endif
