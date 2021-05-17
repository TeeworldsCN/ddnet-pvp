/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "dm.h"

#include <game/server/entities/character.h>
#include <game/server/weapons.h>

CGameControllerDM::CGameControllerDM() :
	IGameController()
{
	m_pGameType = "DM";
	m_GameFlags = IGF_SUDDENDEATH;
}

void CGameControllerDM::OnCharacterSpawn(CCharacter *pChr)
{
	pChr->IncreaseHealth(10);

	pChr->GiveWeapon(WEAPON_GUN, WEAPON_ID_PISTOL, 10);
	pChr->GiveWeapon(WEAPON_HAMMER, WEAPON_ID_HAMMER, -1);
}
