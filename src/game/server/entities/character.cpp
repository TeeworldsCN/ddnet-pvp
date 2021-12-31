/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <antibot/antibot_data.h>
#include <engine/shared/config.h>
#include <game/generated/server_data.h>
#include <game/mapitems.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <new>

#include "character.h"
#include "laser.h"
#include "projectile.h"

#include "light.h"
#include <game/server/teams.h>
#include <game/server/weapons.h>

MACRO_ALLOC_POOL_ID_IMPL(CCharacter, MAX_CLIENTS)

// Character, "physical" player's part
CCharacter::CCharacter(CGameWorld *pWorld) :
	CEntity(pWorld, CGameWorld::ENTTYPE_CHARACTER, vec2(0, 0), ms_PhysSize)
{
	m_Health = 0;
	m_Armor = 0;
	m_WeaponTimerType = WEAPON_TIMER_GLOBAL;
	m_FreezeWeaponSwitch = false;
	m_ProtectTick = 0;
	m_ProtectStartTick = 0;

	// never intilize both to zero
	m_Input.m_TargetX = 0;
	m_Input.m_TargetY = -1;
	m_IsFiring = false;

	m_LatestPrevPrevInput = m_LatestPrevInput = m_LatestInput = m_PrevInput = m_SavedInput = m_Input;
}

CCharacter::~CCharacter()
{
	SetWeaponSlot(WEAPON_GAME, false);
	for(int i = 0; i < NUM_WEAPON_SLOTS; i++)
	{
		if(m_apWeaponSlots[i])
			delete m_apWeaponSlots[i];

		if(m_apOverrideWeaponSlots[i])
			delete m_apOverrideWeaponSlots[i];
	}

	if(m_pPowerupWeapon)
	{
		m_pPowerupWeapon->OnUnequip();
		delete m_pPowerupWeapon;
	}
}

void CCharacter::Reset()
{
	Destroy();
}

bool CCharacter::Spawn(CPlayer *pPlayer, vec2 Pos)
{
	m_EmoteStop = -1;
	m_LastAction = -1;
	m_LastWeaponSlot = WEAPON_HAMMER;
	m_QueuedWeaponSlot = -1;
	m_LastRefillJumps = false;
	m_LastPenalty = false;
	m_LastBonus = false;

	mem_zero(m_apOverrideWeaponSlots, sizeof(m_apOverrideWeaponSlots));
	mem_zero(m_apWeaponSlots, sizeof(m_apWeaponSlots));
	m_pPowerupWeapon = nullptr;

	m_TeleGunTeleport = false;
	m_IsBlueTeleGunTeleport = false;
	m_Solo = false;

	m_pPlayer = pPlayer;
	m_Pos = Pos;

	mem_zero(&m_LatestPrevPrevInput, sizeof(m_LatestPrevPrevInput));
	m_LatestPrevPrevInput.m_TargetY = -1;
	m_SpawnTick = Server()->Tick();
	m_WeaponChangeTick = Server()->Tick();
	Antibot()->OnSpawn(m_pPlayer->GetCID());

	m_Core.Reset();
	m_Core.Init(&GameWorld()->m_Core, GameServer()->Collision(), &GameServer()->Teams()->m_Core);
	m_ActiveWeaponSlot = WEAPON_GAME;
	m_Core.m_Pos = m_Pos;
	GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;

	m_ReckoningTick = 0;
	mem_zero(&m_SendCore, sizeof(m_SendCore));
	mem_zero(&m_ReckoningCore, sizeof(m_ReckoningCore));

	GameWorld()->InsertEntity(this);
	m_Alive = true;

	Controller()->OnInternalCharacterSpawn(this);

	DDRaceInit();

	m_TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(Pos));
	m_TuneZoneOld = -1; // no zone leave msg on spawn
	m_NeededFaketuning = 0; // reset fake tunings on respawn and send the client
	SendZoneMsgs(); // we want a entermessage also on spawn
	GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);

	Server()->StartRecord(m_pPlayer->GetCID());

	return true;
}

void CCharacter::Destroy()
{
	GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	m_Alive = false;
	m_Solo = false;
}

void CCharacter::SetWeaponSlot(int W, bool WithSound)
{
	if(W == m_ActiveWeaponSlot)
		return;

	m_LastWeaponSlot = m_ActiveWeaponSlot;
	m_QueuedWeaponSlot = -1;
	m_ActiveWeaponSlot = W;

	if(m_LastWeaponSlot >= 0 && m_LastWeaponSlot < NUM_WEAPON_SLOTS)
	{
		if(m_apOverrideWeaponSlots[m_LastWeaponSlot])
			m_apOverrideWeaponSlots[m_LastWeaponSlot]->OnUnequip();
		else if(m_apWeaponSlots[m_LastWeaponSlot])
			m_apWeaponSlots[m_LastWeaponSlot]->OnUnequip();
	}

	if(m_ActiveWeaponSlot >= 0 && m_ActiveWeaponSlot < NUM_WEAPON_SLOTS)
	{
		if(m_apOverrideWeaponSlots[m_ActiveWeaponSlot])
			m_apOverrideWeaponSlots[m_ActiveWeaponSlot]->OnEquip();
		else if(m_apWeaponSlots[m_ActiveWeaponSlot])
			m_apWeaponSlots[m_ActiveWeaponSlot]->OnEquip();
	}

	if(WithSound)
		GameWorld()->CreateSound(m_Pos, SOUND_WEAPON_SWITCH);

	if(m_WeaponTimerType == WEAPON_TIMER_INDIVIDUAL)
		m_IsFiring = false;
}

void CCharacter::SetSolo(bool Solo)
{
	m_Solo = Solo;
	m_Core.m_Solo = Solo;
	Teams()->m_Core.SetSolo(m_pPlayer->GetCID(), Solo);

	if(Solo)
		m_NeededFaketuning |= FAKETUNE_SOLO;
	else
		m_NeededFaketuning &= ~FAKETUNE_SOLO;

	GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
}

bool CCharacter::IsGrounded()
{
	if(GameServer()->Collision()->CheckPoint(m_Pos.x + GetProximityRadius() / 2, m_Pos.y + GetProximityRadius() / 2 + 5))
		return true;
	if(GameServer()->Collision()->CheckPoint(m_Pos.x - GetProximityRadius() / 2, m_Pos.y + GetProximityRadius() / 2 + 5))
		return true;

	int MoveRestrictionsBelow = GameServer()->Collision()->GetMoveRestrictions(m_Pos + vec2(0, GetProximityRadius() / 2 + 4), 0.0f);
	if(MoveRestrictionsBelow & CANTMOVE_DOWN)
	{
		return true;
	}

	return false;
}

void CCharacter::DoWeaponSwitch()
{
	// make sure we can switch
	if(m_QueuedWeaponSlot == -1 || m_pPowerupWeapon || !m_apWeaponSlots[m_QueuedWeaponSlot])
		return;

	CWeapon *pCurrentWeapon = CurrentWeapon();
	// only block switch during reload with global timer
	if(m_WeaponTimerType == WEAPON_TIMER_GLOBAL && pCurrentWeapon && pCurrentWeapon->IsReloading())
		return;

	// switch Weapon
	SetWeaponSlot(m_QueuedWeaponSlot, true);
}

void CCharacter::HandleWeaponSwitch()
{
	bool Anything = false;
	for(int i = 0; i < NUM_WEAPON_SLOTS; ++i)
		if(m_apWeaponSlots[i])
			Anything = true;

	if(!Anything)
		return;

	if(IsFrozen() && !m_FreezeWeaponSwitch)
	{
		m_QueuedWeaponSlot = m_ActiveWeaponSlot;
		return;
	}

	int WantedWeapon = m_ActiveWeaponSlot;
	if(m_QueuedWeaponSlot != -1)
		WantedWeapon = m_QueuedWeaponSlot;

	// select Weapon
	int Next = CountInput(m_LatestPrevInput.m_NextWeapon, m_LatestInput.m_NextWeapon).m_Presses;
	int Prev = CountInput(m_LatestPrevInput.m_PrevWeapon, m_LatestInput.m_PrevWeapon).m_Presses;

	if(Next < 128) // make sure we only try sane stuff
	{
		while(Next) // Next Weapon selection
		{
			WantedWeapon = (WantedWeapon + 1) % NUM_WEAPON_SLOTS;
			if(m_apWeaponSlots[WantedWeapon])
				Next--;
		}
	}

	if(Prev < 128) // make sure we only try sane stuff
	{
		while(Prev) // Prev Weapon selection
		{
			WantedWeapon = (WantedWeapon - 1) < 0 ? NUM_WEAPON_SLOTS - 1 : WantedWeapon - 1;
			if(m_apWeaponSlots[WantedWeapon])
				Prev--;
		}
	}

	// Direct Weapon selection
	if(m_LatestInput.m_WantedWeapon)
		WantedWeapon = m_Input.m_WantedWeapon - 1;

	// check for insane values
	if(WantedWeapon >= 0 && WantedWeapon < NUM_WEAPON_SLOTS && WantedWeapon != m_ActiveWeaponSlot && m_apWeaponSlots[WantedWeapon])
		m_QueuedWeaponSlot = WantedWeapon;

	DoWeaponSwitch();
}

void CCharacter::FireWeapon()
{
	DoWeaponSwitch();

	CWeapon *pCurrentWeapon = CurrentWeapon();
	if(!pCurrentWeapon)
		return;

	if(pCurrentWeapon->IsReloading())
	{
		if(m_LatestInput.m_Fire & 1)
		{
			Antibot()->OnHammerFireReloading(m_pPlayer->GetCID());
		}
		return;
	}

	vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));

	// check if we gonna fire
	bool WillFire = false;
	CInputCount Input = CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire);
	if(Input.m_Presses)
	{
		WillFire = true;
		m_IsFiring = true;
	}

	if(!(m_LatestInput.m_Fire & 1))
		m_IsFiring = false;

	// cancel firing if not allowed to hold fire during freeze
	if(IsFrozen() && !m_FreezeAllowHoldFire)
		m_IsFiring = false;

	if((pCurrentWeapon->IsFullAuto() || m_FrozenLastTick) && m_IsFiring)
		WillFire = true;

	if(!WillFire || IsFrozen())
		return;

	pCurrentWeapon->HandleFire(Direction);
	if(m_ProtectCancelOnFire && m_ProtectTick > 1)
		m_ProtectTick = 1;
}

