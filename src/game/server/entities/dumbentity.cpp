#include "dumbentity.h"

CDumbEntity::CDumbEntity(CGameWorld *pGameWorld, int Type, vec2 Pos, vec2 LaserVector) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_CUSTOM, Pos)
{
	m_Type = Type;
	m_PrevPrevPos = m_PrevPos = Pos;
	m_PrevVelocity = {0.0f, 0.0f};
	m_Velocity = {0.0f, 0.0f};
	m_LaserVector = LaserVector;
	m_ID = -1;

	if(!(m_Type & FLAG_MANUAL))
	{
		m_ID = Server()->SnapNewID();
		pGameWorld->InsertEntity(this);
	}
}

CDumbEntity::~CDumbEntity()
{
	if(!(m_Type & FLAG_MANUAL))
		Server()->SnapFreeID(m_ID);
}

void CDumbEntity::Reset()
{
	if(m_Type & FLAG_CLEAR_ON_RESET)
		Destroy();
}

void CDumbEntity::Tick()
{
	m_PrevVelocity = (m_Pos - m_PrevPrevPos) * 2.0f / (float)Server()->TickSpeed();
	m_Velocity = (m_Pos - m_PrevPos) / (float)Server()->TickSpeed();
	m_PrevPrevPos = m_PrevPos;
	m_PrevPos = m_Pos;
}

bool CDumbEntity::NetworkClipped(int SnappingClient)
{
	if((m_Type & MASK_TYPE) == TYPE_LASER && !(m_Type & FLAG_LASER_VELOCITY))
		return NetworkLineClipped(SnappingClient, m_Pos, m_Pos + m_LaserVector);

	return NetworkPointClipped(SnappingClient, m_Pos);
}

void CDumbEntity::Snap(int SnappingClient, int OtherMode)
{
	if(OtherMode || (m_Type & FLAG_MANUAL))
		return;

	DoSnap(m_ID, SnappingClient);
}

void CDumbEntity::MoveTo(vec2 Pos)
{
	m_Pos = Pos;
}

void CDumbEntity::TeleportTo(vec2 Pos)
{
	m_PrevVelocity = m_Velocity = {0.0f, 0.0f};
	m_PrevPrevPos = m_PrevPos = m_Pos = Pos;
}

void CDumbEntity::SetLaserVector(vec2 Vector)
{
	m_LaserVector = Vector;
}

