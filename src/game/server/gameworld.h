/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEWORLD_H
#define GAME_SERVER_GAMEWORLD_H

#include "eventhandler.h"
#include <game/gamecore.h>

#include <list>

class CEntity;
class CCharacter;

/*
	Class: Game World
		Tracks all entities in the game. Propagates tick and
		snap calls to all entities.
*/
class CGameWorld
{
public:
	enum
	{
		// findable entities
		ENTTYPE_PROJECTILE = 0,
		ENTTYPE_LASER,
		ENTTYPE_PICKUP,
		ENTTYPE_FLAG,
		ENTTYPE_CHARACTER,

		// entity collections
		ENTTYPE_DDRACE,
		ENTTYPE_CUSTOM,

		NUM_ENTTYPES
	};

private:
	int m_ResponsibleTeam;
	CEventHandler m_Events;
	void Reset();
	void RemoveEntities();

	CEntity *m_pNextTraverseEntity;
	CEntity *m_apFirstEntityTypes[NUM_ENTTYPES];

	class CGameContext *m_pGameServer;
	class IGameController *m_pController;
	class CConfig *m_pConfig;
	class IServer *m_pServer;

public:
	class CGameContext *GameServer() { return m_pGameServer; }
	class IGameController *Controller() { return m_pController; }
	class CConfig *Config() { return m_pConfig; }
	class IServer *Server() { return m_pServer; }

	int Team() { return m_ResponsibleTeam; }

	bool m_ResetRequested;
	bool m_Paused;
	CWorldCore m_Core;

	CGameWorld(int Team, CGameContext *pGameServer, IGameController *pController);
	~CGameWorld();

	CEntity *FindFirst(int Type);

	/*
		Function: find_entities
			Finds entities close to a position and returns them in a list.

		Arguments:
			pos - Position.
			radius - How close the entities have to be.
			ents - Pointer to a list that should be filled with the pointers
				to the entities.
			max - Number of entities that fits into the ents array.
			type - Type of the entities to find.

		Returns:
			Number of entities found and added to the ents array.
	*/
	int FindEntities(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Type);

	/*
		Function: closest_CEntity
			Finds the closest CEntity of a type to a specific point.
		Arguments:
			pos - The center position.
			radius - How far off the CEntity is allowed to be
			type - Type of the entities to find.
			notthis - Entity to ignore
		Returns:
			Returns a pointer to the closest CEntity or NULL if no CEntity is close enough.
	*/
	CEntity *ClosestEntity(vec2 Pos, float Radius, int Type, CEntity *pNotThis);

	/*
		Function: InterserctCharacter
			Finds the CCharacters that intersects the line. // made for types lasers=1 and doors=0

		Arguments:
			pos0 - Start position
			pos2 - End position
			radius - How for from the line the CCharacter is allowed to be.
			notthis - Entity to ignore intersecting with
			ignoresolo - Set to false to include solo characters

		Returns:
			Returns a pointer to the closest hit or NULL of there is no intersection.
	*/
	// class CCharacter *IntersectCharacter(vec2 Pos0, vec2 Pos1, float Radius, vec2 &NewPos, class CEntity *pNotThis = 0);
	class CCharacter *IntersectCharacter(vec2 Pos0, vec2 Pos1, float Radius, class CCharacter *pNotThis = 0, bool IgnoreSolo = true);

	/*
		Function: IntersectThisCharacter
			Finds the CCharacters that intersects the line. // made for types lasers=1 and doors=0

		Arguments:
			pos0 - Start position
			pos2 - End position
			radius - How for from the line the CCharacter is allowed to be
			char - Entity to check intersection with

		Returns:
			true if intersect, otherwise false.
	*/
	bool IntersectThisCharacter(vec2 Pos0, vec2 Pos1, float Radius, class CCharacter *pChar = 0);

	/*
		Function: insert_entity
			Adds an entity to the world.

		Arguments:
			entity - Entity to add
	*/
	void InsertEntity(CEntity *pEntity);

	/*
		Function: remove_entity
			Removes an entity from the world.

		Arguments:
			entity - Entity to remove
	*/
	void RemoveEntity(CEntity *pEntity);

	/*
		Function: snap
			Calls snap on all the entities in the world to create
			the snapshot.

		Arguments:
			snapping_client - ID of the client which snapshot
			is being created.
	*/
	void Snap(int SnappingClient, int OtherMode);

	/*
		Function: tick
			Calls tick on all the entities in the world to progress
			the world to the next tick.

	*/
	void Tick();

	void OnPostSnap();

	// DDRace
	void ReleaseHooked(int ClientID);

	/*
		Function: interserct_CCharacters
			Finds all CCharacters that intersect the line.

		Arguments:
			pos0 - Start position
			pos2 - End position
			radius - How for from the line the CCharacter is allowed to be.
			new_pos - Intersection position
			notthis - Entity to ignore intersecting with

		Returns:
			Returns list with all Characters on line.
	*/
	std::list<class CCharacter *> IntersectedCharacters(vec2 Pos0, vec2 Pos1, float Radius, class CEntity *pNotThis = 0, bool IgnoreSolo = true);

	// helper functions
	void CreateDamageIndCircle(vec2 Pos, bool Clockwise, float AngleMod, int Amount, int Total, float RadiusScale = 1.0f, int64 Mask = -1LL);
	void CreateDamageInd(vec2 Pos, float AngleMod, int Amount, int64 Mask = -1LL);
	void CreateExplosionParticle(vec2 Pos, int64 Mask = -1LL);
	void CreateExplosion(vec2 Pos, int Owner, int Weapon, int WeaponType, int Damage, bool NoKnockback, int64 Mask = -1LL);
	void CreateHammerHit(vec2 Pos, int64 Mask = -1LL);
	void CreatePlayerSpawn(vec2 Pos, int64 Mask = -1LL);
	void CreateDeath(vec2 Pos, int Who, int64 Mask = -1LL);
	void CreateSound(vec2 Pos, int Sound, int64 Mask = -1LL);
	void CreateSoundGlobal(int Sound, int64 Mask = -1LL);
};

#endif
