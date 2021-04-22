/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/teams.h>
#include <game/version.h>

#include <game/generated/protocol.h>

#include "entities/character.h"
#include "entities/pickup.h"
#include "gamecontext.h"
#include "gamecontroller.h"
#include "player.h"

#include "entities/door.h"
#include "entities/dragger.h"
#include "entities/gun.h"
#include "entities/light.h"
#include "entities/plasma.h"
#include "entities/projectile.h"
#include <game/layers.h>

IGameController::IGameController()
{
	m_ResponsibleTeam = -1;

	m_pGameServer = nullptr;
	m_pConfig = nullptr;
	m_pServer = nullptr;
	m_pWorld = nullptr;
	m_pGameType = "unknown";

	m_GameOverTick = -1;
	m_SuddenDeath = 0;
	m_RoundCount = 0;
	m_GameFlags = 0;
	m_RoundStartTick = 0;

	m_UnbalancedTick = -1;
	m_ForceBalanced = false;

	m_aNumSpawnPoints[0] = 0;
	m_aNumSpawnPoints[1] = 0;
	m_aNumSpawnPoints[2] = 0;

	m_CurrentRecord = 0;
}

IGameController::~IGameController()
{
}

void IGameController::DoActivityCheck()
{
	// 	if(g_Config.m_SvInactiveKickTime == 0)
	// 		return;

	// 	for(int i = 0; i < MAX_CLIENTS; ++i)
	// 	{
	// #ifdef CONF_DEBUG
	// 		if(g_Config.m_DbgDummies)
	// 		{
	// 			if(i >= MAX_CLIENTS - g_Config.m_DbgDummies)
	// 				break;
	// 		}
	// #endif
	// 		if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS && Server()->GetAuthedState(i) == AUTHED_NO)
	// 		{
	// 			if(Server()->Tick() > GameServer()->m_apPlayers[i]->m_LastActionTick + g_Config.m_SvInactiveKickTime * Server()->TickSpeed() * 60)
	// 			{
	// 				switch(g_Config.m_SvInactiveKick)
	// 				{
	// 				case 0:
	// 				{
	// 					// move player to spectator
	// 					DoTeamChange(GameServer()->m_apPlayers[i], TEAM_SPECTATORS);
	// 				}
	// 				break;
	// 				case 1:
	// 				{
	// 					// move player to spectator if the reserved slots aren't filled yet, kick him otherwise
	// 					int Spectators = 0;
	// 					for(auto &pPlayer : GameServer()->m_apPlayers)
	// 						if(pPlayer && pPlayer->GetTeam() == TEAM_SPECTATORS)
	// 							++Spectators;
	// 					if(Spectators >= g_Config.m_SvSpectatorSlots)
	// 						Server()->Kick(i, "Kicked for inactivity");
	// 					else
	// 						DoTeamChange(GameServer()->m_apPlayers[i], TEAM_SPECTATORS);
	// 				}
	// 				break;
	// 				case 2:
	// 				{
	// 					// kick the player
	// 					Server()->Kick(i, "Kicked for inactivity");
	// 				}
	// 				}
	// 			}
	// 		}
	// 	}
}

float IGameController::EvaluateSpawnPos(CSpawnEval *pEval, vec2 Pos)
{
	float Score = 0.0f;
	CCharacter *pC = static_cast<CCharacter *>(GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER));
	for(; pC; pC = (CCharacter *)pC->TypeNext())
	{
		// team mates are not as dangerous as enemies
		float Scoremod = 1.0f;
		if(pEval->m_FriendlyTeam != -1 && pC->GetPlayer()->GetTeam() == pEval->m_FriendlyTeam)
			Scoremod = 0.5f;

		float d = distance(Pos, pC->m_Pos);
		Score += Scoremod * (d == 0 ? 1000000000.0f : 1.0f / d);
	}

	return Score;
}