CWeapon *CCharacter::CurrentWeapon()
{
	CWeapon *pCurrentWeapon = m_pPowerupWeapon;

	if(m_ActiveWeaponSlot < 0 || m_ActiveWeaponSlot >= NUM_WEAPON_SLOTS)
		return pCurrentWeapon;

	if(!m_pPowerupWeapon)
	{
		pCurrentWeapon = m_apWeaponSlots[m_ActiveWeaponSlot];
		if(!pCurrentWeapon)
			return nullptr;

		if(m_apOverrideWeaponSlots[m_ActiveWeaponSlot])
			pCurrentWeapon = m_apOverrideWeaponSlots[m_ActiveWeaponSlot];
	}

	return pCurrentWeapon;
}

void CCharacter::HandleWeapons()
{
	CWeapon *pCurrentWeapon = CurrentWeapon();
	if(!pCurrentWeapon)
		return;

	if(pCurrentWeapon == m_pPowerupWeapon && m_pPowerupWeapon->IsPowerupOver())
	{
		SetPowerUpWeapon(WEAPON_ID_NONE);
		pCurrentWeapon = CurrentWeapon();
		if(!pCurrentWeapon)
			return;
	}

	// fire Weapon, if wanted
	FireWeapon();
	if(m_WeaponTimerType == WEAPON_TIMER_GLOBAL)
	{
		pCurrentWeapon->Tick();
	}
	else
	{
		for(int i = 0; i < NUM_WEAPON_SLOTS; i++)
		{
			if(m_apWeaponSlots[i])
				m_apWeaponSlots[i]->Tick();
			if(m_apOverrideWeaponSlots[i])
				m_apWeaponSlots[i]->Tick();
		}
		if(m_pPowerupWeapon)
			m_pPowerupWeapon->Tick();
	}
	return;
}

void CCharacter::SetEmote(int Emote, int Tick)
{
	m_EmoteType = Emote;
	m_EmoteStop = Tick;
}

void CCharacter::OnPredictedInput(CNetObj_PlayerInput *pNewInput)
{
	// check for changes
	if(mem_comp(&m_SavedInput, pNewInput, sizeof(CNetObj_PlayerInput)) != 0)
		m_LastAction = Server()->Tick();

	// copy new input
	mem_copy(&m_Input, pNewInput, sizeof(m_Input));
	m_NumInputs++;

	// it is not allowed to aim in the center
	if(m_Input.m_TargetX == 0 && m_Input.m_TargetY == 0)
		m_Input.m_TargetY = -1;

	mem_copy(&m_SavedInput, &m_Input, sizeof(m_SavedInput));
}

void CCharacter::OnDirectInput(CNetObj_PlayerInput *pNewInput)
{
	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
	mem_copy(&m_LatestInput, pNewInput, sizeof(m_LatestInput));

	// it is not allowed to aim in the center
	if(m_LatestInput.m_TargetX == 0 && m_LatestInput.m_TargetY == 0)
		m_LatestInput.m_TargetY = -1;

	Antibot()->OnDirectInput(m_pPlayer->GetCID());

	if(m_NumInputs > 2 && m_pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		HandleWeaponSwitch();
		FireWeapon();
	}

	mem_copy(&m_LatestPrevPrevInput, &m_LatestPrevInput, sizeof(m_LatestInput));
	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
}

void CCharacter::ResetHook()
{
	m_Core.m_HookedPlayer = -1;
	m_Core.m_HookState = HOOK_RETRACTED;
	m_Core.m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
	m_Core.m_HookPos = m_Core.m_Pos;
}

void CCharacter::ResetInput()
{
	m_Input.m_Direction = 0;
	//m_Input.m_Hook = 0;
	// simulate releasing the fire button
	if((m_Input.m_Fire & 1) != 0)
		m_Input.m_Fire++;
	m_Input.m_Fire &= INPUT_STATE_MASK;
	m_Input.m_Jump = 0;
	m_LatestPrevInput = m_LatestInput = m_Input;
}

void CCharacter::Tick()
{
	if(m_Disabled)
		return;

	// set emote
	if(m_EmoteStop < Server()->Tick())
	{
		m_EmoteType = m_pPlayer->GetDefaultEmote();
		m_EmoteStop = -1;
	}

	DDRaceTick();

	Antibot()->OnCharacterTick(m_pPlayer->GetCID());

	m_Core.m_Input = m_Input;
	m_Core.Tick(true);

	if(!m_PrevInput.m_Hook && m_Input.m_Hook && !(m_Core.m_TriggeredEvents & COREEVENT_HOOK_ATTACH_PLAYER))
	{
		Antibot()->OnHookAttach(m_pPlayer->GetCID(), false);
	}

	// handle Weapons
	HandleWeapons();

	DDRacePostCoreTick();

	if(m_Core.m_TriggeredEvents & COREEVENT_HOOK_ATTACH_PLAYER)
	{
		if(m_Core.m_HookedPlayer != -1 && GameServer()->m_apPlayers[m_Core.m_HookedPlayer]->GetTeam() != -1)
		{
			Antibot()->OnHookAttach(m_pPlayer->GetCID(), true);
		}
	}

	// Previnput
	m_PrevInput = m_Input;

	m_PrevPos = m_Core.m_Pos;
	return;
}

void CCharacter::TickDefered()
{
	// advance the dummy
	{
		CWorldCore TempWorld;
		m_ReckoningCore.Init(&TempWorld, GameServer()->Collision(), &Teams()->m_Core);
		m_ReckoningCore.m_Id = m_pPlayer->GetCID();
		m_ReckoningCore.Tick(false);
		m_ReckoningCore.Move();
		m_ReckoningCore.Quantize();
	}

	// apply drag velocity when the player is not firing ninja
	// and set it back to 0 for the next tick
	CWeapon *pCurrentWeapon = CurrentWeapon();
	if(pCurrentWeapon && !pCurrentWeapon->IgnoreHookDrag())
		m_Core.AddDragVelocity();
	m_Core.ResetDragVelocity();

	//lastsentcore
	vec2 StartPos = m_Core.m_Pos;
	vec2 StartVel = m_Core.m_Vel;
	bool StuckBefore = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));

	m_Core.m_Id = m_pPlayer->GetCID();
	m_Core.Move();
	bool StuckAfterMove = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));
	m_Core.Quantize();
	bool StuckAfterQuant = GameServer()->Collision()->TestBox(m_Core.m_Pos, vec2(28.0f, 28.0f));
	m_Pos = m_Core.m_Pos;

	if(!StuckBefore && (StuckAfterMove || StuckAfterQuant))
	{
		// Hackish solution to get rid of strict-aliasing warning
		union
		{
			float f;
			unsigned u;
		} StartPosX, StartPosY, StartVelX, StartVelY;

		StartPosX.f = StartPos.x;
		StartPosY.f = StartPos.y;
		StartVelX.f = StartVel.x;
		StartVelY.f = StartVel.y;

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "STUCK!!! %d %d %d %f %f %f %f %x %x %x %x",
			StuckBefore,
			StuckAfterMove,
			StuckAfterQuant,
			StartPos.x, StartPos.y,
			StartVel.x, StartVel.y,
			StartPosX.u, StartPosY.u,
			StartVelX.u, StartVelY.u);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}

	{
		int Events = m_Core.m_TriggeredEvents;
		int CID = m_pPlayer->GetCID();

		// Some sounds are triggered client-side for the acting player
		// so we need to avoid duplicating them
		int64 ExceptSelf = CmaskAllExceptOne(CID);
		// Some are triggered client-side but only on Sixup
		int64 ExceptSelfIfSixup = Server()->IsSixup(CID) ? CmaskAllExceptOne(CID) : -1LL;

		if(Events & COREEVENT_GROUND_JUMP)
			GameWorld()->CreateSound(m_Pos, SOUND_PLAYER_JUMP, ExceptSelf);

		if(Events & COREEVENT_HOOK_ATTACH_PLAYER)
			GameWorld()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_PLAYER, ExceptSelfIfSixup);

		if(Events & COREEVENT_HOOK_ATTACH_GROUND)
			GameWorld()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_GROUND, ExceptSelf);

		if(Events & COREEVENT_HOOK_HIT_NOHOOK)
			GameWorld()->CreateSound(m_Pos, SOUND_HOOK_NOATTACH, ExceptSelf);
	}

	if(m_pPlayer->GetTeam() == TEAM_SPECTATORS)
	{
		m_Pos.x = m_Input.m_TargetX;
		m_Pos.y = m_Input.m_TargetY;
	}

	// update the m_SendCore if needed
	{
		CNetObj_Character Predicted;
		CNetObj_Character Current;
		mem_zero(&Predicted, sizeof(Predicted));
		mem_zero(&Current, sizeof(Current));
		m_ReckoningCore.Write(&Predicted);
		m_Core.Write(&Current);

		// only allow dead reackoning for a top of 3 seconds
		if(m_Core.m_pReset || m_ReckoningTick + Server()->TickSpeed() * 3 < Server()->Tick() || mem_comp(&Predicted, &Current, sizeof(CNetObj_Character)) != 0)
		{
			m_ReckoningTick = Server()->Tick();
			m_SendCore = m_Core;
			m_ReckoningCore = m_Core;
			m_Core.m_pReset = false;
		}
	}
}

void CCharacter::TickPaused()
{
	++m_DamageTakenTick;
	++m_ReckoningTick;
	if(m_LastAction != -1)
		++m_LastAction;
	if(m_EmoteStop > -1)
		++m_EmoteStop;

	for(int i = 0; i < NUM_WEAPON_SLOTS; i++)
	{
		if(m_apWeaponSlots[i])
			m_apWeaponSlots[i]->TickPaused();
		if(m_apOverrideWeaponSlots[i])
			m_apWeaponSlots[i]->TickPaused();
	}
	if(m_pPowerupWeapon)
		m_pPowerupWeapon->TickPaused();
}

bool CCharacter::IncreaseHealth(int Amount)
{
	if(m_Health >= 10)
		return false;
	m_Health = clamp(m_Health + Amount, 0, 10);
	return true;
}

bool CCharacter::IncreaseArmor(int Amount)
{
	if(m_Armor >= 10)
		return false;
	m_Armor = clamp(m_Armor + Amount, 0, 10);
	return true;
}

