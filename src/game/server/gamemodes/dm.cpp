/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "dm.h"

#include <game/server/entities/character.h>
#include <game/server/weapons.h>

CGameControllerDM::CGameControllerDM() :
	IGameController()
{
	m_pGameType = "DM";
}

void CGameControllerDM::OnCharacterSpawn(CCharacter *pChr)
{
	pChr->IncreaseHealth(10);

	pChr->SetWeaponTimerType(WEAPON_TIMER_INDIVIDUAL);

	pChr->GiveWeapon(WEAPON_HAMMER, WEAPON_TYPE_HAMMER, -1);
	pChr->GiveWeapon(WEAPON_GUN, WEAPON_TYPE_PISTOL, 10);
	pChr->GiveWeapon(WEAPON_SHOTGUN, WEAPON_TYPE_GRENADE, 10);
}