void IGameController::EvaluateSpawnType(CSpawnEval *pEval, int Type)
{
	// get spawn point
	for(int i = 0; i < m_aNumSpawnPoints[Type]; i++)
	{
		// check if the position is occupado
		CCharacter *aEnts[MAX_CLIENTS];
		int Num = GameWorld()->FindEntities(m_aaSpawnPoints[Type][i], 64, (CEntity **)aEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		vec2 Positions[5] = {vec2(0.0f, 0.0f), vec2(-32.0f, 0.0f), vec2(0.0f, -32.0f), vec2(32.0f, 0.0f), vec2(0.0f, 32.0f)}; // start, left, up, right, down
		int Result = -1;
		for(int Index = 0; Index < 5 && Result == -1; ++Index)
		{
			Result = Index;
			if(!GameWorld()->m_Core.m_Tuning[0].m_PlayerCollision)
				break;
			for(int c = 0; c < Num; ++c)
			{
				if(GameServer()->Collision()->CheckPoint(m_aaSpawnPoints[Type][i] + Positions[Index]) ||
					distance(aEnts[c]->m_Pos, m_aaSpawnPoints[Type][i] + Positions[Index]) <= aEnts[c]->GetProximityRadius())
				{
					Result = -1;
					break;
				}
			}
		}
		if(Result == -1)
			continue; // try next spawn point

		vec2 P = m_aaSpawnPoints[Type][i] + Positions[Result];
		float S = EvaluateSpawnPos(pEval, P);
		if(!pEval->m_Got || pEval->m_Score > S)
		{
			pEval->m_Got = true;
			pEval->m_Score = S;
			pEval->m_Pos = P;
		}
	}
}

bool IGameController::CanSpawn(int Team, vec2 *pOutPos)
{
	CSpawnEval Eval;

	// spectators can't spawn
	if(Team == TEAM_SPECTATORS)
		return false;

	EvaluateSpawnType(&Eval, 0);
	EvaluateSpawnType(&Eval, 1);
	EvaluateSpawnType(&Eval, 2);

	*pOutPos = Eval.m_Pos;
	return Eval.m_Got;
}

void IGameController::OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number)
{
	if(Index < 0)
		return;

	int Type = -1;
	int SubType = 0;

	int x, y;
	x = (Pos.x - 16.0f) / 32.0f;
	y = (Pos.y - 16.0f) / 32.0f;
	int sides[8];
	sides[0] = GameServer()->Collision()->Entity(x, y + 1, Layer);
	sides[1] = GameServer()->Collision()->Entity(x + 1, y + 1, Layer);
	sides[2] = GameServer()->Collision()->Entity(x + 1, y, Layer);
	sides[3] = GameServer()->Collision()->Entity(x + 1, y - 1, Layer);
	sides[4] = GameServer()->Collision()->Entity(x, y - 1, Layer);
	sides[5] = GameServer()->Collision()->Entity(x - 1, y - 1, Layer);
	sides[6] = GameServer()->Collision()->Entity(x - 1, y, Layer);
	sides[7] = GameServer()->Collision()->Entity(x - 1, y + 1, Layer);

	if(Index >= ENTITY_SPAWN && Index <= ENTITY_SPAWN_BLUE)
	{
		int Type = Index - ENTITY_SPAWN;
		m_aaSpawnPoints[Type][m_aNumSpawnPoints[Type]] = Pos;
		m_aNumSpawnPoints[Type] = minimum(m_aNumSpawnPoints[Type] + 1, (int)(sizeof(m_aaSpawnPoints[0]) / sizeof(m_aaSpawnPoints[0][0])));
	}

	else if(Index == ENTITY_DOOR)
	{
		for(int i = 0; i < 8; i++)
		{
			if(sides[i] >= ENTITY_LASER_SHORT && sides[i] <= ENTITY_LASER_LONG)
			{
				new CDoor(
					GameWorld(), //GameWorld
					Pos, //Pos
					pi / 4 * i, //Rotation
					32 * 3 + 32 * (sides[i] - ENTITY_LASER_SHORT) * 3, //Length
					Number //Number
				);
			}
		}
	}
	else if(Index == ENTITY_CRAZY_SHOTGUN_EX)
	{
		int Dir;
		if(!Flags)
			Dir = 0;
		else if(Flags == ROTATION_90)
			Dir = 1;
		else if(Flags == ROTATION_180)
			Dir = 2;
		else
			Dir = 3;
		float Deg = Dir * (pi / 2);
		CProjectile *bullet = new CProjectile(
			GameWorld(),
			WEAPON_SHOTGUN, //Type
			-1, //Owner
			Pos, //Pos
			vec2(sin(Deg), cos(Deg)), //Dir
			-2, //Span
			0, //Damage
			true, //Explosive
			0, //Force
			(g_Config.m_SvShotgunBulletSound) ? SOUND_GRENADE_EXPLODE : -1, //SoundImpact
			true, //Freeze
			Layer,
			Number);
		bullet->SetBouncing(2 - (Dir % 2));
	}
	else if(Index == ENTITY_CRAZY_SHOTGUN)
	{
		int Dir;
		if(!Flags)
			Dir = 0;
		else if(Flags == (TILEFLAG_ROTATE))
			Dir = 1;
		else if(Flags == (TILEFLAG_VFLIP | TILEFLAG_HFLIP))
			Dir = 2;
		else
			Dir = 3;
		float Deg = Dir * (pi / 2);
		CProjectile *bullet = new CProjectile(
			GameWorld(),
			WEAPON_SHOTGUN, //Type
			-1, //Owner
			Pos, //Pos
			vec2(sin(Deg), cos(Deg)), //Dir
			-2, //Span
			0, //Damage
			false, //Explosive
			0,
			SOUND_GRENADE_EXPLODE,
			true, //Freeze
			Layer,
			Number);
		bullet->SetBouncing(2 - (Dir % 2));
	}

	if(Index == ENTITY_ARMOR_1)
		Type = POWERUP_ARMOR;
	else if(Index == ENTITY_HEALTH_1)
		Type = POWERUP_HEALTH;
	else if(Index == ENTITY_WEAPON_SHOTGUN)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_SHOTGUN;
	}
	else if(Index == ENTITY_WEAPON_GRENADE)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_GRENADE;
	}
	else if(Index == ENTITY_WEAPON_LASER)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_LASER;
	}
	else if(Index == ENTITY_POWERUP_NINJA)
	{
		Type = POWERUP_NINJA;
		SubType = WEAPON_NINJA;
	}
	else if(Index >= ENTITY_LASER_FAST_CCW && Index <= ENTITY_LASER_FAST_CW)
	{
		int sides2[8];
		sides2[0] = GameServer()->Collision()->Entity(x, y + 2, Layer);
		sides2[1] = GameServer()->Collision()->Entity(x + 2, y + 2, Layer);
		sides2[2] = GameServer()->Collision()->Entity(x + 2, y, Layer);
		sides2[3] = GameServer()->Collision()->Entity(x + 2, y - 2, Layer);
		sides2[4] = GameServer()->Collision()->Entity(x, y - 2, Layer);
		sides2[5] = GameServer()->Collision()->Entity(x - 2, y - 2, Layer);
		sides2[6] = GameServer()->Collision()->Entity(x - 2, y, Layer);
		sides2[7] = GameServer()->Collision()->Entity(x - 2, y + 2, Layer);

		float AngularSpeed = 0.0f;
		int Ind = Index - ENTITY_LASER_STOP;
		int M;
		if(Ind < 0)
		{
			Ind = -Ind;
			M = 1;
		}
		else if(Ind == 0)
			M = 0;
		else
			M = -1;

		if(Ind == 0)
			AngularSpeed = 0.0f;
		else if(Ind == 1)
			AngularSpeed = pi / 360;
		else if(Ind == 2)
			AngularSpeed = pi / 180;
		else if(Ind == 3)
			AngularSpeed = pi / 90;
		AngularSpeed *= M;

		for(int i = 0; i < 8; i++)
		{
			if(sides[i] >= ENTITY_LASER_SHORT && sides[i] <= ENTITY_LASER_LONG)
			{
				CLight *Lgt = new CLight(GameWorld(), Pos, pi / 4 * i, 32 * 3 + 32 * (sides[i] - ENTITY_LASER_SHORT) * 3, Layer, Number);
				Lgt->m_AngularSpeed = AngularSpeed;
				if(sides2[i] >= ENTITY_LASER_C_SLOW && sides2[i] <= ENTITY_LASER_C_FAST)
				{
					Lgt->m_Speed = 1 + (sides2[i] - ENTITY_LASER_C_SLOW) * 2;
					Lgt->m_CurveLength = Lgt->m_Length;
				}
				else if(sides2[i] >= ENTITY_LASER_O_SLOW && sides2[i] <= ENTITY_LASER_O_FAST)
				{
					Lgt->m_Speed = 1 + (sides2[i] - ENTITY_LASER_O_SLOW) * 2;
					Lgt->m_CurveLength = 0;
				}
				else
					Lgt->m_CurveLength = Lgt->m_Length;
			}
		}
	}
	else if(Index >= ENTITY_DRAGGER_WEAK && Index <= ENTITY_DRAGGER_STRONG)
	{
		new CDragger(GameWorld(), Pos, Index - ENTITY_DRAGGER_WEAK + 1, false, Layer, Number);
	}
	else if(Index >= ENTITY_DRAGGER_WEAK_NW && Index <= ENTITY_DRAGGER_STRONG_NW)
	{
		new CDragger(GameWorld(), Pos, Index - ENTITY_DRAGGER_WEAK_NW + 1, true, Layer, Number);
	}
	else if(Index == ENTITY_PLASMAE)
	{
		new CGun(GameWorld(), Pos, false, true, Layer, Number);
	}
	else if(Index == ENTITY_PLASMAF)
	{
		new CGun(GameWorld(), Pos, true, false, Layer, Number);
	}
	else if(Index == ENTITY_PLASMA)
	{
		new CGun(GameWorld(), Pos, true, true, Layer, Number);
	}
	else if(Index == ENTITY_PLASMAU)
	{
		new CGun(GameWorld(), Pos, false, false, Layer, Number);
	}

	if(Type != -1)
	{
		CPickup *pPickup = new CPickup(GameWorld(), Type, SubType);
		pPickup->m_Pos = Pos;
	}
}

