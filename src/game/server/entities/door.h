/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_ENTITIES_DOOR_H
#define GAME_SERVER_ENTITIES_DOOR_H

#include <game/server/entity.h>

class CTrigger;

class CDoor : public CEntity
{
	vec2 m_To;
	int m_EvalTick;
	void ResetCollision();
	int m_Length;
	vec2 m_Direction;
	int m_ID;

public:
	void Open(int Tick, bool ActivatedTeam[]);
	void Open(int Team);
	void Close(int Team);
	CDoor(CGameWorld *pGameWorld, vec2 Pos, float Rotation, int Length,
		int Number);
	~CDoor();

	virtual void Reset() override;
	virtual void Tick() override;
	virtual bool NetworkClipped(int SnappingClient) override;
	virtual void Snap(int SnappingClient, int OtherMode) override;
};

#endif // GAME_SERVER_ENTITIES_DOOR_H
