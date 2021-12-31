#include "block-down.h"

#include <game/server/ModCore/BlockDown/player.h>
#include <game/server/entities/character.h>
#include <game/server/weapons.h>

CGameControllerBD::CGameControllerBD() :
	IGameController()
{
	m_pGameType = "BlockDown";
	m_GameFlags = IGF_RACE;
}

void CGameControllerBD::OnCharacterSpawn(CCharacter *pChr)
{
	CBDPlayer *BDPlayer;
	
	if(BDPlayer->IsZombie(pChr->GetPlayer()->GetCID()))
	{
		BDPlayer->ToHuman(pChr->GetPlayer()->GetCID());
		pChr->GiveWeapon(WEAPON_HAMMER, WEAPON_ID_INFHAMMER, -1);
	}
	else if(BDPlayer->IsZombie(pChr->GetPlayer()->GetCID()) == false)
	{
		BDPlayer->ToZombie(pChr->GetPlayer()->GetCID());
		pChr->GiveWeapon(WEAPON_LASER, WEAPON_ID_FREEZELASER, -1);
	}
	pChr->IncreaseHealth(10);
	// pChr->GiveWeapon(WEAPON_GUN, WEAPON_ID_USELESSGUN, -1) // Make it in future, im tired.
}
