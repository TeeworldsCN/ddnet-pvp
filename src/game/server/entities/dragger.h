/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_ENTITIES_DRAGGER_H
#define GAME_SERVER_ENTITIES_DRAGGER_H

#include <game/server/entity.h>
class CCharacter;

class CDragger : public CEntity
{
	vec2 m_Core;
	float m_Strength;
	int m_EvalTick;
	void Move();
	void Drag();
	CCharacter *m_Target;
	bool m_NW;

	CCharacter *m_SoloEnts[MAX_CLIENTS];
	int m_SoloIDs[MAX_CLIENTS];

	int m_ID;

public:
	CDragger(CGameWorld *pGameWorld, vec2 Pos, float Strength, bool NW, int Layer = 0, int Number = 0);
	~CDragger();

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient, int OtherMode) override;
};

#endif // GAME_SERVER_ENTITIES_DRAGGER_H
