#ifndef GAME_SERVER_MODCORE_BD_PLAYER
#define GAME_SERVER_MODCORE_BD_PLAYER

#include <game/server/teeinfo.h>
#include <game/server/entities/character.h>

class CBDPlayer : public CCharacter
{

public:
    class CTeeInfo m_TeeInfos;
    class CCharacter *pChr;

    bool IsZombie(int ClientID);
    void ToHuman(int ClientID);
    void ToZombie(int ClientID);
};

#endif // GAME_SERVER_MODCORE_BD_PLAYER
