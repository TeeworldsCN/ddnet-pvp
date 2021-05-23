/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "laser.h"
#include <game/generated/protocol.h>
#include <game/generated/server_data.h>
#include <game/server/gamecontext.h>

#include <engine/shared/config.h>
#include <game/server/teams.h>

#include "character.h"

CLaser::CLaser(
	CGameWorld *pGameWorld,
	int WeaponType,
	int WeaponID,
	int Owner,
	vec2 Pos,
	vec2 Direction,
	float StartEnergy,
	FLaserImpactCallback Callback,
	SEntityCustomData CustomData) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER)
{
	m_Pos = Pos;
	m_Owner = Owner;
	m_Energy = StartEnergy;
	m_Dir = Direction;
	m_Bounces = 0;
	m_EvalTick = 0;
	m_TelePos = vec2(0, 0);
	m_WasTele = false;
	m_Type = WeaponType;
	m_WeaponID = WeaponID;
	m_TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(m_Pos));
	m_Hit = 0;
	m_IsSolo = false;
	m_Callback = Callback;
	m_CustomData = CustomData;

	m_ID = Server()->SnapNewID();

	if(m_Owner >= 0)
	{
		CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
		if(pOwnerChar)
		{
			m_Hit = pOwnerChar->m_Hit;
			m_IsSolo = Teams()->m_Core.GetSolo(m_Owner);
		}
	}
	GameWorld()->InsertEntity(this);
	DoBounce();
}

bool CLaser::HitCharacter(vec2 From, vec2 To)
{
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	bool IsProtectingOwner = m_Bounces == 0 && !m_WasTele;

	std::list<CCharacter *> pTargetChars;

	// solo bullet can't interact with anyone
	if((m_Type == WEAPON_GRENADE && !(m_Hit & CCharacter::DISABLE_HIT_GRENADE)) ||
		(m_Type == WEAPON_SHOTGUN && !(m_Hit & CCharacter::DISABLE_HIT_SHOTGUN)) ||
		(m_Type == WEAPON_GUN && !(m_Hit & CCharacter::DISABLE_HIT_GUN)) ||
		(m_Type == WEAPON_LASER && !(m_Hit & CCharacter::DISABLE_HIT_LASER)))
	{
		if(m_IsSolo)
		{
			if(GameWorld()->IntersectThisCharacter(m_Pos, To, 0.f, pOwnerChar))
				pTargetChars.push_back(pOwnerChar);
		}
		else
		{
			pTargetChars = GameWorld()->IntersectedCharacters(m_Pos, To, 0.f, nullptr, m_Owner >= 0);
		}
	}

	for(auto pChar : pTargetChars)
	{
		if(pChar == pOwnerChar && IsProtectingOwner)
			continue;

		bool AlreadyHit = m_HitMask & pChar->GetPlayer()->GetCID();
		m_HitMask |= pChar->GetPlayer()->GetCID();
		pChar->m_HitData.m_HitOrder = m_NumHits++;
		pChar->m_HitData.m_FirstImpact = !AlreadyHit;
		if(m_Callback && m_Callback(this, pChar->m_HitData.m_Intersection, pChar, false))
		{
			m_From = From;
			m_Pos = pChar->m_HitData.m_Intersection;
			m_Energy = -1;
			return true;
		}
	}

	return false;
}

