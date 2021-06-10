/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "pickup.h"
#include <game/generated/protocol.h>
#include <game/generated/server_data.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <game/server/teams.h>
#include <game/server/weapons.h>

#include "character.h"

CPickup::CPickup(CGameWorld *pGameWorld, int Type, int SubType) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP, vec2(0, 0), PickupPhysSize)
{
	m_Type = Type;
	m_Subtype = SubType;

	m_ID = Server()->SnapNewID();
	Reset();

	GameWorld()->InsertEntity(this);
}

CPickup::~CPickup()
{
	Server()->SnapFreeID(m_ID);
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
	bool Destroying = false;
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

			SPickupSound PlaySound;
			PlaySound.m_Global = false;
			PlaySound.m_Sound = -1;
			int RespawnTime = Controller()->OnPickup(this, pChr, &PlaySound);
			if(PlaySound.m_Sound >= 0)
			{
				if(PlaySound.m_Global)
					GameWorld()->CreateSound(pChr->m_Pos, PlaySound.m_Sound);
				else
					GameWorld()->CreateSound(pChr->m_Pos, PlaySound.m_Sound, Mask);
			}
			if(RespawnTime >= 0)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "pickup player='%d:%s' item=%d/%d",
					pChr->GetPlayer()->GetCID(), Server()->ClientName(pChr->GetPlayer()->GetCID()), m_Type, m_Subtype);
				GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

				int RespawnTick = Server()->Tick() + RespawnTime;

				if(isSoloInteract)
					m_SoloSpawnTick[i] = RespawnTick;
				else if(isNormalInteract)
					m_SpawnTick = RespawnTick;
			}
			else if(RespawnTime == -2)
				Destroying = true;
		}
	}

	if(Destroying)
		Destroy();
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
	if(OtherMode)
		return;

	CPlayer *SnappingPlayer = SnappingClient > -1 ? GameServer()->m_apPlayers[SnappingClient] : nullptr;
	CPlayer *SnapPlayer = SnappingPlayer;

	if(SnappingClient > -1)
	{
		bool isSpectating = (GameServer()->m_apPlayers[SnappingClient]->GetTeam() == -1 || GameServer()->m_apPlayers[SnappingClient]->IsPaused());
		bool isFreeViewing = isSpectating && GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID() == SPEC_FREEVIEW;
		if(isSpectating && !isFreeViewing)
		{
			SnapPlayer = GameServer()->m_apPlayers[GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID()];
		}

		if(!isFreeViewing || (SnappingPlayer && SnappingPlayer->m_SpecTeam))
		{
			int SnapCID = SnapPlayer->GetCID();
			bool isActiveInSolo = Teams()->m_Core.GetSolo(SnapCID) && (m_SoloSpawnTick[SnapCID] < 0);
			bool isActiveInWorld = !Teams()->m_Core.GetSolo(SnapCID) && m_SpawnTick < 0;
			if(!isActiveInSolo && !isActiveInWorld)
				return;
		}
	}

	int Size = Server()->IsSixup(SnappingClient) ? 3 * 4 : sizeof(CNetObj_Pickup);
	CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_ID, Size));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Type = m_Type;
	if(Server()->IsSixup(SnappingClient))
	{
		if(m_Type == POWERUP_WEAPON)
		{
			switch(m_Subtype)
			{
			case WEAPON_HAMMER:
				pP->m_Type = protocol7::PICKUP_HAMMER;
				break;
			case WEAPON_GUN:
				pP->m_Type = protocol7::PICKUP_GUN;
				break;
			case WEAPON_SHOTGUN:
				pP->m_Type = protocol7::PICKUP_SHOTGUN;
				break;
			case WEAPON_GRENADE:
				pP->m_Type = protocol7::PICKUP_GRENADE;
				break;
			case WEAPON_LASER:
				pP->m_Type = protocol7::PICKUP_LASER;
				break;
			case WEAPON_NINJA:
				pP->m_Type = protocol7::PICKUP_NINJA;
				break;
			}
		}
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
