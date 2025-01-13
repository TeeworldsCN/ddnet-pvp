#include "fng.h"

#include <game/server/entities/character.h>
#include <game/server/entities/textentity.h>
#include <game/server/player.h>
#include <game/server/weapons.h>

CGameControllerSoloFNG::CGameControllerSoloFNG()
{
	m_pGameType = "solofng";
	m_DDNetInfoFlag |= GAMEINFOFLAG_PREDICT_FNG | GAMEINFOFLAG_ENTITIES_FNG;

	INSTANCE_CONFIG_INT(&m_HammerScaleX, "hammer_scale_x", 320, 0, 1000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "linearly scale up hammer x power, percentage, for hammering enemies and unfrozen teammates")
	INSTANCE_CONFIG_INT(&m_HammerScaleY, "hammer_scale_y", 120, 0, 1000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "linearly scale up hammer y power, percentage, for hammering enemies and unfrozen teammates")
	INSTANCE_CONFIG_INT(&m_MeltHammerScaleX, "melt_hammer_scale_x", 50, 0, 1000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "linearly scale up hammer x power, percentage, for hammering frozen teammates")
	INSTANCE_CONFIG_INT(&m_MeltHammerScaleY, "melt_hammer_scale_y", 50, 0, 1000, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "linearly scale up hammer y power, percentage, for hammering frozen teammates")

	INSTANCE_CONFIG_INT(&m_TeamScoreNormal, "team_score_normal", 5, 0, 100, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Points a team receives for grabbing into normal spikes")
	INSTANCE_CONFIG_INT(&m_TeamScoreTeam, "team_score_team", 10, 0, 100, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Points a team receives for grabbing into team spikes")
	INSTANCE_CONFIG_INT(&m_TeamScoreGold, "team_score_gold", 15, 0, 100, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Points a team receives for grabbing into golden spikes")
	INSTANCE_CONFIG_INT(&m_TeamScoreGreen, "team_score_green", 15, 0, 100, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Points a team receives for grabbing into green spikes")
	INSTANCE_CONFIG_INT(&m_TeamScorePurple, "team_score_purple", 15, 0, 100, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Points a team receives for grabbing into purple spikes")
	INSTANCE_CONFIG_INT(&m_TeamScoreFalse, "team_score_false", -2, -100, 0, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Points a team receives for grabbing into opponents spikes")

	INSTANCE_CONFIG_INT(&m_PlayerScoreNormal, "player_score_normal", 3, 0, 100, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Points a player receives for grabbing into normal spikes")
	INSTANCE_CONFIG_INT(&m_PlayerScoreTeam, "player_score_team", 5, 0, 100, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Points a player receives for grabbing into team spikes")
	INSTANCE_CONFIG_INT(&m_PlayerScoreGold, "player_score_gold", 7, 0, 100, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Points a player receives for grabbing into golden spikes")
	INSTANCE_CONFIG_INT(&m_PlayerScoreGreen, "player_score_green", 8, 0, 100, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Points a player receives for grabbing into green spikes")
	INSTANCE_CONFIG_INT(&m_PlayerScorePurple, "player_score_purple", 9, 0, 100, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Points a player receives for grabbing into purple spikes")
	INSTANCE_CONFIG_INT(&m_PlayerScoreFalse, "player_score_false", -5, -100, 0, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Points a player receives for grabbing into opponents spikes")

	INSTANCE_CONFIG_INT(&m_PlayerFreezeScore, "player_freeze_score", 1, 0, 100, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Points a player receives for freezing an opponent")
	INSTANCE_CONFIG_INT(&m_TeamFreezeScore, "team_freeze_score", 1, 0, 100, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Points a team receives for freezing an opponent")

	for(int i = 0; i < MAX_CLIENTS; i++)
		m_HookedBy[i] = -1;
}

CGameControllerFNG::CGameControllerFNG()
{
	m_pGameType = "fng";
	m_GameFlags = IGF_TEAMS | IGF_SUDDENDEATH;
}

void CGameControllerSoloFNG::OnPreTick()
{
	// first pass, check unhook
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GetPlayerIfInRoom(i))
		{
			if(m_HookedBy[i] >= 0)
			{
				CPlayer *pHookingPlayer = GetPlayerIfInRoom(m_HookedBy[i]);
				if(!pHookingPlayer || !pHookingPlayer->GetCharacter() || pHookingPlayer->GetCharacter()->Core()->m_HookedPlayer != i)
				{
					m_LastHookedBy[i] = m_HookedBy[i];
					m_HookedBy[i] = -1;
				}
			}
			m_IsSaved[i] = false;
		}
	}

	// second pass, check new hooks & grounded
	CCharacter *pChr = (CCharacter *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER);
	while(pChr)
	{
		int ChrClientID = pChr->GetPlayer()->GetCID();
		int HookedClientID = pChr->Core()->m_HookedPlayer;
		if(HookedClientID >= 0 && m_HookedBy[HookedClientID] == -1)
			m_HookedBy[HookedClientID] = ChrClientID;

		if(IsFriendlyFire(HookedClientID, ChrClientID))
			m_IsSaved[HookedClientID] = true;

		if(m_HammeredBy[ChrClientID] >= 0 && pChr->IsGrounded())
			m_HammeredBy[ChrClientID] = -1;

		pChr = (CCharacter *)pChr->TypeNext();
	}

	// third pass, clear unfrozen char states
	pChr = (CCharacter *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER);
	while(pChr)
	{
		int ChrClientID = pChr->GetPlayer()->GetCID();
		if(!pChr->IsFrozen())
		{
			m_IsSaved[ChrClientID] = false;
			m_HookedBy[ChrClientID] = -1;
			m_LastHookedBy[ChrClientID] = -1;
			m_HammeredBy[ChrClientID] = -1;
			pChr->SetArmor(0);
		}
		else
		{
			int Seconds = ceil(pChr->m_FreezeTime / (float)Server()->TickSpeed());
			pChr->SetArmor(Seconds);
			if((pChr->m_FreezeTime + 1) % Server()->TickSpeed() == 0)
				GameWorld()->CreateDamageIndCircle(pChr->GetPos(), false, pi, Seconds, 10, 0.8f, (IsTeamplay() ? CmaskTeam(pChr->GetPlayer()->GetTeam()) : CmaskOne(pChr->GetPlayer()->GetCID())) | CmaskTeam(TEAM_SPECTATORS));
		}

		pChr = (CCharacter *)pChr->TypeNext();
	}
}

void CGameControllerSoloFNG::OnCharacterSpawn(CCharacter *pChr)
{
	pChr->IncreaseHealth(10);
	pChr->GiveWeapon(WEAPON_LASER, WEAPON_ID_LASER, -1);
	pChr->GiveWeapon(WEAPON_HAMMER, WEAPON_ID_HAMMER, -1);
	pChr->SetAllowFrozenWeaponSwitch(true);
	pChr->SetWeaponTimerType(WEAPON_TIMER_INDIVIDUAL);
	pChr->Protect(0.5f);

	m_HammeredBy[pChr->GetPlayer()->GetCID()] = -1;
	m_HookedBy[pChr->GetPlayer()->GetCID()] = -1;
	m_LastHookedBy[pChr->GetPlayer()->GetCID()] = -1;
	m_IsSaved[pChr->GetPlayer()->GetCID()] = false;
}

int CGameControllerSoloFNG::OnCharacterTakeDamage(CCharacter *pChr, vec2 &Force, int &Dmg, int From, int Weapon, int WeaponType, bool IsExplosion)
{
	int SelfCID = pChr->GetPlayer()->GetCID();
	if(Weapon == WEAPON_HAMMER)
	{
		if(IsTeamplay() && IsFriendlyFire(SelfCID, From) && pChr->IsFrozen())
		{
			Force.x *= m_MeltHammerScaleX * 0.01f;
			Force.y *= m_MeltHammerScaleY * 0.01f;

			pChr->ReduceFreeze(3);
			if(!pChr->IsFrozen())
			{
				SendKillMsg(From, SelfCID, WEAPON_HAMMER);
				GameWorld()->CreateSound(pChr->GetPos(), SOUND_CTF_RETURN);
			}
		}
		else
		{
			Force.x *= m_HammerScaleX * 0.01f;
			Force.y *= m_HammerScaleY * 0.01f;

			if(pChr->IsFrozen())
				m_HammeredBy[SelfCID] = From;
		}

		return DAMAGE_NO_DAMAGE | DAMAGE_NO_DEATH | DAMAGE_NO_HITSOUND | DAMAGE_NO_PAINSOUND | DAMAGE_NO_INDICATOR;
	}
	else if(Dmg > 0 && !pChr->IsFrozen() && Weapon != WEAPON_WORLD && Weapon != WEAPON_GAME && From >= 0 && From != SelfCID && (!IsExplosion || Dmg >= 4))
	{
		CPlayer *pAttacker = GetPlayerIfInRoom(From);
		if(pAttacker && pAttacker->GetCharacter())
			pAttacker->GetCharacter()->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());

		pChr->Freeze(10, true);
		m_LastHookedBy[SelfCID] = From;
		SendKillMsg(From, SelfCID, Weapon);
		GameWorld()->CreateDeath(pChr->GetPos(), pChr->GetPlayer()->GetCID());
		pAttacker->m_Score += m_PlayerFreezeScore;
		if(IsTeamplay())
		{
			if(pAttacker->GetTeam() == TEAM_RED)
				m_aTeamscore[TEAM_RED] += m_TeamFreezeScore;
			else if(pAttacker->GetTeam() == TEAM_BLUE)
				m_aTeamscore[TEAM_BLUE] += m_TeamFreezeScore;
		}
		// short pain noise
		Dmg = 1;
		return DAMAGE_NO_DAMAGE | DAMAGE_NO_DEATH | DAMAGE_NO_INDICATOR;
	}

	return DAMAGE_NO_DAMAGE | DAMAGE_NO_DEATH | DAMAGE_NO_HITSOUND | DAMAGE_NO_INDICATOR | DAMAGE_NO_PAINSOUND;
}

bool CGameControllerSoloFNG::OnCharacterProximateTile(CCharacter *pChr, int MapIndex)
{
	int TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
	int TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);

	int Type = -1;
	int PlayerScore = 0;
	int TeamScore = 0;

	if(TileIndex == TILE_SPIKE_GOLD || TileFIndex == TILE_SPIKE_GOLD)
	{
		Type = TILE_SPIKE_GOLD;
		PlayerScore = m_PlayerScoreGold;
		TeamScore = m_TeamScoreGold;
	}

	if(TileIndex == TILE_SPIKE_GREEN || TileFIndex == TILE_SPIKE_GREEN)
	{
		Type = TILE_SPIKE_GREEN;
		PlayerScore = m_PlayerScoreGreen;
		TeamScore = m_TeamScoreGreen;
	}

	if(TileIndex == TILE_SPIKE_PURPLE || TileFIndex == TILE_SPIKE_PURPLE)
	{
		Type = TILE_SPIKE_PURPLE;
		PlayerScore = m_PlayerScorePurple;
		TeamScore = m_TeamScorePurple;
	}

	if(TileIndex == TILE_SPIKE_NORMAL || TileFIndex == TILE_SPIKE_NORMAL)
	{
		Type = TILE_SPIKE_NORMAL;
		PlayerScore = m_PlayerScoreNormal;
		TeamScore = m_TeamScoreNormal;
	}

	if(TileIndex == TILE_SPIKE_TEAM_BLUE || TileFIndex == TILE_SPIKE_TEAM_BLUE)
	{
		Type = TILE_SPIKE_TEAM_BLUE;
		PlayerScore = m_PlayerScoreTeam;
		TeamScore = m_TeamScoreTeam;
	}

	if(TileIndex == TILE_SPIKE_TEAM_RED || TileFIndex == TILE_SPIKE_TEAM_RED)
	{
		Type = TILE_SPIKE_TEAM_RED;
		PlayerScore = m_PlayerScoreTeam;
		TeamScore = m_TeamScoreTeam;
	}

	if(Type >= 0)
	{
		int Victim = pChr->GetPlayer()->GetCID();
		int Attacker = -1;
		if(m_IsSaved[Victim] || !pChr->IsFrozen())
			Attacker = Victim;
		else if(m_HammeredBy[Victim] >= 0)
			Attacker = m_HammeredBy[Victim];
		else if(m_HookedBy[Victim] >= 0)
			Attacker = m_HookedBy[Victim];
		else
			Attacker = m_LastHookedBy[Victim];

		// for safety
		if(IsFriendlyFire(Attacker, Victim))
			Attacker = Victim;

		bool IsKill = Attacker != Victim && Attacker >= 0;

		pChr->Die(Attacker, IsKill ? (int)WEAPON_NINJA : (int)WEAPON_SELF);

		if(Attacker == Victim)
			return true;

		char aBuf[8];
		CPlayer *pAttacker = GetPlayerIfInRoom(Attacker);
		vec2 TextOffsetGrounded = vec2(+20, -70);
		vec2 TextOffsetAir = vec2(+20, -40);
		vec2 TextOffset = vec2(+20, 0);
		if(pAttacker)
		{
			if(Type == TILE_SPIKE_TEAM_RED || Type == TILE_SPIKE_TEAM_BLUE)
			{
				if(IsTeamplay() && !IsFriendlyTeamFire(Type - (TILE_SPIKE_TEAM_RED - TEAM_RED), pAttacker->GetTeam()))
				{
					// false kill
					if(pAttacker->GetCharacter())
					{
						pAttacker->GetCharacter()->SetEmote(EMOTE_PAIN, Server()->Tick() + Server()->TickSpeed());
						pAttacker->GetCharacter()->Freeze(5, true);
						str_format(aBuf, sizeof(aBuf), "%+d", m_PlayerScoreFalse);
						if(pAttacker->GetCharacter()->IsGrounded())
							TextOffset = TextOffsetGrounded;
						else
							TextOffset = TextOffsetAir;
						new CTextEntity(GameWorld(), pAttacker->GetCharacter()->GetPos() + TextOffset, CTextEntity::TYPE_LASER, CTextEntity::SIZE_NORMAL, CTextEntity::ALIGN_MIDDLE, aBuf, 2.0f);
					}
					pAttacker->m_Score += m_PlayerScoreFalse;
					if(pAttacker->GetTeam() == TEAM_RED)
						m_aTeamscore[TEAM_RED] += m_TeamScoreFalse;
					else if(pAttacker->GetTeam() == TEAM_BLUE)
						m_aTeamscore[TEAM_BLUE] += m_TeamScoreFalse;

					GameWorld()->CreateSoundGlobal(SOUND_CTF_DROP, CmaskOneAndViewer(Attacker));
					GameWorld()->CreateSoundGlobal(SOUND_TEE_CRY, CmaskOneAndViewer(Attacker));
					GameWorld()->CreateSoundGlobal(SOUND_CTF_DROP, CmaskOneAndViewer(Victim));
					return true;
				}
			}

			// normal kill

			if(pAttacker->GetCharacter())
			{
				pAttacker->GetCharacter()->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());
				str_format(aBuf, sizeof(aBuf), "%+d", PlayerScore);
				if(pAttacker->GetCharacter()->IsGrounded())
					TextOffset = TextOffsetGrounded;
				else
					TextOffset = TextOffsetAir;
				new CTextEntity(GameWorld(), pAttacker->GetCharacter()->GetPos() + TextOffset, CTextEntity::TYPE_LASER, CTextEntity::SIZE_NORMAL, CTextEntity::ALIGN_MIDDLE, aBuf, 2.0f);
			}

			pAttacker->m_Score += PlayerScore;
			if(pAttacker->GetTeam() == TEAM_RED)
				m_aTeamscore[TEAM_RED] += TeamScore;
			else if(pAttacker->GetTeam() == TEAM_BLUE)
				m_aTeamscore[TEAM_BLUE] += TeamScore;

			if(PlayerScore >= m_PlayerScoreGold)
				GameWorld()->CreateSoundGlobal(SOUND_CTF_CAPTURE, CmaskOneAndViewer(Attacker));
			else
				GameWorld()->CreateSoundGlobal(SOUND_CTF_GRAB_PL, CmaskOneAndViewer(Attacker));

			GameWorld()->CreateSoundGlobal(SOUND_CTF_GRAB_EN, CmaskOneAndViewer(Victim));
		}

		return true;
	}

	// keep speedup tiles
	return false;
}

bool CGameControllerSoloFNG::OnCharacterTile(CCharacter *pChr, int MapIndex)
{
	// ignore all ddr tiles
	return true;
}

bool CGameControllerSoloFNG::OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number)
{
	// bypass pickups
	if(Index >= ENTITY_ARMOR_1 && Index <= ENTITY_WEAPON_LASER)
		return true;
	return false;
}

int CGameControllerSoloFNG::OnCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, int Weapon)
{
	return DEATH_SKIP_SCORE | DEATH_NO_SUICIDE_PANATY;
}

void CGameControllerSoloFNG::OnKill(CPlayer *pPlayer)
{
	if(pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsFrozen())
		return;
	IGameController::OnKill(pPlayer);
}

bool CGameControllerSoloFNG::CanChangeTeam(CPlayer *pPlayer, int JoinTeam) const
{
	if(pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsFrozen())
		return false;
	return IGameController::CanChangeTeam(pPlayer, JoinTeam);
}

bool CGameControllerSoloFNG::CanBeMovedOnBalance(CPlayer *pPlayer) const
{
	if(pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsFrozen())
		return false;
	return true;
}

bool CGameControllerSoloFNG::IsDisruptiveLeave(CPlayer *pPlayer) const
{
	if(pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsFrozen())
		return true;
	return false;
}

float CGameControllerSoloFNG::SpawnPosDangerScore(vec2 Pos, int SpawningTeam, CCharacter *pChar) const
{
	if(IsTeamplay() && pChar->GetPlayer()->GetTeam() == SpawningTeam)
		return 0.5f;
	else if(!pChar->IsFrozen())
		return 1.0f;
	else
		return mix(0.5f, 1.0f, 1.0f - (pChar->m_FreezeTime / (float)(Server()->TickSpeed() * 10)));
}

bool CGameControllerSoloFNG::CanPause(int RequestedTicks)
{
	CCharacter *pChr = (CCharacter *)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER);
	while(pChr)
	{
		if(RequestedTicks < Server()->TickSpeed() * 5)
		{
			// See if all players are grounded in the first 5 seconds
			if(!pChr->IsGrounded())
				return false;
		}
		else
		{
			// See if all players are not hooking other players after 5 seconds
			if(pChr->Core()->m_HookedPlayer >= 0)
				return false;
		}

		pChr = (CCharacter *)pChr->TypeNext();
	}

	return true;
}