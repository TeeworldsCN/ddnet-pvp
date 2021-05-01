/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "pickup.h"
#include <game/generated/protocol.h>
#include <game/generated/server_data.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <game/server/teams.h>

#include "character.h"

CPickup::CPickup(CGameWorld *pGameWorld, int Type, int SubType) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP, vec2(0, 0), PickupPhysSize)
{
	m_Type = Type;
	m_Subtype = SubType;

	Reset();

	GameWorld()->InsertEntity(this);
}

void CPickup::Reset()
{
	int SpawnTick = -1;
	if(g_pData->m_aPickups[m_Type].m_Spawndelay > 0)
		SpawnTick = Server()->Tick() + Server()->TickSpeed() * g_pData->m_aPickups[m_Type].m_Spawndelay;

	m_SpawnTick = SpawnTick;
	for(int i = 0; i < MAX_CLIENTS; ++i)
		m_SoloSpawnTick[i] = SpawnTick;
}

void CPickup::Tick()
{
	Move();

	// wait for respawn
	if(m_SpawnTick > 0)
	{
		if(Server()->Tick() > m_SpawnTick)
		{
			// respawn
			m_SpawnTick = -1;

			if(m_Type == POWERUP_WEAPON)
				GameWorld()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN);
		}
	}

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_SoloSpawnTick[i] > 0)
		{
			if(Server()->Tick() > m_SoloSpawnTick[i])
			{
				// respawn
				m_SoloSpawnTick[i] = -1;

				if(m_Type == POWERUP_WEAPON)
					GameWorld()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, CmaskOne(i));
			}
		}
	}

	// Check if a player intersected us
	CCharacter *apEnts[MAX_CLIENTS];
	int Num = GameWorld()->FindEntities(m_Pos, 20.0f, (CEntity **)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; ++i)
	{
		CCharacter *pChr = apEnts[i];
		bool isSoloInteract = (m_SoloSpawnTick[pChr->GetPlayer()->GetCID()] < 0) && GameServer()->Teams()->m_Core.GetSolo(i);
		bool isNormalInteract = m_SpawnTick < 0 && !GameServer()->Teams()->m_Core.GetSolo(i);

		if(pChr && pChr->IsAlive() && (isSoloInteract || isNormalInteract))
		{
			int64 Mask = 0;
			if(isSoloInteract)
				Mask = CmaskOne(i);
			else if(isNormalInteract)
				Mask = -1LL;

			// player picked us up, is someone was hooking us, let them go
			int RespawnTime = -1;

			switch(m_Type)
			{
			case POWERUP_HEALTH:
				if(pChr->IncreaseHealth(1))
				{
					GameWorld()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, Mask);
					RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
				}
				break;

			case POWERUP_ARMOR:
				if(pChr->IncreaseArmor(1))
				{
					GameWorld()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, Mask);
					RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
				}
				break;

			case POWERUP_WEAPON:

				if(m_Subtype >= 0 && m_Subtype < NUM_WEAPONS && pChr->GetWeaponAmmo(m_Subtype) != -1)
				{
					if(pChr->GiveWeapon(m_Subtype, g_pData->m_Weapons.m_aId[m_Subtype].m_Maxammo))
					{
						RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;

						if(m_Subtype == WEAPON_GRENADE)
							GameWorld()->CreateSound(m_Pos, SOUND_PICKUP_GRENADE, Mask);
						else if(m_Subtype == WEAPON_SHOTGUN)
							GameWorld()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN, Mask);
						else if(m_Subtype == WEAPON_LASER)
							GameWorld()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN, Mask);

						if(pChr->GetPlayer())
							GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), m_Subtype);
					}
				}
				break;

			case POWERUP_NINJA:
			{
				// activate ninja on target player
				pChr->GiveNinja();
				RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;

				// loop through all players, setting their emotes
				CCharacter *pC = static_cast<CCharacter *>(GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER));
				for(; pC; pC = (CCharacter *)pC->TypeNext())
				{
					if(pC != pChr)
						pC->SetEmote(EMOTE_SURPRISE, Server()->Tick() + Server()->TickSpeed());
				}

				pChr->SetEmote(EMOTE_ANGRY, Server()->Tick() + 1200 * Server()->TickSpeed() / 1000);
				break;
			}
			default:
				break;
			};

			if(RespawnTime >= 0)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "pickup player='%d:%s' item=%d/%d",
					pChr->GetPlayer()->GetCID(), Server()->ClientName(pChr->GetPlayer()->GetCID()), m_Type, m_Subtype);
				GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

				int RespawnTick = Server()->Tick() + Server()->TickSpeed() * RespawnTime;

				if(isSoloInteract)
					m_SoloSpawnTick[i] = RespawnTick;
				else if(isNormalInteract)
					m_SpawnTick = RespawnTick;
			}
		}
	}
}

void CPickup::TickPaused()
{
	if(m_SpawnTick != -1)
		++m_SpawnTick;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_SoloSpawnTick[i] != -1)
			++m_SoloSpawnTick[i];
	}
}

void CPickup::Snap(int SnappingClient, int OtherMode)
{
	if(OtherMode || NetworkClipped(SnappingClient))
		return;

	CPlayer *SnappingPlayer = SnappingClient > -1 ? GameServer()->m_apPlayers[SnappingClient] : nullptr;
	CPlayer *SnapPlayer = SnappingPlayer;

	if(SnappingClient > -1)
	{
		bool isSpectating = (GameServer()->m_apPlayers[SnappingClient]->GetTeam() == -1 || GameServer()->m_apPlayers[SnappingClient]->IsPaused());
		bool isFreeViewing = isSpectating && GameServer()->m_apPlayers[SnappingClient]->m_SpectatorID == SPEC_FREEVIEW;
		if(isSpectating && !isFreeViewing)
		{
			SnapPlayer = GameServer()->m_apPlayers[GameServer()->m_apPlayers[SnappingClient]->m_SpectatorID];
		}

		if(!isFreeViewing || (SnappingPlayer && SnappingPlayer->m_SpecTeam))
		{
			int SnapCID = SnapPlayer->GetCID();
			bool isSoloActive = Teams()->m_Core.GetSolo(SnapCID) && (m_SoloSpawnTick[SnapCID] < 0);
			bool isTeamActive = !Teams()->m_Core.GetSolo(SnapCID) && m_SpawnTick < 0;
			if(!isSoloActive && !isTeamActive)
				return;
		}
	}

	int Size = Server()->IsSixup(SnappingClient) ? 3 * 4 : sizeof(CNetObj_Pickup);
	CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), Size));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Type = m_Type;
	if(Server()->IsSixup(SnappingClient))
	{
		if(m_Type == POWERUP_WEAPON)
			pP->m_Type = m_Subtype == WEAPON_SHOTGUN ? 3 : m_Subtype == WEAPON_GRENADE ? 2 :
                                                                                                     4;
		else if(m_Type == POWERUP_NINJA)
			pP->m_Type = 5;
	}
	else
		pP->m_Subtype = m_Subtype;
}

void CPickup::Move()
{
	if(Server()->Tick() % int(Server()->TickSpeed() * 0.15f) == 0)
	{
		int Flags;
		int index = GameServer()->Collision()->IsMover(m_Pos.x, m_Pos.y, &Flags);
		if(index)
		{
			m_Core = GameServer()->Collision()->CpSpeed(index, Flags);
		}
		m_Pos += m_Core;
	}
}