void CCharacter::Die(int Killer, int Weapon)
{
	if(Server()->IsRecording(m_pPlayer->GetCID()))
		Server()->StopRecord(m_pPlayer->GetCID());

	// a nice sound
	GameWorld()->CreateSound(m_Pos, SOUND_PLAYER_DIE);

	// this is to rate limit respawning to 3 secs
	m_pPlayer->m_PreviousDieTick = m_pPlayer->m_DieTick;
	m_pPlayer->m_DieTick = Server()->Tick();

	m_Alive = false;
	m_pPlayer->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() / 2;
	m_Solo = false;

	GameWorld()->RemoveEntity(this);
	GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
	GameWorld()->CreateDeath(m_Pos, m_pPlayer->GetCID());

	int DeathFlag = Controller()->OnInternalCharacterDeath(this, GameServer()->m_apPlayers[Killer], Weapon);
	int ModeSpecial = DeathFlag & (DEATH_KILLER_HAS_FLAG | DEATH_VICTIM_HAS_FLAG);

	if(!(DeathFlag & DEATH_NO_KILL_MSG))
		Controller()->SendKillMsg(Killer, m_pPlayer->GetCID(), Weapon, ModeSpecial);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "kill killer='%d:%s' victim='%d:%s' weapon=%d special=%d",
		Killer, Server()->ClientName(Killer),
		m_pPlayer->GetCID(), Server()->ClientName(m_pPlayer->GetCID()), Weapon, ModeSpecial);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
}

bool CCharacter::TakeDamage(vec2 Force, int Dmg, int From, int Weapon, int WeaponID, bool IsExplosion)
{
	// m_pPlayer only inflicts half damage on self
	if(From == m_pPlayer->GetCID())
		Dmg = maximum(1, Dmg / 2);

	if(From >= 0)
	{
		if(Controller()->IsFriendlyFire(m_pPlayer->GetCID(), From))
			Dmg = 0;
	}

	if(m_ProtectTick > 0)
		Dmg = 0;

	int DamageFlag = Controller()->OnCharacterTakeDamage(this, Force, Dmg, From, Weapon, WeaponID, IsExplosion);

	if((DamageFlag & DAMAGE_SKIP) == DAMAGE_SKIP)
		return true;

	if(!(DamageFlag & DAMAGE_NO_KNOCKBACK))
	{
		vec2 Temp = m_Core.m_Vel + Force;
		m_Core.m_Vel = ClampVel(m_MoveRestrictions, Temp);
	}

	if(Dmg == 0)
		return false;

	if(!(DamageFlag & DAMAGE_NO_INDICATOR))
	{
		m_DamageTaken++;

		// create healthmod indicator
		if(Server()->Tick() < m_DamageTakenTick + 25)
		{
			// make sure that the damage indicators doesn't group together
			GameWorld()->CreateDamageInd(m_Pos, m_DamageTaken * 0.25f, Dmg);
		}
		else
		{
			m_DamageTaken = 0;
			GameWorld()->CreateDamageInd(m_Pos, 0, Dmg);
		}
	}

	if(!(DamageFlag & DAMAGE_NO_DAMAGE))
	{
		if(Dmg > 0)
		{
			if(m_Armor)
			{
				if(Dmg > 1)
				{
					m_Health--;
					Dmg--;
				}

				if(Dmg > m_Armor)
				{
					Dmg -= m_Armor;
					m_Armor = 0;
				}
				else
				{
					m_Armor -= Dmg;
					Dmg = 0;
				}
			}

			m_Health -= Dmg;
		}

		if(Dmg < 0)
		{
			m_Health = clamp(m_Health - Dmg, 0, 10);
		}

		m_DamageTakenTick = Server()->Tick();
	}

	if(!(DamageFlag & DAMAGE_NO_HITSOUND))
	{
		// do damage Hit sound
		if(From >= 0 && From != m_pPlayer->GetCID() && GameServer()->m_apPlayers[From])
		{
			int64 Mask = CmaskOneAndViewer(From);
			GameWorld()->CreateSound(GameServer()->m_apPlayers[From]->m_ViewPos, SOUND_HIT, Mask);
		}
	}

	if(!(DamageFlag & DAMAGE_NO_DEATH))
	{
		// check for death
		if(m_Health <= 0)
		{
			Die(From, Weapon);

			// set attacker's face to happy (taunt!)
			if(!(DamageFlag & DAMAGE_NO_EMOTE) && From >= 0 && From != m_pPlayer->GetCID() && GameServer()->m_apPlayers[From])
			{
				CCharacter *pChr = GameServer()->m_apPlayers[From]->GetCharacter();
				if(pChr)
				{
					pChr->m_EmoteType = EMOTE_HAPPY;
					pChr->m_EmoteStop = Server()->Tick() + Server()->TickSpeed();
				}
			}

			return false;
		}
	}

	if(!(DamageFlag & DAMAGE_NO_PAINSOUND))
	{
		if(Dmg > 2)
			GameWorld()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_LONG);
		else if(Dmg > 0)
			GameWorld()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_SHORT);
		else if(Dmg < 0)
			GameWorld()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
	}

	if(!(DamageFlag & DAMAGE_NO_EMOTE))
	{
		if(Dmg)
		{
			m_EmoteType = Dmg > 0 ? EMOTE_PAIN : EMOTE_HAPPY;
			m_EmoteStop = Server()->Tick() + 500 * Server()->TickSpeed() / 1000;
		}
	}

	return true;
}

//TODO: Move the emote stuff to a function
void CCharacter::SnapCharacter(int SnappingClient, int MappedID)
{
	CCharacterCore *pCore;

	CWeapon *pCurrentWeapon = CurrentWeapon();

	int Tick, Emote = m_EmoteType, Weapon = pCurrentWeapon ? pCurrentWeapon->GetType() : m_ActiveWeaponSlot, AmmoCount = 0,
		  Health = 0, Armor = 0, AttackTick = pCurrentWeapon ? pCurrentWeapon->GetAttackTick() : 0;

	if(!m_ReckoningTick || GameWorld()->m_Paused)
	{
		Tick = 0;
		pCore = &m_Core;
	}
	else
	{
		Tick = m_ReckoningTick;
		pCore = &m_SendCore;
	}

	// change eyes and use ninja graphic if player is frozen
	if(m_DeepFreeze || m_FreezeTime > 0)
	{
		if(Emote == EMOTE_NORMAL)
			Emote = m_DeepFreeze ? EMOTE_PAIN : EMOTE_BLINK;

		Weapon = WEAPON_NINJA;
	}

	// This could probably happen when m_Jetpack changes instead
	// jetpack and ninjajetpack prediction
	if(m_pPlayer->GetCID() == SnappingClient)
	{
		if(m_Jetpack && Weapon != WEAPON_NINJA)
		{
			if(!(m_NeededFaketuning & FAKETUNE_JETPACK))
			{
				m_NeededFaketuning |= FAKETUNE_JETPACK;
				GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);
			}
		}
		else
		{
			if(m_NeededFaketuning & FAKETUNE_JETPACK)
			{
				m_NeededFaketuning &= ~FAKETUNE_JETPACK;
				GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone);
			}
		}
	}

	if(m_pPlayer->GetCID() == SnappingClient || SnappingClient == -1 ||
		(!g_Config.m_SvStrictSpectateMode && m_pPlayer->GetCID() == GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID()))
	{
		Health = m_Health;
		Armor = m_Armor;
		if(pCurrentWeapon && pCurrentWeapon->NumAmmoIcons() > 0)
			AmmoCount = (!m_FreezeTime) ? pCurrentWeapon->NumAmmoIcons() : 0;
	}

	if(GetPlayer()->m_Afk || GetPlayer()->IsPaused())
	{
		if(m_FreezeTime > 0 || m_DeepFreeze)
			Emote = EMOTE_NORMAL;
		else
			Emote = EMOTE_BLINK;
	}

	if(Emote == EMOTE_NORMAL)
	{
		if(250 - ((Server()->Tick() - m_LastAction) % (250)) < 5)
			Emote = EMOTE_BLINK;
	}

	float NinjaProgress = 1.0f;
	bool ShowNinjaProgress = true;
	if(m_FreezeTime > 0)
		NinjaProgress = 1.0f - (m_FreezeTime / (float)(m_FreezeTime + (Server()->Tick() - m_FreezeTick)));
	else if(m_DeepFreeze)
		NinjaProgress = 1.0f;
	else if(Weapon == WEAPON_NINJA)
	{
		if(pCurrentWeapon)
			NinjaProgress = (1.0f - pCurrentWeapon->PowerupProgress());
		else
			NinjaProgress = 0.0f; // always full
	}
	else
		ShowNinjaProgress = false;

	NinjaProgress = clamp(NinjaProgress, 0.0f, 1.0f);

	// Protection indicator
	if(m_ProtectTick > 0)
	{
		int TotalTick = (Server()->Tick() - m_ProtectStartTick) + m_ProtectTick;
		float Progress = 1.0f - m_ProtectTick / (float)TotalTick;
		Health = Health * Progress;
		Armor = Armor * Progress;
		int Factor = 8;
		if(m_ProtectTick < Server()->TickSpeed())
			Factor = 4;
		else if(m_ProtectTick < Server()->TickSpeed() * 2)
			Factor = 5;
		else if(m_ProtectTick < Server()->TickSpeed() * 3)
			Factor = 6;
		else if(m_ProtectTick < Server()->TickSpeed() * 4)
			Factor = 7;

		if((Server()->Tick() - m_ProtectStartTick) / Factor % 2)
			Emote = EMOTE_NORMAL;
		else
			Emote = EMOTE_SURPRISE;
	}

	if(!Server()->IsSixup(SnappingClient))
	{
		CNetObj_Character *pCharacter = static_cast<CNetObj_Character *>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, MappedID, sizeof(CNetObj_Character)));
		if(!pCharacter)
			return;

		pCore->Write(pCharacter);

		pCharacter->m_Tick = Tick;
		pCharacter->m_Emote = Emote;

		if(pCharacter->m_HookedPlayer != -1)
		{
			if(!Server()->Translate(pCharacter->m_HookedPlayer, SnappingClient))
				pCharacter->m_HookedPlayer = -1;
		}

		// HACK: try disable weapon antiping when the weapon is non-standard or is using quick switch
		if(m_WeaponTimerType == WEAPON_TIMER_INDIVIDUAL && !IsFrozen() && (!pCurrentWeapon || pCurrentWeapon->GetType() != WEAPON_NINJA))
			AttackTick = AttackTick > Server()->Tick() - 25 ? AttackTick : Server()->Tick() - 12;
		else if(pCurrentWeapon)
		{
			int WeaponID = pCurrentWeapon->GetWeaponID();
			if(WeaponID != WEAPON_ID_HAMMER &&
				WeaponID != WEAPON_ID_PISTOL &&
				WeaponID != WEAPON_ID_SHOTGUN &&
				WeaponID != WEAPON_ID_GRENADE &&
				WeaponID != WEAPON_ID_LASER &&
				pCurrentWeapon->GetType() != WEAPON_NINJA)
				AttackTick = AttackTick > Server()->Tick() - 25 ? AttackTick : Server()->Tick() - 12;
		}

		pCharacter->m_AttackTick = AttackTick;
		pCharacter->m_Direction = m_Input.m_Direction;
		pCharacter->m_Weapon = Weapon;
		pCharacter->m_AmmoCount = AmmoCount;
		pCharacter->m_Health = Health;
		pCharacter->m_Armor = Armor;
		pCharacter->m_PlayerFlags = GetPlayer()->m_PlayerFlags;

		// HACK: no shaking during pause / round end
		if(GameWorld()->m_Paused)
		{
			pCharacter->m_VelX = 0;
			pCharacter->m_VelY = 0;
			pCharacter->m_AttackTick = 0;
		}

		// note: not used, 0.6 doesn't have a progress bar
		int NinjaDuration = g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000;

		if(ShowNinjaProgress)
			pCharacter->m_AmmoCount = Server()->Tick() + (int)(NinjaDuration * NinjaProgress);
	}
	else
	{
		protocol7::CNetObj_Character *pCharacter = static_cast<protocol7::CNetObj_Character *>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, MappedID, sizeof(protocol7::CNetObj_Character)));
		if(!pCharacter)
			return;

		pCore->Write(reinterpret_cast<CNetObj_CharacterCore *>(static_cast<protocol7::CNetObj_CharacterCore *>(pCharacter)));

		pCharacter->m_Tick = Tick;
		pCharacter->m_Emote = Emote;
		pCharacter->m_AttackTick = AttackTick;
		pCharacter->m_Direction = m_Input.m_Direction;
		pCharacter->m_Weapon = Weapon;
		pCharacter->m_AmmoCount = AmmoCount;

		int NinjaDuration = g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000;

		if(ShowNinjaProgress)
			pCharacter->m_AmmoCount = Server()->Tick() + (int)(NinjaDuration * NinjaProgress);

		pCharacter->m_Health = Health;
		pCharacter->m_Armor = Armor;
		pCharacter->m_TriggeredEvents = 0;
	}
}