void CDumbEntity::DoSnap(int SnapID, int SnappingClient)
{
	int Type = m_Type & MASK_TYPE;

	if(Type <= TYPE_POWERUP_NINJA)
	{
		int Size = Server()->IsSixup(SnappingClient) ? 3 * 4 : sizeof(CNetObj_Pickup);
		CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, SnapID, Size));
		if(!pP)
			return;

		int PickupType = POWERUP_HEALTH;
		int PickupSubtype = -1;
		if(Type == TYPE_HEART)
			PickupType = POWERUP_HEALTH;
		else if(Type == TYPE_ARMOR)
			PickupType = POWERUP_ARMOR;
		else if(Type <= TYPE_PICKUP_NINJA)
		{
			PickupType = POWERUP_WEAPON;
			PickupSubtype = Type - (TYPE_PICKUP_HAMMER - WEAPON_HAMMER);
		}
		else if(Type == TYPE_POWERUP_NINJA)
			PickupType = POWERUP_NINJA;

		pP->m_X = (int)m_Pos.x;
		pP->m_Y = (int)m_Pos.y;
		pP->m_Type = PickupType;
		if(Server()->IsSixup(SnappingClient))
		{
			if(Type == POWERUP_WEAPON)
			{
				switch(PickupSubtype)
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
			pP->m_Subtype = PickupSubtype;
	}
	else if(Type <= TYPE_PROJECTILE_GRENADE)
	{
		CNetObj_Projectile *pProj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_ID, sizeof(CNetObj_Projectile)));
		if(!pProj)
			return;

		pProj->m_StartTick = Server()->Tick() - 2;
		pProj->m_Type = Type - (TYPE_PROJECTILE_GUN - WEAPON_GUN);
		vec2 PrevPos = m_PrevPrevPos;
		vec2 Vel = m_PrevVelocity;
		float Delta = 2.0f / (float)Server()->TickSpeed();
		if(m_Type & FLAG_IMMEDIATE)
		{
			PrevPos = m_PrevPos;
			Vel = m_Velocity;
			Delta = 1.0f / (float)Server()->TickSpeed();
		}

		pProj->m_X = round_to_int(PrevPos.x);
		pProj->m_Y = round_to_int(PrevPos.y);

		if(m_Type & FLAG_NO_VELOCITY)
		{
			pProj->m_VelX = 0;
			pProj->m_VelY = 0;
		}
		else
		{
			if(length(Vel) == 0.0f)
			{
				pProj->m_VelX = 0;
				pProj->m_VelY = 0;
			}
			else
			{
				float Curvature = 0;
				float Speed = 0;
				GetProjectileProperties(&Curvature, &Speed);
				vec2 Direction = normalize(Vel);
				vec2 ClientPos = CalcPos(PrevPos, Direction, Curvature, Speed, Delta);
				vec2 ClientVel = (ClientPos - PrevPos) * Delta;

				if(fabs(ClientVel.x) < 1e-6)
					pProj->m_VelX = 0;
				else
					pProj->m_VelX = (int)(Direction.x * (Vel.x / ClientVel.x) * 100.0f);

				if(fabs(ClientVel.y) < 1e-6)
					pProj->m_VelY = 0;
				else
					pProj->m_VelY = (int)(Direction.y * (Vel.y / ClientVel.y) * 100.0f);
			}
		}
	}
	else if(Type <= TYPE_LASER)
	{
		CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_ID, sizeof(CNetObj_Laser)));
		if(!pObj)
			return;

		if(m_Type & FLAG_LASER_VELOCITY)
		{
			pObj->m_X = (int)m_Pos.x;
			pObj->m_Y = (int)m_Pos.y;
			pObj->m_FromX = (int)m_PrevPrevPos.x;
			pObj->m_FromY = (int)m_PrevPrevPos.y;
			pObj->m_StartTick = Server()->Tick();
		}
		else
		{
			vec2 To = m_Pos + m_LaserVector;
			pObj->m_X = (int)To.x;
			pObj->m_Y = (int)To.y;
			pObj->m_FromX = (int)m_Pos.x;
			pObj->m_FromY = (int)m_Pos.y;
			pObj->m_StartTick = Server()->Tick();
		}
	}
}

void CDumbEntity::GetProjectileProperties(float *pCurvature, float *pSpeed, int TuneZone)
{
	int Type = (m_Type & MASK_TYPE) - (TYPE_PROJECTILE_GUN - WEAPON_GUN);
	switch(Type)
	{
	case WEAPON_GRENADE:
		if(!TuneZone)
		{
			*pCurvature = GameServer()->Tuning()->m_GrenadeCurvature;
			*pSpeed = GameServer()->Tuning()->m_GrenadeSpeed;
		}
		else
		{
			*pCurvature = GameServer()->TuningList()[TuneZone].m_GrenadeCurvature;
			*pSpeed = GameServer()->TuningList()[TuneZone].m_GrenadeSpeed;
		}

		break;

	case WEAPON_SHOTGUN:
		if(!TuneZone)
		{
			*pCurvature = GameServer()->Tuning()->m_ShotgunCurvature;
			*pSpeed = GameServer()->Tuning()->m_ShotgunSpeed;
		}
		else
		{
			*pCurvature = GameServer()->TuningList()[TuneZone].m_ShotgunCurvature;
			*pSpeed = GameServer()->TuningList()[TuneZone].m_ShotgunSpeed;
		}

		break;

	case WEAPON_GUN:
		if(!TuneZone)
		{
			*pCurvature = GameServer()->Tuning()->m_GunCurvature;
			*pSpeed = GameServer()->Tuning()->m_GunSpeed;
		}
		else
		{
			*pCurvature = GameServer()->TuningList()[TuneZone].m_GunCurvature;
			*pSpeed = GameServer()->TuningList()[TuneZone].m_GunSpeed;
		}
		break;
	}
}
