/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_CHARACTER_H
#define GAME_SERVER_ENTITIES_CHARACTER_H

#include <engine/antibot.h>
#include <game/server/entity.h>
#include <game/server/weapon.h>

class CAntibot;
class CGameTeams;
struct CAntibotCharacterData;

enum
{
	FAKETUNE_FREEZE = 1,
	FAKETUNE_SOLO = 2,
	FAKETUNE_NOJUMP = 4,
	FAKETUNE_NOCOLL = 8,
	FAKETUNE_NOHOOK = 16,
	FAKETUNE_JETPACK = 32,
	FAKETUNE_NOHAMMER = 64,
};

enum
{
	AMMO_UNLIMITED = -1
};

enum
{
	NUM_WEAPON_SLOTS = NUM_WEAPONS - 1,
};

enum
{
	// regen current weapon, reload before switch (vanilla server, recommended for deathmatch etc)
	WEAPON_TIMER_GLOBAL = 0,

	// regen all weapons, quick switch (recommended for custom mods)
	WEAPON_TIMER_INDIVIDUAL,

	// regen all weapons, quick switch, hold + switch = fire (noby server, not recommended, but it is there if you want it like noby's server)
	WEAPON_TIMER_INDIVIDUAL_QUICKFIRE,
};

class CCharacter : public CEntity
{
	MACRO_ALLOC_POOL_ID()

public:
	//character's size
	static const int ms_PhysSize = 28;

	bool m_Infection = false;
	CCharacter(CGameWorld *pWorld);
	~CCharacter();

	virtual void Reset() override;
	virtual void Destroy() override;
	virtual void Tick() override;
	virtual void TickDefered() override;
	virtual void TickPaused() override;
	virtual bool NetworkClipped(int SnappingClient) override;
	virtual void Snap(int SnappingClient, int OtherMode) override;

	bool IsGrounded();

	void SetWeaponSlot(int W, bool WithSound);
	void SetSolo(bool Solo);
	void HandleWeaponSwitch();
	void DoWeaponSwitch();

	class CWeapon *CurrentWeapon();
	void HandleWeapons();

	void OnPredictedInput(CNetObj_PlayerInput *pNewInput);
	void OnDirectInput(CNetObj_PlayerInput *pNewInput);
	void ResetHook();
	void ResetInput();
	void FireWeapon();

	void Die(int Killer, int Weapon);
	bool TakeDamage(vec2 Force, int Dmg, int From, int Weapon, int WeaponType, bool IsExplosion);

	void Infection(int ClientID);

	bool Spawn(class CPlayer *pPlayer, vec2 Pos);
	bool Remove();

	bool IncreaseHealth(int Amount);
	bool IncreaseArmor(int Amount);

	bool RemoveWeapon(int Slot);
	bool GiveWeapon(int Slot, int Type, int Ammo = -1);
	void ForceSetWeapon(int Slot, int Type, int Ammo = -1);
	void SetOverrideWeapon(int Slot, int Type, int Ammo = -1);
	void SetPowerUpWeapon(int Type, int Ammo = -1);

	void SetEndlessHook(bool Enable);

	void SetEmote(int Emote, int Tick);

	int NeededFaketuning() { return m_NeededFaketuning; }
	bool IsAlive() const { return m_Alive; }
	bool IsDisabled() const { return m_Disabled; }
	class CPlayer *GetPlayer() { return m_pPlayer; }

private:
	// player controlling this character
	class CPlayer *m_pPlayer;

	bool m_Alive;
	bool m_Disabled;
	int m_WeaponTimerType;
	bool m_FreezeWeaponSwitch;
	int m_NeededFaketuning;

	CWeapon *m_pPowerupWeapon;
	CWeapon *m_apOverrideWeaponSlots[NUM_WEAPONS - 1];
	CWeapon *m_apWeaponSlots[NUM_WEAPONS - 1];

	int m_LastWeaponSlot;
	int m_QueuedWeaponSlot;
	int m_ActiveWeaponSlot;

	int m_DamageTaken;

	int m_EmoteType;
	int m_EmoteStop;

	// last tick that the player took any action ie some input
	int m_LastAction;

	// these are non-heldback inputs
	CNetObj_PlayerInput m_LatestPrevPrevInput;
	CNetObj_PlayerInput m_LatestPrevInput;
	CNetObj_PlayerInput m_LatestInput;

	// input
	CNetObj_PlayerInput m_PrevInput;
	CNetObj_PlayerInput m_Input;
	CNetObj_PlayerInput m_SavedInput;
	int m_NumInputs;
	int m_Jumped;

	int m_DamageTakenTick;

	int m_Health;
	int m_Armor;

	// the player core for the physics
	CCharacterCore m_Core;

	// info for dead reckoning
	int m_ReckoningTick; // tick that we are performing dead reckoning From
	CCharacterCore m_SendCore; // core that we should send
	CCharacterCore m_ReckoningCore; // the dead reckoning core

	// DDRace

