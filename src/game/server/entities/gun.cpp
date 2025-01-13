/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */
#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/teams.h>

#include "character.h"
#include "gun.h"
#include "plasma.h"

//////////////////////////////////////////////////
// CGun
//////////////////////////////////////////////////
CGun::CGun(CGameWorld *pGameWorld, vec2 Pos, bool Freeze, bool Explosive, int Layer, int Number) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_DDRACE)
{
	m_Layer = Layer;
	m_Number = Number;
	m_LastFire = Server()->Tick();
	m_Pos = Pos;
	m_EvalTick = Server()->Tick();
	m_Freeze = Freeze;
	m_Explosive = Explosive;

	GameWorld()->InsertEntity(this);

	m_ID = Server()->SnapNewID();
}

void CGun::Fire()
{
	CCharacter *Ents[MAX_CLIENTS];
	int Id = -1;
	int Len = 0;
	int Num = -1;
	Num = GameWorld()->FindEntities(m_Pos, g_Config.m_SvPlasmaRange, (CEntity **)Ents, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

	for(int i = 0; i < Num; i++)
	{
		CCharacter *Target = Ents[i];
		// now gun doesn't affect on super
		if(Target->Team() == TEAM_SUPER)
			continue;
		if(m_Layer == LAYER_SWITCH && m_Number > 0 && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[Target->Team()])
			continue;
		int res = GameServer()->Collision()->IntersectLine(m_Pos, Target->m_Pos, 0, 0);
		if(!res)
		{
			int TargetLen = length(Target->m_Pos - m_Pos);
			if(Len == 0 || Len > TargetLen)
			{
				Len = TargetLen;
				Id = i;
			}
		}
	}

	if(Id != -1)
	{
		CCharacter *Target = Ents[Id];
		new CPlasma(GameWorld(), m_Pos, normalize(Target->m_Pos - m_Pos), m_Freeze, m_Explosive);
		m_LastFire = Server()->Tick();
	}

	for(int i = 0; i < Num; i++)
	{
		CCharacter *Target = Ents[i];
		if(Target->IsAlive() && Teams()->m_Core.GetSolo(Target->GetPlayer()->GetCID()))
		{
			if(Id != i)
			{
				int res = GameServer()->Collision()->IntersectLine(m_Pos, Target->m_Pos, 0, 0);
				if(!res)
				{
					new CPlasma(GameWorld(), m_Pos, normalize(Target->m_Pos - m_Pos), m_Freeze, m_Explosive);
					m_LastFire = Server()->Tick();
				}
			}
		}
	}
}

CGun::~CGun()
{
	Server()->SnapFreeID(m_ID);
}

void CGun::Reset()
{
	m_MarkedForDestroy = true;
}

void CGun::Tick()
{
	if(Server()->Tick() % int(Server()->TickSpeed() * 0.15f) == 0)
	{
		int Flags;
		m_EvalTick = Server()->Tick();
		int index = GameServer()->Collision()->IsMover(m_Pos.x, m_Pos.y, &Flags);
		if(index)
		{
			m_Core = GameServer()->Collision()->CpSpeed(index, Flags);
		}
		m_Pos += m_Core;
	}
	if(m_LastFire + Server()->TickSpeed() / g_Config.m_SvPlasmaPerSec <= Server()->Tick())
		Fire();
}

void CGun::Snap(int SnappingClient, int OtherMode)
{
	int Tick = (Server()->Tick() % Server()->TickSpeed()) % 11;
	if(m_Layer == LAYER_SWITCH && m_Number > 0 && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[GameWorld()->Team()] && (!Tick))
		return;

	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_ID, sizeof(CNetObj_Laser)));

	if(!pObj)
		return;

	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_FromX = (int)m_Pos.x;
	pObj->m_FromY = (int)m_Pos.y;
	pObj->m_StartTick = m_EvalTick;
}
