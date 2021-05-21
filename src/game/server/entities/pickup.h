/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_PICKUP_H
#define GAME_SERVER_ENTITIES_PICKUP_H

#include <game/server/entity.h>

const int PickupPhysSize = 14;

struct SPickupSound
{
	int m_Sound;
	bool m_Global;
};

class CPickup : public CEntity
{
public:
	CPickup(CGameWorld *pGameWorld, int Type, int SubType = 0);
	~CPickup();

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient, int OtherMode);

	int GetType() { return m_Type; }
	int GetSubtype() { return m_Subtype; }

private:
	int m_Type;
	int m_Subtype;
	int m_SpawnTick; // for team
	int m_SoloSpawnTick[MAX_CLIENTS]; // for solo

	int m_ID;
	// DDRace
	void Move();
	vec2 m_Core;
};

#endif
