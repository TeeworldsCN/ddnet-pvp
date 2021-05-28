#ifndef GAME_SERVER_ENTITIES_TEXTENTITY_H
#define GAME_SERVER_ENTITIES_TEXTENTITY_H

#include <game/server/entity.h>

class CTextEntity : public CEntity
{
private:
	int m_Type;
	vec2 m_PrevPos;
	vec2 m_PrevPrevPos;
	vec2 m_Velocity;
	vec2 m_PrevVelocity;
	int *m_pIDs;
	int m_NumIDs;
	int m_LifeSpan;
	int m_GapSize;
	int m_TextLen;
	float m_BoxWidth;
	float m_BoxHeight;
	vec2 m_Offset;
	char *m_pText;

	void GetProjectileProperties(float *pCurvature, float *pSpeed, int TuneZone = 0);

public:
	enum
	{
		TYPE_HEART = 0,
		TYPE_ARMOR,

		TYPE_GUN,
		TYPE_SHOTGUN,
		TYPE_GRENADE,
		TYPE_LASER,

		ALIGN_LEFT = 0,
		ALIGN_MIDDLE,
		ALIGN_RIGHT,

		SIZE_SMALL = 7,
		SIZE_NORMAL = 10,
		SIZE_LARGE = 15,
		SIZE_XLARGE = 20,
	};

	CTextEntity(CGameWorld *pGameWorld, vec2 Pos, int Type, int GapSize, int Align, char *pText, float Time = -1.0f);
	~CTextEntity();

	virtual void Reset() override;
	virtual void Tick() override;
	virtual bool NetworkClipped(int SnappingClient) override;
	virtual void Snap(int SnappingClient, int OtherMode) override;

	// SnapHelper
	void SnapLaser();
	void SnapProjectile();
	void SnapPickup(int SnappingClient);

	// Moving
	void MoveTo(vec2 Pos);
	void TeleportTo(vec2 Pos);
};

#endif // GAME_SERVER_ENTITIES_TEXTENTITY_H