void IGameController::OnPlayerConnect(CPlayer *pPlayer)
{
	int ClientID = pPlayer->GetCID();
	pPlayer->Respawn();

	if(!Server()->ClientPrevIngame(ClientID))
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' team=%d", ClientID, Server()->ClientName(ClientID), pPlayer->GetTeam());
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

		str_format(aBuf, sizeof(aBuf), "'%s' entered and joined the %s", Server()->ClientName(ClientID), GetTeamName(pPlayer->GetTeam()));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, -1, CGameContext::CHAT_SIX);

		GameServer()->SendChatTarget(ClientID, "DDraceNetwork Mod. Version: " GAME_VERSION);
		GameServer()->SendChatTarget(ClientID, "please visit DDNet.tw or say /info and make sure to read our /rules");
	}
}

void IGameController::OnPlayerDisconnect(class CPlayer *pPlayer, const char *pReason)
{
	int ClientID = pPlayer->GetCID();
	bool WasModerator = pPlayer->m_Moderating && Server()->ClientIngame(ClientID);
	pPlayer->OnDisconnect();
	if(Server()->ClientIngame(ClientID))
	{
		char aBuf[512];
		if(pReason && *pReason)
			str_format(aBuf, sizeof(aBuf), "'%s' has left the game (%s)", Server()->ClientName(ClientID), pReason);
		else
			str_format(aBuf, sizeof(aBuf), "'%s' has left the game", Server()->ClientName(ClientID));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, -1, CGameContext::CHAT_SIX);

		str_format(aBuf, sizeof(aBuf), "leave player='%d:%s'", ClientID, Server()->ClientName(ClientID));
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);
	}

	if(!GameServer()->PlayerModerating() && WasModerator)
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, "Server kick/spec votes are no longer actively moderated.");

	if(g_Config.m_SvTeam != 3)
		GameServer()->m_pTeams->SetForceCharacterTeam(ClientID, TEAM_FLOCK);
}

