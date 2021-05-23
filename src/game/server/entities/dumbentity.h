#ifndef GAME_SERVER_ENTITIES_DUMBENTITY_H
#define GAME_SERVER_ENTITIES_DUMBENTITY_H

#include <game/server/entity.h>

class CDumbEntity : public CEntity
{
private:
	int m_Type;
	vec2 m_PrevPos;
	vec2 m_PrevPrevPos;
	vec2 m_Velocity;
	vec2 m_PrevVelocity;
	vec2 m_LaserVector;
	int m_ID;

	void GetProjectileProperties(float *pCurvature, float *pSpeed, int TuneZone = 0);

public:
	enum
	{
		MASK_TYPE = (1 << 16) - 1,

		TYPE_HEART = 0,
		TYPE_ARMOR,
		TYPE_PICKUP_HAMMER,
		TYPE_PICKUP_GUN,
		TYPE_PICKUP_SHOTGUN,
		TYPE_PICKUP_GRENADE,
		TYPE_PICKUP_LASER,
		TYPE_PICKUP_NINJA, // same as TYPE_POWERUP_NINJA for 0.7
		TYPE_POWERUP_NINJA,
		TYPE_PROJECTILE_GUN,
		TYPE_PROJECTILE_SHOTGUN,
		TYPE_PROJECTILE_GRENADE,
		TYPE_LASER,

		// The entity will be destroy when GameWorld resets
		FLAG_CLEAR_ON_RESET = 1 << 16,

		// The entity will send the latest location, only affects projectiles
		FLAG_IMMEDIATE = 1 << 17,

		// The entity will send velocity 0 instead of calculating it from delta, only affects projectiles
		FLAG_NO_VELOCITY = 1 << 18,

		// Laser entity will use line to indicate delta/velocity, LaserVector property will be discarded.
		FLAG_LASER_VELOCITY = 1 << 19,

		// The entity won't be inserted into GameWorld, Tick and Snap won't be called automatically, won't request SnapID on creation. Ideal for custom behavior such as in weapons.
		FLAG_MANUAL = 1 << 20,
	};

	// Let CGameWorld handle snaps
	CDumbEntity(CGameWorld *pGameWorld, int Type, vec2 Pos, vec2 To = {0.0f, 0.0f});
	~CDumbEntity();

	virtual void Reset() override;
	virtual void Tick() override;
	virtual bool NetworkClipped(int SnappingClient) override;
	virtual void Snap(int SnappingClient, int OtherMode) override;

	// Moving
	void MoveTo(vec2 Pos);
	void TeleportTo(vec2 Pos);

	// Laser
	void SetLaserVector(vec2 Vector);

	// Custom Snap, pass SnapID from outside world
	void DoSnap(int SnapID, int SnappingClient);
};

#endif // GAME_SERVER_ENTITIES_DUMBENTITY_H