	void SnapCharacter(int SnappingClient, int ID);
	static bool IsSwitchActiveCb(int Number, void *pUser);
	void HandleTiles(int Index);
	float m_Time;
	int m_LastBroadcast;
	void DDRaceInit();
	void HandleSkippableTiles(int Index);
	void SetRescue();
	void DDRaceTick();
	void DDRacePostCoreTick();
	void HandleBroadcast();
	void HandleTuneLayer();
	void SendZoneMsgs();
	IAntibot *Antibot();

	bool m_Solo;

public:
	void FillAntibot(CAntibotCharacterData *pData);
	void SetDisable(bool Pause);
	bool Freeze(float Time, bool BlockHoldFire = false);
	bool IsFrozen();
	bool IsDeepFrozen();
	bool UnFreeze();
	bool ReduceFreeze(float Time);
	bool DeepFreeze();
	bool UndeepFreeze();
	void SetAllowFrozenWeaponSwitch(bool Allow);
	void RemoveWeapons();
	int m_DDRaceState;
	int Team();
	bool CanCollide(int ClientID);
	bool IsSolo();
	bool m_Super;
	bool m_SuperJump;
	bool m_Jetpack;
	int m_TeamBeforeSuper;
	int m_FreezeTime;
	int m_FreezeTick;
	bool m_FrozenLastTick;
	bool m_DeepFreeze;
	bool m_EndlessHook;
	int m_IsFiring;

	enum
	{
		SHOTGUN_VANILLA = 0,
		SHOTGUN_NORMAL_PULL = 1,
		SHOTGUN_SHOOTER_PULL = 2,
	};
	int m_PullingShotgun;

	enum
	{
		HIT_ALL = 0,
		DISABLE_HIT_HAMMER = 1,
		DISABLE_HIT_SHOTGUN = 2,
		DISABLE_HIT_GRENADE = 4,
		DISABLE_HIT_LASER = 8,
		DISABLE_HIT_GUN = 16
	};
	int m_Hit;
	int m_TuneZone;
	int m_TuneZoneOld;
	int m_LastMove;
	int m_StartTime;
	vec2 m_PrevPos;
	int m_TeleCheckpoint;
	int m_CpTick;
	int m_CpActive;
	int m_CpLastBroadcast;
	float m_CpCurrent[25];
	int m_TileIndex;
	int m_TileFIndex;

	int m_MoveRestrictions;

	int64 m_LastStartWarning;
	int64 m_LastRescue;
	bool m_LastRefillJumps;
	bool m_LastPenalty;
	bool m_LastBonus;
	vec2 m_TeleGunPos;
	bool m_TeleGunTeleport;
	bool m_IsBlueTeleGunTeleport;

	int m_SpawnTick;
	int m_WeaponChangeTick;
	int m_ProtectTick;
	int m_ProtectStartTick;
	bool m_ProtectCancelOnFire;
	bool m_FreezeAllowHoldFire;

	// Setters/Getters because i don't want to modify vanilla vars access modifiers
	int GetLastWeapon() { return m_LastWeaponSlot; };
	void SetLastWeapon(int LastWeap) { m_LastWeaponSlot = LastWeap; };
	int GetActiveWeapon() { return m_ActiveWeaponSlot; };
	void SetActiveWeapon(int ActiveWeap) { m_ActiveWeaponSlot = ActiveWeap; };
	void SetLastAction(int LastAction) { m_LastAction = LastAction; };
	int GetArmor() { return m_Armor; };
	void SetArmor(int Armor) { m_Armor = Armor; };
	CCharacterCore GetCore() { return m_Core; };
	void SetCore(CCharacterCore Core) { m_Core = Core; };
	CCharacterCore *Core() { return &m_Core; };
	bool IsAlive() { return m_Alive; };
	void SetWeaponTimerType(int Type) { m_WeaponTimerType = Type; }
	CWeapon *GetWeapon(int Slot) { return m_apWeaponSlots[Slot]; }
	CWeapon *GetOverrideWeapon(int Slot) { return m_apOverrideWeaponSlots[Slot]; }
	CWeapon *GetPowerupWeapon() { return m_pPowerupWeapon; }
	void Protect(float Seconds, bool CancelOnFire = false);
	bool IsProtected();

	int GetLastAction() const
	{
		return m_LastAction;
	}

	bool HasTelegunGun() { return m_Core.m_HasTelegunGun; };
	bool HasTelegunGrenade() { return m_Core.m_HasTelegunGrenade; };
	bool HasTelegunLaser() { return m_Core.m_HasTelegunLaser; };

	CTuningParams *CurrentTuning() { return m_TuneZone ? &GameServer()->TuningList()[m_TuneZone] : GameServer()->Tuning(); }
	// hit data
	struct
	{
		vec2 m_Intersection;
		float m_HitDistance;
		int m_HitOrder;
		bool m_FirstImpact;
	} m_HitData;
};

enum
{
	DDRACE_NONE = 0,
	DDRACE_STARTED,
	DDRACE_CHEAT, // no time and won't start again unless ordered by a mod or death
	DDRACE_FINISHED
};

#endif