void IGameController::EndRound()
{
	if(m_Warmup) // game can't end when we are running warmup
		return;

	GameWorld()->m_Paused = true;
	m_GameOverTick = Server()->Tick();
	m_SuddenDeath = 0;
}

void IGameController::ResetGame()
{
	GameWorld()->m_ResetRequested = true;
}

const char *IGameController::GetTeamName(int Team)
{
	if(Team == 0)
		return "game";
	return "spectators";
}

//static bool IsSeparator(char c) { return c == ';' || c == ' ' || c == ',' || c == '\t'; }

void IGameController::StartRound()
{
	ResetGame();

	m_RoundStartTick = Server()->Tick();
	m_SuddenDeath = 0;
	m_GameOverTick = -1;
	GameWorld()->m_Paused = false;
	m_ForceBalanced = false;
	Server()->DemoRecorder_HandleAutoStart();
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "start round type='%s' teamplay='%d'", m_pGameType, m_GameFlags & GAMEFLAG_TEAMS);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
}

void IGameController::OnReset()
{
	for(auto &pPlayer : GameServer()->m_apPlayers)
		if(pPlayer)
		{
			pPlayer->Respawn();
			pPlayer->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() / 2;
			if(m_RoundCount == 0)
			{
				pPlayer->m_Score = 0;
			}
		}
}

