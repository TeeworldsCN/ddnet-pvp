#ifndef GAME_SERVER_WEAPONS_INFHAMMER_H
#define GAME_SERVER_WEAPONS_INFHAMMER_H

#include <game/server/weapons/hammer.h>

class CInfHammer : public CHammer
{
public:
	CInfHammer(CCharacter *pOwnerChar) :
        CHammer(pOwnerChar) 
        { 
            m_FullAuto = true;
            m_FireDelay = 100;
        }

	void Fire(vec2 Direction) override;
	int GetType() override { return WEAPON_HAMMER; }
};

#endif // GAME_SERVER_WEAPONS_INFHAMMER_H
