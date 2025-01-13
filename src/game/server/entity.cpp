/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "entity.h"
#include "gamecontext.h"
#include "player.h"

//////////////////////////////////////////////////
// Entity
//////////////////////////////////////////////////
CEntity::CEntity(CGameWorld *pGameWorld, int ObjType, vec2 Pos, int ProximityRadius)
{
	m_pGameWorld = pGameWorld;
	m_pGameServer = pGameWorld->GameServer();

	m_ObjType = ObjType;
	m_Pos = Pos;
	m_ProximityRadius = ProximityRadius;

	m_MarkedForDestroy = false;
	m_OnSnap = nullptr;

	m_pPrevTypeEntity = 0;
	m_pNextTypeEntity = 0;
}

CEntity::~CEntity()
{
	GameWorld()->RemoveEntity(this);
}

bool CEntity::NetworkClipped(int SnappingClient)
{
	return ::NetworkPointClipped(GameServer(), SnappingClient, m_Pos);
}

bool CEntity::NetworkPointClipped(int SnappingClient, vec2 CheckPos, vec2 MinView)
{
	return ::NetworkPointClipped(GameServer(), SnappingClient, CheckPos, MinView);
}

bool CEntity::NetworkLineClipped(int SnappingClient, vec2 From, vec2 To, vec2 MinView)
{
	return ::NetworkLineClipped(GameServer(), SnappingClient, From, To, MinView);
}

bool CEntity::NetworkRectClipped(int SnappingClient, vec2 TL, vec2 BR, vec2 MinView)
{
	return ::NetworkRectClipped(GameServer(), SnappingClient, TL, BR, MinView);
}

bool CEntity::GameLayerClipped(vec2 CheckPos)
{
	return round_to_int(CheckPos.x) / 32 < -200 || round_to_int(CheckPos.x) / 32 > GameServer()->Collision()->GetWidth() + 200 ||
			       round_to_int(CheckPos.y) / 32 < -200 || round_to_int(CheckPos.y) / 32 > GameServer()->Collision()->GetHeight() + 200 ?
		       true :
		       false;
}

bool CEntity::GetNearestAirPos(vec2 Pos, vec2 PrevPos, vec2 *pOutPos)
{
	for(int k = 0; k < 16 && GameServer()->Collision()->CheckPoint(Pos); k++)
	{
		Pos -= normalize(PrevPos - Pos);
	}

	vec2 PosInBlock = vec2(round_to_int(Pos.x) % 32, round_to_int(Pos.y) % 32);
	vec2 BlockCenter = vec2(round_to_int(Pos.x), round_to_int(Pos.y)) - PosInBlock + vec2(16.0f, 16.0f);

	*pOutPos = vec2(BlockCenter.x + (PosInBlock.x < 16 ? -2.0f : 1.0f), Pos.y);
	if(!GameServer()->Collision()->TestBox(*pOutPos, vec2(28.0f, 28.0f)))
		return true;

	*pOutPos = vec2(Pos.x, BlockCenter.y + (PosInBlock.y < 16 ? -2.0f : 1.0f));
	if(!GameServer()->Collision()->TestBox(*pOutPos, vec2(28.0f, 28.0f)))
		return true;

	*pOutPos = vec2(BlockCenter.x + (PosInBlock.x < 16 ? -2.0f : 1.0f),
		BlockCenter.y + (PosInBlock.y < 16 ? -2.0f : 1.0f));
	if(!GameServer()->Collision()->TestBox(*pOutPos, vec2(28.0f, 28.0f)))
		return true;

	return false;
}

bool CEntity::GetNearestAirPosPlayer(vec2 PlayerPos, vec2 *OutPos)
{
	for(int dist = 5; dist >= -1; dist--)
	{
		*OutPos = vec2(PlayerPos.x, PlayerPos.y - dist);
		if(!GameServer()->Collision()->TestBox(*OutPos, vec2(28.0f, 28.0f)))
		{
			return true;
		}
	}
	return false;
}

