/* BlockDown Player */
#include "player.h"


bool CBDPlayer::IsZombie(int ClientID)
{
    return GameServer()->m_apPlayers[ClientID]->m_IsZombie;
}

void CBDPlayer::ToHuman(int ClientID)
{
    Die(-1, WEAPON_WORLD);
    pChr->GetPlayer()->m_IsZombie = false;
    m_TeeInfos.m_UseCustomColor = true;
	m_TeeInfos.m_ColorBody = 93866368;
	m_TeeInfos.m_ColorFeet = 965414;
}

void CBDPlayer::ToZombie(int ClientID)
{
    Die(-1, WEAPON_WORLD);
    pChr->GetPlayer()->m_IsZombie = true;
    m_TeeInfos.m_UseCustomColor = true;
    m_TeeInfos.m_ColorBody = 3866368;
	m_TeeInfos.m_ColorFeet = 65414;
}