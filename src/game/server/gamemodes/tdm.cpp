/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include "tdm.h"
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/weapons.h>

CGameControllerTDM::CGameControllerTDM() :
	IGameController()
{
	m_pGameType = "TDM";
	m_GameFlags = IGF_TEAMS | IGF_SUDDENDEATH;

	INSTANCE_CONFIG_INT(&m_RespawnDelayTDM, "respawn_delay", 3, 0, 10, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Time needed to respawn after death in tdm gametype")
}

// event
void CGameControllerTDM::OnCharacterSpawn(CCharacter *pChr)
{
	pChr->IncreaseHealth(10);

	pChr->GiveWeapon(WEAPON_GUN, WEAPON_ID_PISTOL, 10);
	pChr->GiveWeapon(WEAPON_HAMMER, WEAPON_ID_HAMMER, -1);
}

int CGameControllerTDM::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	if(pKiller && Weapon != WEAPON_GAME)
	{
		// do team scoring
		if(pKiller == pVictim->GetPlayer() || pKiller->GetTeam() == pVictim->GetPlayer()->GetTeam())
			m_aTeamscore[pKiller->GetTeam() & 1]--; // klant arschel
		else
			m_aTeamscore[pKiller->GetTeam() & 1]++; // good shit
	}

	pVictim->GetPlayer()->m_RespawnTick = maximum(pVictim->GetPlayer()->m_RespawnTick, Server()->Tick() + Server()->TickSpeed() * m_RespawnDelayTDM);

	return DEATH_NORMAL;
}