bool NetworkPointClipped(CGameContext *pGameServer, int SnappingClient, vec2 CheckPos, vec2 MinView)
{
	if(SnappingClient == -1)
		return false;

	vec2 ShowDistance;
	if(pGameServer->m_apPlayers[SnappingClient]->IsSpectating())
		ShowDistance = pGameServer->m_apPlayers[SnappingClient]->m_ShowDistance;
	else
		ShowDistance = vec2(SHOW_DISTANCE_DEFAULT_X, SHOW_DISTANCE_DEFAULT_Y);

	float dx = pGameServer->m_apPlayers[SnappingClient]->m_ViewPos.x - CheckPos.x;
	if(absolute(dx) > maximum(ShowDistance.x, MinView.x))
		return true;

	float dy = pGameServer->m_apPlayers[SnappingClient]->m_ViewPos.y - CheckPos.y;
	if(absolute(dy) > maximum(ShowDistance.y, MinView.y))
		return true;

	return false;
}

bool NetworkLineClipped(CGameContext *pGameServer, int SnappingClient, vec2 From, vec2 To, vec2 MinView)
{
	if(SnappingClient == -1)
		return false;

	vec2 ShowDistance;
	if(pGameServer->m_apPlayers[SnappingClient]->IsSpectating())
		ShowDistance = pGameServer->m_apPlayers[SnappingClient]->m_ShowDistance;
	else
		ShowDistance = vec2(SHOW_DISTANCE_DEFAULT_X, SHOW_DISTANCE_DEFAULT_Y);

	ShowDistance.x = maximum(ShowDistance.x, MinView.x);
	ShowDistance.y = maximum(ShowDistance.y, MinView.y);

	vec2 ViewPos = pGameServer->m_apPlayers[SnappingClient]->m_ViewPos;
	vec2 TL = ViewPos - ShowDistance;
	vec2 TR = vec2(ViewPos.x + ShowDistance.x, ViewPos.y - ShowDistance.y);
	vec2 BL = vec2(ViewPos.x - ShowDistance.x, ViewPos.y + ShowDistance.y);
	vec2 BR = ViewPos - ShowDistance;

	// not clipped if endpoints already in view rect
	if((absolute(ViewPos.x - From.x) <= ShowDistance.x && absolute(ViewPos.y - From.y) <= ShowDistance.y) ||
		(absolute(ViewPos.x - To.x) <= ShowDistance.x && absolute(ViewPos.y - To.y) <= ShowDistance.y))
		return false;

	auto SegmentIntersect = [From, To](vec2 p0, vec2 p1) -> bool {
		vec2 l = p0 - p1;
		float d = To.x * From.y - From.x * To.y;
		float s = (l.x * From.y - l.y * From.x);
		if(s < 0 || s > d)
			return false;
		float t = (l.x * To.y - l.y * To.x);
		if(t < 0 || t > d)
			return false;
		return true;
	};

	// not clipped if line intersect with any edge of the view rect
	if(SegmentIntersect(TL, TR) || SegmentIntersect(BL, BR) || SegmentIntersect(TL, BL) || SegmentIntersect(TR, BR))
		return false;

	return true;
}

bool NetworkRectClipped(CGameContext *pGameServer, int SnappingClient, vec2 TL, vec2 BR, vec2 MinView)
{
	if(SnappingClient == -1)
		return false;

	vec2 ShowDistance;
	if(pGameServer->m_apPlayers[SnappingClient]->IsSpectating())
		ShowDistance = pGameServer->m_apPlayers[SnappingClient]->m_ShowDistance;
	else
		ShowDistance = vec2(SHOW_DISTANCE_DEFAULT_X, SHOW_DISTANCE_DEFAULT_Y);

	ShowDistance.x = maximum(ShowDistance.x, MinView.x);
	ShowDistance.y = maximum(ShowDistance.y, MinView.y);

	vec2 ViewPos = pGameServer->m_apPlayers[SnappingClient]->m_ViewPos;
	float Top = ViewPos.y - ShowDistance.y;
	float Left = ViewPos.x - ShowDistance.x;
	float Bottom = ViewPos.y + ShowDistance.y;
	float Right = ViewPos.x + ShowDistance.x;

	if(TL.x < Right && BR.x > Left && TL.y > Bottom && BR.y < Top)
		return true;

	return false;
}

void CEntity::InternalSnap(int SnappingClient, int OtherMode)
{
	bool IsClipped = NetworkClipped(SnappingClient);

	if(!OtherMode && m_OnSnap && m_OnSnap(Controller(), this, SnappingClient, IsClipped))
		return;

	if(IsClipped)
		return;

	Snap(SnappingClient, OtherMode);
}