int IGameController::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	if(!pKiller || Weapon == WEAPON_GAME)
		return 0;
	if(pKiller == pVictim->GetPlayer())
		pVictim->GetPlayer()->m_Score--;
	else
		pKiller->m_Score++;

	if(Weapon == WEAPON_SELF)
		pVictim->GetPlayer()->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() * 3.0f;

	return 0;
}

void IGameController::OnCharacterSpawn(class CCharacter *pChr)
{
	// default health
	pChr->IncreaseHealth(10);

	// give default weapons
	pChr->GiveWeapon(WEAPON_HAMMER, -1);
	pChr->GiveWeapon(WEAPON_GUN, 10);

	// TODO: move pTeam calls
	GameServer()->m_pTeams->OnCharacterSpawn(pChr->GetPlayer()->GetCID());
}

void IGameController::HandleCharacterTiles(CCharacter *pChr, int MapIndex)
{
	CPlayer *pPlayer = pChr->GetPlayer();
	int ClientID = pPlayer->GetCID();

	int m_TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
	int m_TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);

	// solo part
	if(((m_TileIndex == TILE_SOLO_ENABLE) || (m_TileFIndex == TILE_SOLO_ENABLE)) && !GameServer()->m_pTeams->m_Core.GetSolo(ClientID))
	{
		GameServer()->SendChatTarget(ClientID, "You are now in a solo part");
		pChr->SetSolo(true);
	}
	else if(((m_TileIndex == TILE_SOLO_DISABLE) || (m_TileFIndex == TILE_SOLO_DISABLE)) && GameServer()->m_pTeams->m_Core.GetSolo(ClientID))
	{
		GameServer()->SendChatTarget(ClientID, "You are now out of the solo part");
		pChr->SetSolo(false);
	}
}

void IGameController::DoWarmup(int Seconds)
{
	if(Seconds < 0)
		m_Warmup = 0;
	else
		m_Warmup = Seconds * Server()->TickSpeed();
}

bool IGameController::IsForceBalanced()
{
	return false;
}

bool IGameController::CanBeMovedOnBalance(int ClientID)
{
	return true;
}

void IGameController::Tick()
{
	// do warmup
	if(m_Warmup)
	{
		m_Warmup--;
		if(!m_Warmup)
			StartRound();
	}

	if(m_GameOverTick != -1)
	{
		// game over.. wait for restart
		if(Server()->Tick() > m_GameOverTick + Server()->TickSpeed() * 10)
		{
			StartRound();
			m_RoundCount++;
		}
	}

	DoActivityCheck();
}