void CLaser::DoBounce()
{
	m_EvalTick = Server()->Tick();

	if(m_Energy < 0)
	{
		m_MarkedForDestroy = true;
		return;
	}
	m_PrevPos = m_Pos;
	vec2 Coltile;

	int Res;
	int z;

	if(m_WasTele)
	{
		m_PrevPos = m_TelePos;
		m_Pos = m_TelePos;
		m_TelePos = vec2(0, 0);
	}

	vec2 To = m_Pos + m_Dir * m_Energy;

	Res = GameServer()->Collision()->IntersectLineTeleWeapon(m_Pos, To, &Coltile, &To, &z);

	if(Res)
	{
		if(!HitCharacter(m_Pos, To))
		{
			// intersected
			m_From = m_Pos;
			m_Pos = To;

			vec2 TempPos = m_Pos;
			vec2 TempDir = m_Dir * 4.0f;

			int f = 0;
			if(Res == -1)
			{
				f = GameServer()->Collision()->GetTile(round_to_int(Coltile.x), round_to_int(Coltile.y));
				GameServer()->Collision()->SetCollisionAt(round_to_int(Coltile.x), round_to_int(Coltile.y), TILE_SOLID);
			}
			GameServer()->Collision()->MovePoint(&TempPos, &TempDir, 1.0f, 0);
			if(Res == -1)
			{
				GameServer()->Collision()->SetCollisionAt(round_to_int(Coltile.x), round_to_int(Coltile.y), f);
			}
			m_Pos = TempPos;
			m_Dir = normalize(TempDir);

			if(!m_TuneZone)
				m_Energy -= distance(m_From, m_Pos) + GameServer()->Tuning()->m_LaserBounceCost;
			else
				m_Energy -= distance(m_From, m_Pos) + GameServer()->TuningList()[m_TuneZone].m_LaserBounceCost;

			if(Res == TILE_TELEINWEAPON && GameServer()->Collision()->NumTeles(z - 1))
			{
				m_TelePos = GameServer()->Collision()->TelePos(z - 1);
				m_WasTele = true;
			}
			else
			{
				m_Bounces++;
				m_WasTele = false;
			}

			int BounceNum = GameServer()->Tuning()->m_LaserBounceNum;
			if(m_TuneZone)
				BounceNum = GameServer()->TuningList()[m_TuneZone].m_LaserBounceNum;

			if(m_Bounces > BounceNum)
				m_Energy = -1;

			if(m_Callback && m_Callback(this, m_Pos, nullptr, m_Energy == -1))
			{
				m_Energy = -1;
			}
			GameWorld()->CreateSound(m_Pos, SOUND_LASER_BOUNCE);
		}
	}
	else
	{
		if(!HitCharacter(m_Pos, To))
		{
			m_From = m_Pos;
			m_Pos = To;
			m_Energy = -1;
		}
	}
}

CLaser::~CLaser()
{
	Server()->SnapFreeID(m_ID);
}

void CLaser::Reset()
{
	m_MarkedForDestroy = true;
}

void CLaser::Tick()
{
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	CPlayer *pOwnerPlayer = GameServer()->m_apPlayers[m_Owner];
	if((!pOwnerChar || !pOwnerChar->IsAlive()) && m_Owner >= 0 && (m_IsSolo || g_Config.m_SvDestroyLasersOnDeath))
	{
		Reset();
		return;
	}

	if(!pOwnerPlayer || GameServer()->GetDDRaceTeam(m_Owner) != GameWorld()->Team())
	{
		Reset(); // owner has gone to another reality.
		return;
	}

	float Delay;
	if(m_TuneZone)
		Delay = GameServer()->TuningList()[m_TuneZone].m_LaserBounceDelay;
	else
		Delay = GameServer()->Tuning()->m_LaserBounceDelay;

	if((Server()->Tick() - m_EvalTick) > (Server()->TickSpeed() * Delay / 1000.0f))
		DoBounce();
}

void CLaser::TickPaused()
{
	++m_EvalTick;
}

bool CLaser::NetworkClipped(int SnappingClient)
{
	return NetworkLineClipped(SnappingClient, m_From, m_Pos);
}

void CLaser::Snap(int SnappingClient, int OtherMode)
{
	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_ID, sizeof(CNetObj_Laser)));
	if(!pObj)
		return;

	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_FromX = (int)m_From.x;
	pObj->m_FromY = (int)m_From.y;
	pObj->m_StartTick = OtherMode ? m_EvalTick - 4 : m_EvalTick; // HACK: Send thin laser for other team.
}

void CLaser::Destroy()
{
	if(m_CustomData.m_Callback)
		m_CustomData.m_Callback(m_CustomData.m_pData);
	CEntity::Destroy();
}