/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "dragger.h"
#include <engine/config.h>
#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include "character.h"

CDragger::CDragger(CGameWorld *pGameWorld, vec2 Pos, float Strength, bool NW, int Layer, int Number) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_DDRACE)
{
	m_Target = 0;
	m_Layer = Layer;
	m_Number = Number;
	m_Pos = Pos;
	m_Strength = Strength;
	m_EvalTick = Server()->Tick();
	m_NW = NW;
	GameWorld()->InsertEntity(this);

	for(int &SoloID : m_SoloIDs)
		SoloID = -1;

	m_ID = Server()->SnapNewID();
}

void CDragger::Move()
{
	if(m_Target && (!m_Target->IsAlive() || (m_Target->IsAlive() && (m_Target->m_Super || m_Target->IsDisabled() || (m_Layer == LAYER_SWITCH && m_Number && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[m_Target->Team()])))))
		m_Target = 0;

	mem_zero(m_SoloEnts, sizeof(m_SoloEnts));
	CCharacter *TempEnts[MAX_CLIENTS];

	int Num = GameWorld()->FindEntities(m_Pos, g_Config.m_SvDraggerRange,
		(CEntity **)m_SoloEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	mem_copy(TempEnts, m_SoloEnts, sizeof(TempEnts));

	int Id = -1;
	int MinLen = 0;
	CCharacter *Temp;
	for(int i = 0; i < Num; i++)
	{
		Temp = m_SoloEnts[i];
		if(m_Layer == LAYER_SWITCH && m_Number && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[Temp->Team()])
		{
			m_SoloEnts[i] = 0;
			continue;
		}
		int Res =
			m_NW ?
				GameServer()->Collision()->IntersectNoLaserNW(m_Pos, Temp->m_Pos, 0, 0) :
				GameServer()->Collision()->IntersectNoLaser(m_Pos, Temp->m_Pos, 0, 0);

		if(Res == 0)
		{
			int Len = length(Temp->m_Pos - m_Pos);
			if(MinLen == 0 || MinLen > Len)
			{
				MinLen = Len;
				Id = i;
			}

			if(!Teams()->m_Core.GetSolo(Temp->GetPlayer()->GetCID()))
				m_SoloEnts[i] = 0;
		}
		else
		{
			m_SoloEnts[i] = 0;
		}
	}

	if(!m_Target)
		m_Target = Id != -1 ? TempEnts[Id] : 0;

	if(m_Target)
	{
		for(auto &SoloEnt : m_SoloEnts)
		{
			if(SoloEnt == m_Target)
				SoloEnt = 0;
		}
	}
}

void CDragger::Drag()
{
	if(!m_Target)
		return;

	CCharacter *Target = m_Target;

	for(int i = -1; i < MAX_CLIENTS; i++)
	{
		if(i >= 0)
			Target = m_SoloEnts[i];

		if(!Target)
			continue;

		int Res = 0;
		if(!m_NW)
			Res = GameServer()->Collision()->IntersectNoLaser(m_Pos,
				Target->m_Pos, 0, 0);
		else
			Res = GameServer()->Collision()->IntersectNoLaserNW(m_Pos,
				Target->m_Pos, 0, 0);
		if(Res || length(m_Pos - Target->m_Pos) > g_Config.m_SvDraggerRange)
		{
			Target = 0;
			if(i == -1)
				m_Target = 0;
			else
				m_SoloEnts[i] = 0;
		}
		else if(length(m_Pos - Target->m_Pos) > 28)
		{
			vec2 Temp = Target->Core()->m_Vel + (normalize(m_Pos - Target->m_Pos) * m_Strength);
			Target->Core()->m_Vel = ClampVel(Target->m_MoveRestrictions, Temp);
		}
	}
}

CDragger::~CDragger()
{
	Server()->SnapFreeID(m_ID);
	for(int &SoloID : m_SoloIDs)
		if(SoloID >= 0)
			Server()->SnapFreeID(SoloID);
}

void CDragger::Reset()
{
	m_MarkedForDestroy = true;
}

void CDragger::Tick()
{
	if(Server()->Tick() % int(Server()->TickSpeed() * 0.15f) == 0)
	{
		int Flags;
		m_EvalTick = Server()->Tick();
		int index = GameServer()->Collision()->IsMover(m_Pos.x, m_Pos.y,
			&Flags);
		if(index)
		{
			m_Core = GameServer()->Collision()->CpSpeed(index, Flags);
		}
		m_Pos += m_Core;
		Move();
	}
	Drag();
	return;
}

void CDragger::Snap(int SnappingClient, int OtherMode)
{
	if(OtherMode)
		return;

	CCharacter *Target = m_Target;

	int pos = 0;
	for(int &SoloID : m_SoloIDs)
	{
		if(SoloID == -1)
			break;

		Server()->SnapFreeID(SoloID);
		SoloID = -1;
	}

	for(int i = -1; i < MAX_CLIENTS; i++)
	{
		if(i >= 0)
		{
			Target = m_SoloEnts[i];

			if(!Target)
				continue;
		}

		if(Target && NetworkLineClipped(SnappingClient, m_Pos, Target->m_Pos))
			continue;

		int Tick = (Server()->Tick() % Server()->TickSpeed()) % 11;
		if(m_Layer == LAYER_SWITCH && m_Number && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[GameWorld()->Team()] && (!Tick))
			continue;

		int TargetCID = Target->GetPlayer()->GetCID();

		if(TargetCID != SnappingClient && Teams()->m_Core.GetSolo(TargetCID))
			continue;

		CNetObj_Laser *obj;

		if(i == -1)
		{
			obj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(
				NETOBJTYPE_LASER, m_ID, sizeof(CNetObj_Laser)));
		}
		else
		{
			m_SoloIDs[pos] = Server()->SnapNewID();
			obj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem( // TODO: Have to free IDs again?
				NETOBJTYPE_LASER, m_SoloIDs[pos], sizeof(CNetObj_Laser)));
			pos++;
		}

		if(!obj)
			continue;
		obj->m_X = (int)m_Pos.x;
		obj->m_Y = (int)m_Pos.y;
		if(Target)
		{
			obj->m_FromX = (int)Target->m_Pos.x;
			obj->m_FromY = (int)Target->m_Pos.y;
		}
		else
		{
			obj->m_FromX = (int)m_Pos.x;
			obj->m_FromY = (int)m_Pos.y;
		}

		int StartTick = m_EvalTick;
		if(StartTick < Server()->Tick() - 4)
			StartTick = Server()->Tick() - 4;
		else if(StartTick > Server()->Tick())
			StartTick = Server()->Tick();
		obj->m_StartTick = StartTick;
	}
}