void IGameController::Snap(int SnappingClient)
{
	if(m_ResponsibleTeam < 0)
		return;

	// TODO: Think about what to do for SnappingClient -1 (server demo)
	if(SnappingClient < 0)
		return;

	int DDRTeam = GameServer()->GetPlayerDDRTeam(SnappingClient);
	if(DDRTeam != m_ResponsibleTeam)
		return;

	CNetObj_GameInfo *pGameInfoObj = (CNetObj_GameInfo *)Server()->SnapNewItem(NETOBJTYPE_GAMEINFO, 0, sizeof(CNetObj_GameInfo));
	if(!pGameInfoObj)
		return;

	pGameInfoObj->m_GameFlags = m_GameFlags;
	pGameInfoObj->m_GameStateFlags = 0;
	if(m_GameOverTick != -1)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_GAMEOVER;
	if(m_SuddenDeath)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_SUDDENDEATH;
	if(GameWorld()->m_Paused)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_PAUSED;
	pGameInfoObj->m_RoundStartTick = m_RoundStartTick;
	pGameInfoObj->m_WarmupTimer = m_Warmup;

	pGameInfoObj->m_RoundNum = 0;
	pGameInfoObj->m_RoundCurrent = m_RoundCount + 1;

	CCharacter *pChr;
	CPlayer *pPlayer = SnappingClient > -1 ? GameServer()->m_apPlayers[SnappingClient] : 0;
	CPlayer *pPlayer2;

	if(pPlayer && (pPlayer->m_TimerType == CPlayer::TIMERTYPE_GAMETIMER || pPlayer->m_TimerType == CPlayer::TIMERTYPE_GAMETIMER_AND_BROADCAST) && pPlayer->GetClientVersion() >= VERSION_DDNET_GAMETICK)
	{
		if((pPlayer->GetTeam() == -1 || pPlayer->IsPaused()) && pPlayer->m_SpectatorID != SPEC_FREEVIEW && (pPlayer2 = GameServer()->m_apPlayers[pPlayer->m_SpectatorID]))
		{
			if((pChr = pPlayer2->GetCharacter()) && pChr->m_DDRaceState == DDRACE_STARTED)
			{
				pGameInfoObj->m_WarmupTimer = -pChr->m_StartTime;
				pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_RACETIME;
			}
		}
		else if((pChr = pPlayer->GetCharacter()) && pChr->m_DDRaceState == DDRACE_STARTED)
		{
			pGameInfoObj->m_WarmupTimer = -pChr->m_StartTime;
			pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_RACETIME;
		}
	}

	CNetObj_GameInfoEx *pGameInfoEx = (CNetObj_GameInfoEx *)Server()->SnapNewItem(NETOBJTYPE_GAMEINFOEX, 0, sizeof(CNetObj_GameInfoEx));
	if(!pGameInfoEx)
		return;

	pGameInfoEx->m_Flags =
		GAMEINFOFLAG_ALLOW_EYE_WHEEL |
		GAMEINFOFLAG_ALLOW_HOOK_COLL |
		GAMEINFOFLAG_ENTITIES_DDNET |
		GAMEINFOFLAG_ENTITIES_DDRACE |
		GAMEINFOFLAG_ENTITIES_RACE;
	pGameInfoEx->m_Flags2 = 0;
	pGameInfoEx->m_Version = GAMEINFO_CURVERSION;

	if(Server()->IsSixup(SnappingClient))
	{
		protocol7::CNetObj_GameData *pGameData = static_cast<protocol7::CNetObj_GameData *>(Server()->SnapNewItem(-protocol7::NETOBJTYPE_GAMEDATA, 0, sizeof(protocol7::CNetObj_GameData)));
		if(!pGameData)
			return;

		pGameData->m_GameStartTick = m_RoundStartTick;
		pGameData->m_GameStateFlags = 0;
		if(m_GameOverTick != -1)
			pGameData->m_GameStateFlags |= protocol7::GAMESTATEFLAG_GAMEOVER;
		if(m_SuddenDeath)
			pGameData->m_GameStateFlags |= protocol7::GAMESTATEFLAG_SUDDENDEATH;
		if(GameWorld()->m_Paused)
			pGameData->m_GameStateFlags |= protocol7::GAMESTATEFLAG_PAUSED;

		pGameData->m_GameStateEndTick = 0;

		protocol7::CNetObj_GameDataRace *pRaceData = static_cast<protocol7::CNetObj_GameDataRace *>(Server()->SnapNewItem(-protocol7::NETOBJTYPE_GAMEDATARACE, 0, sizeof(protocol7::CNetObj_GameDataRace)));
		if(!pRaceData)
			return;

		pRaceData->m_BestTime = round_to_int(m_CurrentRecord * 1000);
		pRaceData->m_Precision = 0;
		pRaceData->m_RaceFlags = protocol7::RACEFLAG_HIDE_KILLMSG | protocol7::RACEFLAG_KEEP_WANTED_WEAPON;
	}
}