bool CCharacter::NetworkClipped(int SnappingClient)
{
	return NetworkLineClipped(SnappingClient, m_Core.m_Pos, m_Core.m_HookPos);
}

void CCharacter::Snap(int SnappingClient, int OtherMode)
{
	int MappedID = m_pPlayer->GetCID();

	if(SnappingClient > -1 && !Server()->Translate(MappedID, SnappingClient))
		return;

	if(m_Disabled)
		return;

	SnapCharacter(SnappingClient, MappedID);

	CNetObj_DDNetCharacter *pDDNetCharacter = static_cast<CNetObj_DDNetCharacter *>(Server()->SnapNewItem(NETOBJTYPE_DDNETCHARACTER, MappedID, sizeof(CNetObj_DDNetCharacter)));
	if(!pDDNetCharacter)
		return;

	pDDNetCharacter->m_Flags = 0;
	if(m_Solo)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_SOLO;
	if(m_Super)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_SUPER;
	if(m_EndlessHook)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_ENDLESS_HOOK;
	if(!m_Core.m_Collision || !GameServer()->Tuning()->m_PlayerCollision)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_NO_COLLISION;
	if(!m_Core.m_Hook || !GameServer()->Tuning()->m_PlayerHooking)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_NO_HOOK;
	if(m_SuperJump)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_ENDLESS_JUMP;
	if(m_Jetpack)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_JETPACK;
	if(m_Hit & DISABLE_HIT_GRENADE)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_NO_GRENADE_HIT;
	if(m_Hit & DISABLE_HIT_HAMMER)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_NO_HAMMER_HIT;
	if(m_Hit & DISABLE_HIT_LASER)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_NO_LASER_HIT;
	if(m_Hit & DISABLE_HIT_SHOTGUN)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_NO_SHOTGUN_HIT;
	if(m_Core.m_HasTelegunGun)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_TELEGUN_GUN;
	if(m_Core.m_HasTelegunGrenade)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_TELEGUN_GRENADE;
	if(m_Core.m_HasTelegunLaser)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_TELEGUN_LASER;

	// we need this to make ninja behave normally
	CWeapon *pWeapon = CurrentWeapon();
	if(pWeapon && pWeapon->GetType() == WEAPON_NINJA)
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_WEAPON_NINJA;

	pDDNetCharacter->m_FreezeEnd = m_DeepFreeze ? -1 : m_FreezeTime == 0 ? 0 :
                                                                               Server()->Tick() + m_FreezeTime;
	pDDNetCharacter->m_Jumps = m_Core.m_Jumps;
	pDDNetCharacter->m_TeleCheckpoint = m_TeleCheckpoint;
	pDDNetCharacter->m_StrongWeakID = SnappingClient == m_pPlayer->GetCID() ? 1 : 0;
}

// DDRace

bool CCharacter::CanCollide(int ClientID)
{
	return Teams()->m_Core.CanCollide(GetPlayer()->GetCID(), ClientID);
}

int CCharacter::Team()
{
	return Teams()->m_Core.Team(m_pPlayer->GetCID());
}

bool CCharacter::IsSolo()
{
	return Teams()->m_Core.GetSolo(m_pPlayer->GetCID());
}

void CCharacter::FillAntibot(CAntibotCharacterData *pData)
{
	pData->m_Pos = m_Pos;
	pData->m_Vel = m_Core.m_Vel;
	pData->m_Angle = m_Core.m_Angle;
	pData->m_HookedPlayer = m_Core.m_HookedPlayer;
	pData->m_SpawnTick = m_SpawnTick;
	pData->m_WeaponChangeTick = m_WeaponChangeTick;
	pData->m_aLatestInputs[0].m_TargetX = m_LatestInput.m_TargetX;
	pData->m_aLatestInputs[0].m_TargetY = m_LatestInput.m_TargetY;
	pData->m_aLatestInputs[1].m_TargetX = m_LatestPrevInput.m_TargetX;
	pData->m_aLatestInputs[1].m_TargetY = m_LatestPrevInput.m_TargetY;
	pData->m_aLatestInputs[2].m_TargetX = m_LatestPrevPrevInput.m_TargetX;
	pData->m_aLatestInputs[2].m_TargetY = m_LatestPrevPrevInput.m_TargetY;
}

void CCharacter::HandleBroadcast()
{
	// CPlayerData *pData = GameServer()->Score()->PlayerData(m_pPlayer->GetCID());

	// if(m_DDRaceState == DDRACE_STARTED && m_CpLastBroadcast != m_CpActive &&
	// 	m_CpActive > -1 && m_CpTick > Server()->Tick() && m_pPlayer->GetClientVersion() == VERSION_VANILLA &&
	// 	pData->m_BestTime && pData->m_aBestCpTime[m_CpActive] != 0)
	// {
	// 	char aBroadcast[128];
	// 	float Diff = m_CpCurrent[m_CpActive] - pData->m_aBestCpTime[m_CpActive];
	// 	str_format(aBroadcast, sizeof(aBroadcast), "Checkpoint | Diff : %+5.2f", Diff);
	// 	GameServer()->SendBroadcast(aBroadcast, m_pPlayer->GetCID());
	// 	m_CpLastBroadcast = m_CpActive;
	// 	m_LastBroadcast = Server()->Tick();
	// }
	// else if((m_pPlayer->m_TimerType == CPlayer::TIMERTYPE_BROADCAST || m_pPlayer->m_TimerType == CPlayer::TIMERTYPE_GAMETIMER_AND_BROADCAST) && m_DDRaceState == DDRACE_STARTED && m_LastBroadcast + Server()->TickSpeed() * g_Config.m_SvTimeInBroadcastInterval <= Server()->Tick())
	// {
	// 	char aBuf[32];
	// 	int Time = (int64)100 * ((float)(Server()->Tick() - m_StartTime) / ((float)Server()->TickSpeed()));
	// 	str_time(Time, TIME_HOURS, aBuf, sizeof(aBuf));
	// 	GameServer()->SendBroadcast(aBuf, m_pPlayer->GetCID(), false);
	// 	m_CpLastBroadcast = m_CpActive;
	// 	m_LastBroadcast = Server()->Tick();
	// }
}

