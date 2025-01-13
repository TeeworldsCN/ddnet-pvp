/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITY_H
#define GAME_SERVER_ENTITY_H

#include <base/vmath.h>

#include "alloc.h"
#include "gamecontext.h"
#include "gameworld.h"

typedef void (*FCustomDataDestroyCallback)(void *pData);
typedef bool (*FCustomSnapCallback)(class IGameController *pController, CEntity *pEntity, int SnappingClient, bool IsClipped);

struct SEntityCustomData
{
	void *m_pData;
	FCustomDataDestroyCallback m_Callback;
};

/*
	Class: Entity
		Basic entity class.
*/
class CEntity
{
	MACRO_ALLOC_HEAP()

private:
	friend class CGameWorld; // entity list handling
	CEntity *m_pPrevTypeEntity;
	CEntity *m_pNextTypeEntity;

	/* Identity */
	class CGameWorld *m_pGameWorld;
	class CGameContext *m_pGameServer;

	int m_ObjType;

	/*
		Variable: m_ProximityRadius
			Contains the physical size of the entity.
	*/
	float m_ProximityRadius;

protected:
	/* State */
	bool m_MarkedForDestroy;

	/* Callback */
	FCustomSnapCallback m_OnSnap;

public: // TODO: Maybe make protected
	/*
		Variable: m_Pos
			Contains the current posititon of the entity.
	*/
	vec2 m_Pos;

public:
	/* Constructor */
	CEntity(CGameWorld *pGameWorld, int Objtype, vec2 Pos = vec2(0, 0), int ProximityRadius = 0);

	/* Destructor
		You should free SnapID in destructor
	*/
	virtual ~CEntity();

	/* Objects */
	class CGameWorld *GameWorld() { return m_pGameWorld; }
	class CConfig *Config() { return m_pGameServer->Config(); }
	class CGameContext *GameServer() { return m_pGameServer; }
	class IServer *Server() { return m_pGameServer->Server(); }
	class CGameTeams *Teams() { return m_pGameServer->Teams(); }

	// If the entity exists, the world is still associated with a controller
	class IGameController *Controller() { return m_pGameServer->GameInstance(m_pGameWorld->Team()).m_pController; }

	/* Getters */
	CEntity *TypeNext()
	{
		return m_pNextTypeEntity;
	}
	CEntity *TypePrev() { return m_pPrevTypeEntity; }
	const vec2 &GetPos() const { return m_Pos; }
	float GetProximityRadius() const { return m_ProximityRadius; }
	void CustomSnap(class IGameController *pController, FCustomSnapCallback Callback)
	{
#ifdef CONF_DEBUG
		if(pController == Controller())
			m_OnSnap = Callback;
		else
			dbg_msg("entity", "warning: you can only do custom snaps for entities in the same room of your controller");
#else
		m_OnSnap = Callback;
#endif
	}

	/* Other functions */

	/*
		Function: Destroy
			Destroys the entity.
	*/
	virtual void Destroy()
	{
		delete this;
	}

	/*
		Function: Reset
			Called when the game resets the map. Puts the entity
			back to its starting state or perhaps destroys it.
	*/
	virtual void Reset() {}

	/*
		Function: Tick
			Called to progress the entity to the next tick. Updates
			and moves the entity to its new state and position.
	*/
	virtual void Tick() {}

	/*
		Function: TickDefered
			Called after all entities Tick() function has been called.
	*/
	virtual void TickDefered() {}

	/*
		Function: TickPaused
			Called when the game is paused, to freeze the state and position of the entity.
	*/
	virtual void TickPaused() {}

	/*
		Function: Snap
			Called when a new snapshot is being generated for a specific
			client.

		Arguments:
			SnappingClient - ID of the client which snapshot is
				being generated. Could be -1 to create a complete
				snapshot of everything in the game for demo
				recording.
	*/
	virtual void Snap(int SnappingClient, int OtherMode) {}

	/*
		Function: NetworkClipped
			Performs a series of test to see if a client can see the
			entity.

		Arguments:
			SnappingClient - ID of the client which snapshot is
				being generated. Could be -1 to create a complete
				snapshot of everything in the game for demo
				recording.

		Returns:
			True if the entity doesn't have to be in the snapshot.
	*/
	virtual bool NetworkClipped(int SnappingClient);

	bool NetworkPointClipped(int SnappingClient, vec2 CheckPos, vec2 MinView = {0.0f, 0.0f});
	bool NetworkLineClipped(int SnappingClient, vec2 From, vec2 To, vec2 MinView = {0.0f, 0.0f});
	bool NetworkRectClipped(int SnappingClient, vec2 TL, vec2 BR, vec2 MinView = {0.0f, 0.0f});
	bool GameLayerClipped(vec2 CheckPos);

	void InternalSnap(int SnappingClient, int OtherMode);

	// DDRace

	bool GetNearestAirPos(vec2 Pos, vec2 ColPos, vec2 *pOutPos);
	bool GetNearestAirPosPlayer(vec2 PlayerPos, vec2 *OutPos);

	int m_Number;
	int m_Layer;
};

bool NetworkPointClipped(CGameContext *pGameServer, int SnappingClient, vec2 CheckPos, vec2 MinView = {0.0f, 0.0f});
bool NetworkLineClipped(CGameContext *pGameServer, int SnappingClient, vec2 From, vec2 To, vec2 MinView = {0.0f, 0.0f});
bool NetworkRectClipped(CGameContext *pGameServer, int SnappingClient, vec2 TL, vec2 BR, vec2 MinView = {0.0f, 0.0f});

#endif