int IGameController::GetAutoTeam(int NotThisID)
{
	// this will force the auto balancer to work overtime as well
#ifdef CONF_DEBUG
	if(g_Config.m_DbgStress)
		return 0;
#endif

	int aNumplayers[2] = {0, 0};
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i] && i != NotThisID)
		{
			if(GameServer()->m_apPlayers[i]->GetTeam() >= TEAM_RED && GameServer()->m_apPlayers[i]->GetTeam() <= TEAM_BLUE)
				aNumplayers[GameServer()->m_apPlayers[i]->GetTeam()]++;
		}
	}

	int Team = 0;

	if(CanJoinTeam(Team, NotThisID))
		return Team;
	return -1;
}

bool IGameController::CanJoinTeam(int Team, int NotThisID)
{
	if(Team == TEAM_SPECTATORS || (GameServer()->m_apPlayers[NotThisID] && GameServer()->m_apPlayers[NotThisID]->GetTeam() != TEAM_SPECTATORS))
		return true;

	int aNumplayers[2] = {0, 0};
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i] && i != NotThisID)
		{
			if(GameServer()->m_apPlayers[i]->GetTeam() >= TEAM_RED && GameServer()->m_apPlayers[i]->GetTeam() <= TEAM_BLUE)
				aNumplayers[GameServer()->m_apPlayers[i]->GetTeam()]++;
		}
	}

	return (aNumplayers[0] + aNumplayers[1]) < Server()->MaxClients() - g_Config.m_SvSpectatorSlots;
}

int IGameController::ClampTeam(int Team)
{
	if(Team < 0)
		return TEAM_SPECTATORS;
	return 0;
}

// TODO: Controller, don't need this anymore
int64 IGameController::GetMaskForPlayerWorldEvent(int Asker, int ExceptID)
{
	return CmaskAllExceptOne(ExceptID);
}

int IGameController::GetPlayerTeam(int ClientID) const
{
	return GameServer()->GetPlayerDDRTeam(ClientID);
}

void IGameController::DoTeamChange(CPlayer *pPlayer, int Team, bool DoChatMsg)
{
	Team = ClampTeam(Team);
	if(Team == pPlayer->GetTeam())
		return;

	pPlayer->SetTeam(Team);
	int ClientID = pPlayer->GetCID();

	char aBuf[128];
	DoChatMsg = false;
	if(DoChatMsg)
	{
		str_format(aBuf, sizeof(aBuf), "'%s' joined the %s", Server()->ClientName(ClientID), GetTeamName(Team));
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}

	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' m_Team=%d", ClientID, Server()->ClientName(ClientID), Team);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	// OnPlayerInfoChange(pPlayer);
}

void IGameController::InitController(int Team, class CGameContext *pGameServer, class CGameWorld *pWorld)
{
	m_ResponsibleTeam = Team;
	m_pGameServer = pGameServer;
	m_pConfig = m_pGameServer->Config();
	m_pServer = m_pGameServer->Server();
	m_pWorld = pWorld;
	DoWarmup(g_Config.m_SvWarmup);
	m_RoundStartTick = Server()->Tick();
}