void CCharacter::HandleSkippableTiles(int Index)
{
	// handle death-tiles and leaving gamelayer
	if((GameServer()->Collision()->GetCollisionAt(m_Pos.x + GetProximityRadius() / 3.f, m_Pos.y - GetProximityRadius() / 3.f) == TILE_DEATH ||
		   GameServer()->Collision()->GetCollisionAt(m_Pos.x + GetProximityRadius() / 3.f, m_Pos.y + GetProximityRadius() / 3.f) == TILE_DEATH ||
		   GameServer()->Collision()->GetCollisionAt(m_Pos.x - GetProximityRadius() / 3.f, m_Pos.y - GetProximityRadius() / 3.f) == TILE_DEATH ||
		   GameServer()->Collision()->GetCollisionAt(m_Pos.x - GetProximityRadius() / 3.f, m_Pos.y + GetProximityRadius() / 3.f) == TILE_DEATH ||
		   GameServer()->Collision()->GetFCollisionAt(m_Pos.x + GetProximityRadius() / 3.f, m_Pos.y - GetProximityRadius() / 3.f) == TILE_DEATH ||
		   GameServer()->Collision()->GetFCollisionAt(m_Pos.x + GetProximityRadius() / 3.f, m_Pos.y + GetProximityRadius() / 3.f) == TILE_DEATH ||
		   GameServer()->Collision()->GetFCollisionAt(m_Pos.x - GetProximityRadius() / 3.f, m_Pos.y - GetProximityRadius() / 3.f) == TILE_DEATH ||
		   GameServer()->Collision()->GetFCollisionAt(m_Pos.x - GetProximityRadius() / 3.f, m_Pos.y + GetProximityRadius() / 3.f) == TILE_DEATH) &&
		!m_Super) // && !(Team() && Teams()->TeeFinished(m_pPlayer->GetCID())))
	{
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
		return;
	}

	if(GameLayerClipped(m_Pos))
	{
		Die(m_pPlayer->GetCID(), WEAPON_WORLD);
		return;
	}

	vec2 aOffsets[4] = {
		vec2(GetProximityRadius() / 3.f, -GetProximityRadius() / 3.f),
		vec2(GetProximityRadius() / 3.f, GetProximityRadius() / 3.f),
		vec2(-GetProximityRadius() / 3.f, -GetProximityRadius() / 3.f),
		vec2(-GetProximityRadius() / 3.f, GetProximityRadius() / 3.f),
	};

	for(int i = 0; i < 4; i++)
	{
		if(Controller()->OnCharacterProximateTile(this, GameServer()->Collision()->GetPureMapIndex(m_Pos + aOffsets[i])))
			return;
		if(!m_Alive)
			return;
	}

	if(Index < 0)
		return;

	// handle speedup tiles
	if(GameServer()->Collision()->IsSpeedup(Index))
	{
		vec2 Direction, TempVel = m_Core.m_Vel;
		int Force, MaxSpeed = 0;
		float TeeAngle, SpeederAngle, DiffAngle, SpeedLeft, TeeSpeed;
		GameServer()->Collision()->GetSpeedup(Index, &Direction, &Force, &MaxSpeed);
		if(Force == 255 && MaxSpeed)
		{
			m_Core.m_Vel = Direction * (MaxSpeed / 5);
		}
		else
		{
			if(MaxSpeed > 0 && MaxSpeed < 5)
				MaxSpeed = 5;
			if(MaxSpeed > 0)
			{
				if(Direction.x > 0.0000001f)
					SpeederAngle = -atan(Direction.y / Direction.x);
				else if(Direction.x < 0.0000001f)
					SpeederAngle = atan(Direction.y / Direction.x) + 2.0f * asin(1.0f);
				else if(Direction.y > 0.0000001f)
					SpeederAngle = asin(1.0f);
				else
					SpeederAngle = asin(-1.0f);

				if(SpeederAngle < 0)
					SpeederAngle = 4.0f * asin(1.0f) + SpeederAngle;

				if(TempVel.x > 0.0000001f)
					TeeAngle = -atan(TempVel.y / TempVel.x);
				else if(TempVel.x < 0.0000001f)
					TeeAngle = atan(TempVel.y / TempVel.x) + 2.0f * asin(1.0f);
				else if(TempVel.y > 0.0000001f)
					TeeAngle = asin(1.0f);
				else
					TeeAngle = asin(-1.0f);

				if(TeeAngle < 0)
					TeeAngle = 4.0f * asin(1.0f) + TeeAngle;

				TeeSpeed = sqrt(pow(TempVel.x, 2) + pow(TempVel.y, 2));

				DiffAngle = SpeederAngle - TeeAngle;
				SpeedLeft = MaxSpeed / 5.0f - cos(DiffAngle) * TeeSpeed;
				if(abs((int)SpeedLeft) > Force && SpeedLeft > 0.0000001f)
					TempVel += Direction * Force;
				else if(abs((int)SpeedLeft) > Force)
					TempVel += Direction * -Force;
				else
					TempVel += Direction * SpeedLeft;
			}
			else
				TempVel += Direction * Force;

			m_Core.m_Vel = ClampVel(m_MoveRestrictions, TempVel);
		}
	}
}

bool CCharacter::IsSwitchActiveCb(int Number, void *pUser)
{
	CCharacter *pThis = (CCharacter *)pUser;
	CCollision *pCollision = pThis->GameServer()->Collision();
	return pCollision->m_pSwitchers && pCollision->m_pSwitchers[Number].m_Status[pThis->Team()] && pThis->Team() != TEAM_SUPER;
}

void CCharacter::HandleTiles(int Index)
{
	int MapIndex = Index;
	//int PureMapIndex = GameServer()->Collision()->GetPureMapIndex(m_Pos);
	m_TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
	m_TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);
	m_MoveRestrictions = GameServer()->Collision()->GetMoveRestrictions(IsSwitchActiveCb, this, m_Pos, 18.0f, MapIndex);
	if(Index < 0)
	{
		m_LastRefillJumps = false;
		m_LastPenalty = false;
		m_LastBonus = false;
		return;
	}

	int tcp = GameServer()->Collision()->IsTCheckpoint(MapIndex);
	if(tcp)
		m_TeleCheckpoint = tcp;

	if(Controller()->OnInternalCharacterTile(this, Index))
		return;

	// freeze
	if(((m_TileIndex == TILE_FREEZE) || (m_TileFIndex == TILE_FREEZE)) && !m_Super && !m_DeepFreeze)
	{
		Freeze(3);
	}
	else if(((m_TileIndex == TILE_UNFREEZE) || (m_TileFIndex == TILE_UNFREEZE)) && !m_DeepFreeze)
	{
		UnFreeze();
	}

	// deep freeze
	if(((m_TileIndex == TILE_DFREEZE) || (m_TileFIndex == TILE_DFREEZE)) && !m_Super && !m_DeepFreeze)
	{
		m_DeepFreeze = true;
	}
	else if(((m_TileIndex == TILE_DUNFREEZE) || (m_TileFIndex == TILE_DUNFREEZE)) && !m_Super && m_DeepFreeze)
	{
		m_DeepFreeze = false;
	}

	// endless hook
	if(((m_TileIndex == TILE_EHOOK_ENABLE) || (m_TileFIndex == TILE_EHOOK_ENABLE)))
	{
		SetEndlessHook(true);
	}
	else if(((m_TileIndex == TILE_EHOOK_DISABLE) || (m_TileFIndex == TILE_EHOOK_DISABLE)))
	{
		SetEndlessHook(false);
	}

	// hit others
	if(((m_TileIndex == TILE_HIT_DISABLE) || (m_TileFIndex == TILE_HIT_DISABLE)) && m_Hit != (DISABLE_HIT_GRENADE | DISABLE_HIT_HAMMER | DISABLE_HIT_LASER | DISABLE_HIT_SHOTGUN))
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't hit others");
		m_Hit = DISABLE_HIT_GRENADE | DISABLE_HIT_HAMMER | DISABLE_HIT_LASER | DISABLE_HIT_SHOTGUN;
		m_Core.m_NoShotgunHit = true;
		m_Core.m_NoGrenadeHit = true;
		m_Core.m_NoHammerHit = true;
		m_Core.m_NoLaserHit = true;
		m_NeededFaketuning |= FAKETUNE_NOHAMMER;
		GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
	}
	else if(((m_TileIndex == TILE_HIT_ENABLE) || (m_TileFIndex == TILE_HIT_ENABLE)) && m_Hit != HIT_ALL)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can hit others");
		m_Hit = HIT_ALL;
		m_Core.m_NoShotgunHit = false;
		m_Core.m_NoGrenadeHit = false;
		m_Core.m_NoHammerHit = false;
		m_Core.m_NoLaserHit = false;
		m_NeededFaketuning &= ~FAKETUNE_NOHAMMER;
		GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
	}

	// collide with others
	if(((m_TileIndex == TILE_NPC_DISABLE) || (m_TileFIndex == TILE_NPC_DISABLE)) && m_Core.m_Collision)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't collide with others");
		m_Core.m_Collision = false;
		m_Core.m_NoCollision = true;
		m_NeededFaketuning |= FAKETUNE_NOCOLL;
		GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
	}
	else if(((m_TileIndex == TILE_NPC_ENABLE) || (m_TileFIndex == TILE_NPC_ENABLE)) && !m_Core.m_Collision)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can collide with others");
		m_Core.m_Collision = true;
		m_Core.m_NoCollision = false;
		m_NeededFaketuning &= ~FAKETUNE_NOCOLL;
		GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
	}

	// hook others
	if(((m_TileIndex == TILE_NPH_DISABLE) || (m_TileFIndex == TILE_NPH_DISABLE)) && m_Core.m_Hook)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't hook others");
		m_Core.m_Hook = false;
		m_Core.m_NoHookHit = true;
		m_NeededFaketuning |= FAKETUNE_NOHOOK;
		GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
	}
	else if(((m_TileIndex == TILE_NPH_ENABLE) || (m_TileFIndex == TILE_NPH_ENABLE)) && !m_Core.m_Hook)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can hook others");
		m_Core.m_Hook = true;
		m_Core.m_NoHookHit = false;
		m_NeededFaketuning &= ~FAKETUNE_NOHOOK;
		GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
	}

	// unlimited air jumps
	if(((m_TileIndex == TILE_UNLIMITED_JUMPS_ENABLE) || (m_TileFIndex == TILE_UNLIMITED_JUMPS_ENABLE)) && !m_SuperJump)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You have unlimited air jumps");
		m_SuperJump = true;
		m_Core.m_EndlessJump = true;
		if(m_Core.m_Jumps == 0)
		{
			m_NeededFaketuning &= ~FAKETUNE_NOJUMP;
			GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
		}
	}
	else if(((m_TileIndex == TILE_UNLIMITED_JUMPS_DISABLE) || (m_TileFIndex == TILE_UNLIMITED_JUMPS_DISABLE)) && m_SuperJump)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You don't have unlimited air jumps");
		m_SuperJump = false;
		m_Core.m_EndlessJump = false;
		if(m_Core.m_Jumps == 0)
		{
			m_NeededFaketuning |= FAKETUNE_NOJUMP;
			GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
		}
	}

	// walljump
	if((m_TileIndex == TILE_WALLJUMP) || (m_TileFIndex == TILE_WALLJUMP))
	{
		if(m_Core.m_Vel.y > 0 && m_Core.m_Colliding && m_Core.m_LeftWall)
		{
			m_Core.m_LeftWall = false;
			m_Core.m_JumpedTotal = m_Core.m_Jumps - 1;
			m_Core.m_Jumped = 1;
		}
	}

	// jetpack gun
	if(((m_TileIndex == TILE_JETPACK_ENABLE) || (m_TileFIndex == TILE_JETPACK_ENABLE)) && !m_Jetpack)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You have a jetpack gun");
		m_Jetpack = true;
		m_Core.m_Jetpack = true;
	}
	else if(((m_TileIndex == TILE_JETPACK_DISABLE) || (m_TileFIndex == TILE_JETPACK_DISABLE)) && m_Jetpack)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You lost your jetpack gun");
		m_Jetpack = false;
		m_Core.m_Jetpack = false;
	}

	// refill jumps
	if(((m_TileIndex == TILE_REFILL_JUMPS) || (m_TileFIndex == TILE_REFILL_JUMPS)) && !m_LastRefillJumps)
	{
		m_Core.m_JumpedTotal = 0;
		m_Core.m_Jumped = 0;
		m_LastRefillJumps = true;
	}
	if((m_TileIndex != TILE_REFILL_JUMPS) && (m_TileFIndex != TILE_REFILL_JUMPS))
	{
		m_LastRefillJumps = false;
	}

	// Teleport gun
	if(((m_TileIndex == TILE_TELE_GUN_ENABLE) || (m_TileFIndex == TILE_TELE_GUN_ENABLE)) && !m_Core.m_HasTelegunGun)
	{
		m_Core.m_HasTelegunGun = true;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Teleport gun enabled");
	}
	else if(((m_TileIndex == TILE_TELE_GUN_DISABLE) || (m_TileFIndex == TILE_TELE_GUN_DISABLE)) && m_Core.m_HasTelegunGun)
	{
		m_Core.m_HasTelegunGun = false;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Teleport gun disabled");
	}

	if(((m_TileIndex == TILE_TELE_GRENADE_ENABLE) || (m_TileFIndex == TILE_TELE_GRENADE_ENABLE)) && !m_Core.m_HasTelegunGrenade)
	{
		m_Core.m_HasTelegunGrenade = true;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Teleport grenade enabled");
	}
	else if(((m_TileIndex == TILE_TELE_GRENADE_DISABLE) || (m_TileFIndex == TILE_TELE_GRENADE_DISABLE)) && m_Core.m_HasTelegunGrenade)
	{
		m_Core.m_HasTelegunGrenade = false;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Teleport grenade disabled");
	}

	if(((m_TileIndex == TILE_TELE_LASER_ENABLE) || (m_TileFIndex == TILE_TELE_LASER_ENABLE)) && !m_Core.m_HasTelegunLaser)
	{
		m_Core.m_HasTelegunLaser = true;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Teleport laser enabled");
	}
	else if(((m_TileIndex == TILE_TELE_LASER_DISABLE) || (m_TileFIndex == TILE_TELE_LASER_DISABLE)) && m_Core.m_HasTelegunLaser)
	{
		m_Core.m_HasTelegunLaser = false;
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "Teleport laser disabled");
	}

	// stopper
	if(m_Core.m_Vel.y > 0 && (m_MoveRestrictions & CANTMOVE_DOWN))
	{
		m_Core.m_Jumped = 0;
		m_Core.m_JumpedTotal = 0;
	}
	m_Core.m_Vel = ClampVel(m_MoveRestrictions, m_Core.m_Vel);

	// handle switch tiles
	if(GameServer()->Collision()->IsSwitch(MapIndex) == TILE_SWITCHOPEN && Team() != TEAM_SUPER && GameServer()->Collision()->GetSwitchNumber(MapIndex) > 0)
	{
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()] = true;
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_EndTick[Team()] = 0;
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Type[Team()] = TILE_SWITCHOPEN;
	}
	else if(GameServer()->Collision()->IsSwitch(MapIndex) == TILE_SWITCHTIMEDOPEN && Team() != TEAM_SUPER && GameServer()->Collision()->GetSwitchNumber(MapIndex) > 0)
	{
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()] = true;
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_EndTick[Team()] = Server()->Tick() + 1 + GameServer()->Collision()->GetSwitchDelay(MapIndex) * Server()->TickSpeed();
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Type[Team()] = TILE_SWITCHTIMEDOPEN;
	}
	else if(GameServer()->Collision()->IsSwitch(MapIndex) == TILE_SWITCHTIMEDCLOSE && Team() != TEAM_SUPER && GameServer()->Collision()->GetSwitchNumber(MapIndex) > 0)
	{
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()] = false;
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_EndTick[Team()] = Server()->Tick() + 1 + GameServer()->Collision()->GetSwitchDelay(MapIndex) * Server()->TickSpeed();
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Type[Team()] = TILE_SWITCHTIMEDCLOSE;
	}
	else if(GameServer()->Collision()->IsSwitch(MapIndex) == TILE_SWITCHCLOSE && Team() != TEAM_SUPER && GameServer()->Collision()->GetSwitchNumber(MapIndex) > 0)
	{
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()] = false;
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_EndTick[Team()] = 0;
		GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Type[Team()] = TILE_SWITCHCLOSE;
	}
	else if(GameServer()->Collision()->IsSwitch(MapIndex) == TILE_FREEZE && Team() != TEAM_SUPER)
	{
		if(GameServer()->Collision()->GetSwitchNumber(MapIndex) == 0 || GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()])
			Freeze(GameServer()->Collision()->GetSwitchDelay(MapIndex));
	}
	else if(GameServer()->Collision()->IsSwitch(MapIndex) == TILE_DFREEZE && Team() != TEAM_SUPER)
	{
		if(GameServer()->Collision()->GetSwitchNumber(MapIndex) == 0 || GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()])
			m_DeepFreeze = true;
	}
	else if(GameServer()->Collision()->IsSwitch(MapIndex) == TILE_DUNFREEZE && Team() != TEAM_SUPER)
	{
		if(GameServer()->Collision()->GetSwitchNumber(MapIndex) == 0 || GameServer()->Collision()->m_pSwitchers[GameServer()->Collision()->GetSwitchNumber(MapIndex)].m_Status[Team()])
			m_DeepFreeze = false;
	}
	else if(GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_ENABLE && m_Hit & DISABLE_HIT_HAMMER && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_HAMMER)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can hammer hit others");
		m_Hit &= ~DISABLE_HIT_HAMMER;
		m_NeededFaketuning &= ~FAKETUNE_NOHAMMER;
		m_Core.m_NoHammerHit = false;
		GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
	}
	else if(GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_DISABLE && !(m_Hit & DISABLE_HIT_HAMMER) && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_HAMMER)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't hammer hit others");
		m_Hit |= DISABLE_HIT_HAMMER;
		m_NeededFaketuning |= FAKETUNE_NOHAMMER;
		m_Core.m_NoHammerHit = true;
		GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
	}
	else if(GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_ENABLE && m_Hit & DISABLE_HIT_SHOTGUN && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_SHOTGUN)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can shoot others with shotgun");
		m_Hit &= ~DISABLE_HIT_SHOTGUN;
		m_Core.m_NoShotgunHit = false;
	}
	else if(GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_DISABLE && !(m_Hit & DISABLE_HIT_SHOTGUN) && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_SHOTGUN)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't shoot others with shotgun");
		m_Hit |= DISABLE_HIT_SHOTGUN;
		m_Core.m_NoShotgunHit = true;
	}
	else if(GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_ENABLE && m_Hit & DISABLE_HIT_GRENADE && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_GRENADE)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can shoot others with grenade");
		m_Hit &= ~DISABLE_HIT_GRENADE;
		m_Core.m_NoGrenadeHit = false;
	}
	else if(GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_DISABLE && !(m_Hit & DISABLE_HIT_GRENADE) && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_GRENADE)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't shoot others with grenade");
		m_Hit |= DISABLE_HIT_GRENADE;
		m_Core.m_NoGrenadeHit = true;
	}
	else if(GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_ENABLE && m_Hit & DISABLE_HIT_LASER && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_LASER)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can shoot others with laser");
		m_Hit &= ~DISABLE_HIT_LASER;
		m_Core.m_NoLaserHit = false;
	}
	else if(GameServer()->Collision()->IsSwitch(MapIndex) == TILE_HIT_DISABLE && !(m_Hit & DISABLE_HIT_LASER) && GameServer()->Collision()->GetSwitchDelay(MapIndex) == WEAPON_LASER)
	{
		GameServer()->SendChatTarget(GetPlayer()->GetCID(), "You can't shoot others with laser");
		m_Hit |= DISABLE_HIT_LASER;
		m_Core.m_NoLaserHit = true;
	}
	else if(GameServer()->Collision()->IsSwitch(MapIndex) == TILE_JUMP)
	{
		int newJumps = GameServer()->Collision()->GetSwitchDelay(MapIndex);

		if(newJumps != m_Core.m_Jumps)
		{
			char aBuf[256];
			if(newJumps == 1)
				str_format(aBuf, sizeof(aBuf), "You can jump %d time", newJumps);
			else
				str_format(aBuf, sizeof(aBuf), "You can jump %d times", newJumps);
			GameServer()->SendChatTarget(GetPlayer()->GetCID(), aBuf);

			if(newJumps == 0 && !m_SuperJump)
			{
				m_NeededFaketuning |= FAKETUNE_NOJUMP;
				GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
			}
			else if(m_Core.m_Jumps == 0)
			{
				m_NeededFaketuning &= ~FAKETUNE_NOJUMP;
				GameServer()->SendTuningParams(m_pPlayer->GetCID(), m_TuneZone); // update tunings
			}

			m_Core.m_Jumps = newJumps;
		}
	}
	else if(GameServer()->Collision()->IsSwitch(MapIndex) == TILE_ADD_TIME && !m_LastPenalty)
	{
		int min = GameServer()->Collision()->GetSwitchDelay(MapIndex);
		int sec = GameServer()->Collision()->GetSwitchNumber(MapIndex);
		int Team = Teams()->m_Core.Team(m_Core.m_Id);

		m_StartTime -= (min * 60 + sec) * Server()->TickSpeed();

		if(Team != TEAM_FLOCK && Team != TEAM_SUPER)
		{
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(Teams()->m_Core.Team(i) == Team && i != m_Core.m_Id && GameServer()->m_apPlayers[i])
				{
					CCharacter *pChar = GameServer()->m_apPlayers[i]->GetCharacter();

					if(pChar)
						pChar->m_StartTime = m_StartTime;
				}
			}
		}

		m_LastPenalty = true;
	}
	else if(GameServer()->Collision()->IsSwitch(MapIndex) == TILE_SUBTRACT_TIME && !m_LastBonus)
	{
		int min = GameServer()->Collision()->GetSwitchDelay(MapIndex);
		int sec = GameServer()->Collision()->GetSwitchNumber(MapIndex);
		int Team = Teams()->m_Core.Team(m_Core.m_Id);

		m_StartTime += (min * 60 + sec) * Server()->TickSpeed();
		if(m_StartTime > Server()->Tick())
			m_StartTime = Server()->Tick();

		if(Team != TEAM_FLOCK && Team != TEAM_SUPER)
		{
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(Teams()->m_Core.Team(i) == Team && i != m_Core.m_Id && GameServer()->m_apPlayers[i])
				{
					CCharacter *pChar = GameServer()->m_apPlayers[i]->GetCharacter();

					if(pChar)
						pChar->m_StartTime = m_StartTime;
				}
			}
		}

		m_LastBonus = true;
	}

	if(GameServer()->Collision()->IsSwitch(MapIndex) != TILE_ADD_TIME)
	{
		m_LastPenalty = false;
	}

	if(GameServer()->Collision()->IsSwitch(MapIndex) != TILE_SUBTRACT_TIME)
	{
		m_LastBonus = false;
	}

	int z = GameServer()->Collision()->IsTeleport(MapIndex);
	if(!g_Config.m_SvOldTeleportHook && !g_Config.m_SvOldTeleportWeapons && z && GameServer()->Collision()->NumTeles(z - 1))
	{
		if(m_Super)
			return;

		m_Core.m_Pos = GameServer()->Collision()->TelePos(z - 1);
		if(!g_Config.m_SvTeleportHoldHook)
		{
			ResetHook();
		}
		if(g_Config.m_SvTeleportLoseWeapons)
			RemoveWeapons();
		return;
	}
	int evilz = GameServer()->Collision()->IsEvilTeleport(MapIndex);
	if(evilz && GameServer()->Collision()->NumTeles(evilz - 1))
	{
		if(m_Super)
			return;

		m_Core.m_Pos = GameServer()->Collision()->TelePos(evilz - 1);
		if(!g_Config.m_SvOldTeleportHook && !g_Config.m_SvOldTeleportWeapons)
		{
			m_Core.m_Vel = vec2(0, 0);

			if(!g_Config.m_SvTeleportHoldHook)
			{
				ResetHook();
				GameWorld()->ReleaseHooked(GetPlayer()->GetCID());
			}
			if(g_Config.m_SvTeleportLoseWeapons)
			{
				RemoveWeapons();
			}
		}
		return;
	}
	if(GameServer()->Collision()->IsCheckEvilTeleport(MapIndex))
	{
		if(m_Super)
			return;
		// first check if there is a TeleCheckOut for the current recorded checkpoint, if not check previous checkpoints
		for(int k = m_TeleCheckpoint - 1; k >= 0; k--)
		{
			if(GameServer()->Collision()->NumCpTeles(k))
			{
				m_Core.m_Pos = GameServer()->Collision()->CpTelePos(k);
				m_Core.m_Vel = vec2(0, 0);

				if(!g_Config.m_SvTeleportHoldHook)
				{
					ResetHook();
					GameWorld()->ReleaseHooked(GetPlayer()->GetCID());
				}

				return;
			}
		}
		// if no checkpointout have been found (or if there no recorded checkpoint), teleport to start
		vec2 SpawnPos;
		if(Controller()->CanSpawn(m_pPlayer->GetTeam(), &SpawnPos))
		{
			m_Core.m_Pos = SpawnPos;
			m_Core.m_Vel = vec2(0, 0);

			if(!g_Config.m_SvTeleportHoldHook)
			{
				ResetHook();
				GameWorld()->ReleaseHooked(GetPlayer()->GetCID());
			}
		}
		return;
	}
	if(GameServer()->Collision()->IsCheckTeleport(MapIndex))
	{
		if(m_Super)
			return;
		// first check if there is a TeleCheckOut for the current recorded checkpoint, if not check previous checkpoints
		for(int k = m_TeleCheckpoint - 1; k >= 0; k--)
		{
			if(GameServer()->Collision()->NumCpTeles(k))
			{
				m_Core.m_Pos = GameServer()->Collision()->CpTelePos(k);

				if(!g_Config.m_SvTeleportHoldHook)
				{
					ResetHook();
				}

				return;
			}
		}
		// if no checkpointout have been found (or if there no recorded checkpoint), teleport to start
		vec2 SpawnPos;
		if(Controller()->CanSpawn(m_pPlayer->GetTeam(), &SpawnPos))
		{
			m_Core.m_Pos = SpawnPos;

			if(!g_Config.m_SvTeleportHoldHook)
			{
				ResetHook();
			}
		}
		return;
	}
}

void CCharacter::HandleTuneLayer()
{
	m_TuneZoneOld = m_TuneZone;
	int CurrentIndex = GameServer()->Collision()->GetMapIndex(m_Pos);
	m_TuneZone = GameServer()->Collision()->IsTune(CurrentIndex);

	if(m_TuneZone)
		m_Core.m_pWorld->m_Tuning = GameServer()->TuningList()[m_TuneZone]; // throw tunings from specific zone into gamecore
	else
		m_Core.m_pWorld->m_Tuning = *GameServer()->Tuning();

	if(m_TuneZone != m_TuneZoneOld) // don't send tunigs all the time
	{
		// send zone msgs
		SendZoneMsgs();
	}
}

void CCharacter::SendZoneMsgs()
{
	// send zone leave msg
	// (m_TuneZoneOld >= 0: avoid zone leave msgs on spawn)
	if(m_TuneZoneOld >= 0 && GameServer()->m_aaZoneLeaveMsg[m_TuneZoneOld])
	{
		const char *pCur = GameServer()->m_aaZoneLeaveMsg[m_TuneZoneOld];
		const char *pPos;
		while((pPos = str_find(pCur, "\\n")))
		{
			char aBuf[256];
			str_copy(aBuf, pCur, pPos - pCur + 1);
			aBuf[pPos - pCur + 1] = '\0';
			pCur = pPos + 2;
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		}
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), pCur);
	}
	// send zone enter msg
	if(GameServer()->m_aaZoneEnterMsg[m_TuneZone])
	{
		const char *pCur = GameServer()->m_aaZoneEnterMsg[m_TuneZone];
		const char *pPos;
		while((pPos = str_find(pCur, "\\n")))
		{
			char aBuf[256];
			str_copy(aBuf, pCur, pPos - pCur + 1);
			aBuf[pPos - pCur + 1] = '\0';
			pCur = pPos + 2;
			GameServer()->SendChatTarget(m_pPlayer->GetCID(), aBuf);
		}
		GameServer()->SendChatTarget(m_pPlayer->GetCID(), pCur);
	}
}

IAntibot *CCharacter::Antibot()
{
	return GameServer()->Antibot();
}

void CCharacter::DDRaceTick()
{
	mem_copy(&m_Input, &m_SavedInput, sizeof(m_Input));
	// m_Armor = (m_FreezeTime >= 0) ? 10 - (m_FreezeTime / 15) : 0;
	if(m_Input.m_Direction != 0 || m_Input.m_Jump != 0)
		m_LastMove = Server()->Tick();

	if(m_FreezeTime > 0)
	{
		if(m_FreezeTime > 0)
			m_FreezeTime--;

		m_Input.m_Direction = 0;
		m_Input.m_Jump = 0;
		m_Input.m_Hook = 0;
		if(m_FreezeTime == 1)
			UnFreeze();
	}
	else if(m_FreezeTime < 0)
		m_FreezeTime = 0;

	if(m_ProtectTick > 0)
	{
		m_ProtectTick--;
	}
	else if(m_ProtectTick < 0)
		m_ProtectTick = 0;

	HandleTuneLayer(); // need this before coretick

	m_Core.m_Id = GetPlayer()->GetCID();
}

void CCharacter::DDRacePostCoreTick()
{
	m_Time = (float)(Server()->Tick() - m_StartTime) / ((float)Server()->TickSpeed());

	if(m_EndlessHook || (m_Super && g_Config.m_SvEndlessSuperHook))
		m_Core.m_HookTick = 0;

	m_FrozenLastTick = false;

	if(m_DeepFreeze && !m_Super)
		Freeze(3);

	if(m_Core.m_Jumps == 0 && !m_Super)
		m_Core.m_Jumped = 3;
	else if(m_Core.m_Jumps == 1 && m_Core.m_Jumped > 0)
		m_Core.m_Jumped = 3;
	else if(m_Core.m_JumpedTotal < m_Core.m_Jumps - 1 && m_Core.m_Jumped > 1)
		m_Core.m_Jumped = 1;

	if((m_Super || m_SuperJump) && m_Core.m_Jumped > 1)
		m_Core.m_Jumped = 1;

	int CurrentIndex = GameServer()->Collision()->GetMapIndex(m_Pos);
	HandleSkippableTiles(CurrentIndex);
	if(!m_Alive)
		return;

	// handle Anti-Skip tiles
	std::list<int> Indices = GameServer()->Collision()->GetMapIndices(m_PrevPos, m_Pos);
	if(!Indices.empty())
	{
		for(int &Index : Indices)
		{
			HandleTiles(Index);
			if(!m_Alive)
				return;
		}
	}
	else
	{
		HandleTiles(CurrentIndex);
		if(!m_Alive)
			return;
	}

	// teleport gun
	if(m_TeleGunTeleport)
	{
		GameWorld()->CreateDeath(m_Pos, m_pPlayer->GetCID());
		m_Core.m_Pos = m_TeleGunPos;
		if(!m_IsBlueTeleGunTeleport)
			m_Core.m_Vel = vec2(0, 0);
		GameWorld()->CreateDeath(m_TeleGunPos, m_pPlayer->GetCID());
		GameWorld()->CreateSound(m_TeleGunPos, SOUND_WEAPON_SPAWN);
		m_TeleGunTeleport = false;
		m_IsBlueTeleGunTeleport = false;
	}

	HandleBroadcast();
}

void CCharacter::Protect(float Seconds, bool CancelOnFire)
{
	if(m_ProtectTick == 0)
		m_ProtectStartTick = Server()->Tick();
	m_ProtectTick = round_to_int(Seconds * Server()->TickSpeed());
	m_ProtectCancelOnFire = CancelOnFire;
}

bool CCharacter::IsProtected()
{
	return m_ProtectTick > 0;
}

bool CCharacter::DeepFreeze()
{
	if(m_DeepFreeze)
		return false;
	m_DeepFreeze = true;
	return true;
}

bool CCharacter::UndeepFreeze()
{
	if(!m_DeepFreeze)
		return false;
	m_DeepFreeze = false;
	return true;
}

void CCharacter::SetAllowFrozenWeaponSwitch(bool Allow)
{
	m_FreezeWeaponSwitch = Allow;
}

bool CCharacter::Freeze(float Seconds, bool BlockHoldFire)
{
	int Ticks = round_to_int(Seconds * Server()->TickSpeed());
	m_FreezeAllowHoldFire = !BlockHoldFire;
	if(Seconds <= 0 || m_Super || m_FreezeTime > Ticks)
		return false;
	if(m_FreezeTime == 0 || m_FreezeTick < Server()->Tick() - Server()->TickSpeed())
	{
		m_FreezeTime = Ticks;
		m_FreezeTick = Server()->Tick();
		return true;
	}
	return false;
}

bool CCharacter::IsFrozen()
{
	return m_DeepFreeze || m_FreezeTime > 0;
}

bool CCharacter::IsDeepFrozen()
{
	return m_DeepFreeze;
}

bool CCharacter::UnFreeze()
{
	if(m_FreezeTime > 0)
	{
		m_FreezeTime = 0;
		m_FreezeTick = 0;
		m_FrozenLastTick = true;
		return true;
	}
	return false;
}

bool CCharacter::ReduceFreeze(float Time)
{
	if(m_FreezeTime > 0)
	{
		m_FreezeTime -= round_to_int(Time * Server()->TickSpeed());
		if(m_FreezeTime <= 0)
		{
			m_FreezeTime = 0;
			m_FreezeTick = 0;
			m_FrozenLastTick = true;
		}
		return true;
	}
	return false;
}

bool CCharacter::RemoveWeapon(int Slot)
{
	bool Removed = false;
	if(m_ActiveWeaponSlot == Slot)
	{
		// switch to an existing weapon
		for(int i = 0; i < NUM_WEAPON_SLOTS; ++i)
			if(m_apWeaponSlots[i])
			{
				SetWeaponSlot(i, true);
				break;
			}
	}

	if(m_apWeaponSlots[Slot])
	{
		delete m_apWeaponSlots[Slot];
		m_apWeaponSlots[Slot] = nullptr;
		Removed = true;
	}

	if(m_ActiveWeaponSlot == Slot)
	{
		// no weapon
		m_ActiveWeaponSlot = WEAPON_GAME;
	}

	return Removed;
}

bool CCharacter::GiveWeapon(int Slot, int Type, int Ammo)
{
	if(Type == WEAPON_ID_NONE)
		return RemoveWeapon(Slot);
#define REGISTER_WEAPON(WEAPTYPE, CLASS) \
	else if(Type == WEAPTYPE) \
	{ \
		bool IsCreatingWeapon = false; \
		if(m_apWeaponSlots[Slot]) \
		{ \
			if(m_apWeaponSlots[Slot]->GetWeaponID() != Type || m_apWeaponSlots[Slot]->GetAmmo() >= Ammo) \
				return false; \
		} \
		else \
		{ \
			m_apWeaponSlots[Slot] = new CLASS(this); \
			m_apWeaponSlots[Slot]->SetTypeID(WEAPTYPE); \
			IsCreatingWeapon = true; \
		} \
		m_apWeaponSlots[Slot]->SetAmmo(Ammo); \
		m_apWeaponSlots[Slot]->OnGiven(!IsCreatingWeapon); \
		if(m_ActiveWeaponSlot < 0 || m_ActiveWeaponSlot >= NUM_WEAPON_SLOTS) \
			SetWeaponSlot(Slot, false); \
	}
#include <game/server/weapons.h>
#undef REGISTER_WEAPON

	return true;
}

void CCharacter::ForceSetWeapon(int Slot, int Type, int Ammo)
{
	if(Type == WEAPON_ID_NONE)
		RemoveWeapon(Slot);
#define REGISTER_WEAPON(WEAPTYPE, CLASS) \
	else if(Type == WEAPTYPE) \
	{ \
		bool IsCreatingWeapon = false; \
		if(m_apWeaponSlots[Slot] && m_apWeaponSlots[Slot]->GetWeaponID() != Type) \
		{ \
			if(m_ActiveWeaponSlot == Slot) \
				SetWeaponSlot(WEAPON_GAME, false); \
			delete m_apWeaponSlots[Slot]; \
			m_apWeaponSlots[Slot] = nullptr; \
		} \
		if(m_apWeaponSlots[Slot] == nullptr) \
		{ \
			m_apWeaponSlots[Slot] = new CLASS(this); \
			m_apWeaponSlots[Slot]->SetTypeID(WEAPTYPE); \
			IsCreatingWeapon = true; \
		} \
		m_apWeaponSlots[Slot]->SetAmmo(Ammo); \
		m_apWeaponSlots[Slot]->OnGiven(!IsCreatingWeapon); \
		if(m_ActiveWeaponSlot < 0 || m_ActiveWeaponSlot >= NUM_WEAPON_SLOTS) \
			SetWeaponSlot(Slot, false); \
	}
#include <game/server/weapons.h>
#undef REGISTER_WEAPON
}

void CCharacter::SetOverrideWeapon(int Slot, int Type, int Ammo)
{
	if(Type == WEAPON_ID_NONE)
	{
		bool IsActive = m_ActiveWeaponSlot == Slot;
		if(IsActive)
			SetWeaponSlot(WEAPON_GAME, false);
		if(m_apOverrideWeaponSlots[Slot])
		{
			delete m_apOverrideWeaponSlots[Slot];
			m_apOverrideWeaponSlots[Slot] = nullptr;
		}
		if(IsActive)
			SetWeaponSlot(Slot, false);
	}
#define REGISTER_WEAPON(WEAPTYPE, CLASS) \
	else if(Type == WEAPTYPE) \
	{ \
		bool IsCreatingWeapon = false; \
		if(m_apOverrideWeaponSlots[Slot] && m_apOverrideWeaponSlots[Slot]->GetWeaponID() != Type) \
		{ \
			if(m_ActiveWeaponSlot == Slot) \
				SetWeaponSlot(WEAPON_GAME, false); \
			delete m_apOverrideWeaponSlots[Slot]; \
			m_apOverrideWeaponSlots[Slot] = nullptr; \
		} \
		if(m_apOverrideWeaponSlots[Slot] == nullptr) \
		{ \
			m_apOverrideWeaponSlots[Slot] = new CLASS(this); \
			m_apOverrideWeaponSlots[Slot]->SetTypeID(WEAPTYPE); \
			IsCreatingWeapon = true; \
		} \
		m_apOverrideWeaponSlots[Slot]->SetAmmo(Ammo); \
		m_apOverrideWeaponSlots[Slot]->OnGiven(!IsCreatingWeapon); \
		if(m_ActiveWeaponSlot < 0 || m_ActiveWeaponSlot >= NUM_WEAPON_SLOTS) \
			SetWeaponSlot(Slot, false); \
	}
#include <game/server/weapons.h>
#undef REGISTER_WEAPON
}

void CCharacter::SetPowerUpWeapon(int Type, int Ammo)
{
	if(Type == WEAPON_ID_NONE)
	{
		if(m_pPowerupWeapon)
		{
			m_pPowerupWeapon->OnUnequip();
			delete m_pPowerupWeapon;
			m_pPowerupWeapon = nullptr;
		}
	}
#define REGISTER_WEAPON(WEAPTYPE, CLASS) \
	else if(Type == WEAPTYPE) \
	{ \
		bool IsCreatingWeapon = false; \
		if(m_pPowerupWeapon && m_pPowerupWeapon->GetWeaponID() != Type) \
		{ \
			delete m_pPowerupWeapon; \
			m_pPowerupWeapon = nullptr; \
		} \
		if(m_pPowerupWeapon == nullptr) \
		{ \
			m_pPowerupWeapon = new CLASS(this); \
			m_pPowerupWeapon->SetTypeID(WEAPTYPE); \
			IsCreatingWeapon = true; \
			m_pPowerupWeapon->OnEquip(); \
		} \
		m_pPowerupWeapon->SetAmmo(Ammo); \
		m_pPowerupWeapon->OnGiven(!IsCreatingWeapon); \
	}
#include <game/server/weapons.h>
#undef REGISTER_WEAPON
}

void CCharacter::RemoveWeapons()
{
	if(m_pPowerupWeapon)
	{
		m_pPowerupWeapon->OnUnequip();
		delete m_pPowerupWeapon;
	}
	m_pPowerupWeapon = nullptr;
	SetWeaponSlot(WEAPON_GAME, false);

	for(int i = 0; i < NUM_WEAPON_SLOTS; i++)
	{
		if(m_apWeaponSlots[i])
			delete m_apWeaponSlots[i];
		m_apWeaponSlots[i] = nullptr;

		if(m_ActiveWeaponSlot == i)
			m_ActiveWeaponSlot = WEAPON_GUN;
	}
}

void CCharacter::SetEndlessHook(bool Enable)
{
	if(m_EndlessHook == Enable)
	{
		return;
	}

	GameServer()->SendChatTarget(GetPlayer()->GetCID(), Enable ? "Endless hook has been activated" : "Endless hook has been deactived");
	m_EndlessHook = Enable;
	m_Core.m_EndlessHook = Enable;
}

// pvp: keeping this for potential mods
void CCharacter::SetDisable(bool Disable)
{
	m_Disabled = Disable;
	if(Disable)
	{
		GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
		GameWorld()->RemoveEntity(this);

		if(m_Core.m_HookedPlayer != -1) // Keeping hook would allow cheats
		{
			ResetHook();
			GameWorld()->ReleaseHooked(GetPlayer()->GetCID());
		}
	}
	else
	{
		m_Core.m_Vel = vec2(0, 0);
		GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;
		GameWorld()->InsertEntity(this);
	}
}

void CCharacter::DDRaceInit()
{
	m_Disabled = false;
	m_DDRaceState = DDRACE_NONE;
	m_PrevPos = m_Pos;
	m_LastBroadcast = 0;
	m_TeamBeforeSuper = 0;
	m_Core.m_Id = GetPlayer()->GetCID();
	m_TeleCheckpoint = 0;
	m_EndlessHook = g_Config.m_SvEndlessDrag;
	m_Hit = g_Config.m_SvHit ? HIT_ALL : DISABLE_HIT_GRENADE | DISABLE_HIT_HAMMER | DISABLE_HIT_LASER | DISABLE_HIT_SHOTGUN;
	m_SuperJump = false;
	m_Jetpack = false;
	m_Core.m_Jumps = 2;

	int Team = Teams()->m_Core.Team(m_Core.m_Id);

	if(Teams()->TeamLocked(Team))
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(Teams()->m_Core.Team(i) == Team && i != m_Core.m_Id && GameServer()->m_apPlayers[i])
			{
				CCharacter *pChar = GameServer()->m_apPlayers[i]->GetCharacter();

				if(pChar)
				{
					m_DDRaceState = pChar->m_DDRaceState;
					m_StartTime = pChar->m_StartTime;
				}
			}
		}
	}
}

void CCharacter::Infection(int ClientID)
{
	GameServer()->m_apPlayers[ClientID]->BDState = (int)ZOMBIE;
}