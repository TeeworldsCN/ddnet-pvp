#ifndef GAME_SERVER_GAMEMODES_CATCH_H
#define GAME_SERVER_GAMEMODES_CATCH_H

#include "instagib.h"
#include <game/server/gamecontroller.h>

class CGameControllerCatch : public IGameController
{
private:
	// config
	int m_WinnerBonus;
	int m_MinimumPlayers;

	// states
	int m_aCaughtBy[MAX_CLIENTS];
	int m_aNumCaught[MAX_CLIENTS];
	class CDumbEntity *m_apHearts[MAX_CLIENTS];
	int m_aHeartID[MAX_CLIENTS];
	int m_aHeartKillTick[MAX_CLIENTS];

	struct Path
	{
		vec2 m_aPathPoints[MAX_CLIENTS];
		int m_PathIndex;

		int Prev(int Num) { return (m_PathIndex + MAX_CLIENTS - Num) % MAX_CLIENTS; }
		int Next(int Num) { return (m_PathIndex + Num) % MAX_CLIENTS; }
		int Index() { return Prev(1); }
		vec2 LatestPoint() { return m_aPathPoints[Index()]; }
		vec2 PrevPoint(int Num) { return m_aPathPoints[Prev(Num + 1)]; }

		Path() { m_PathIndex = 0; }

		void Init(vec2 Point)
		{
			for(auto &P : m_aPathPoints)
				P = Point;
		}

		void RecordPoint(vec2 Point)
		{
			m_aPathPoints[m_PathIndex] = Point;
			m_PathIndex = Next(1);
		}
	};

	// path
	Path m_aCharPath[MAX_CLIENTS];
	vec2 m_aLastPosition[MAX_CLIENTS];
	float m_aCharInertia[MAX_CLIENTS];
	float m_aCharMoveDist[MAX_CLIENTS];

public:
	CGameControllerCatch();

	void Catch(class CPlayer *pVictim);
	void Catch(class CPlayer *pVictim, class CPlayer *pBy, vec2 Pos);
	void Release(class CPlayer *pPlayer, bool IsKillRelease);

	// event
	virtual void OnInit() override;
	virtual void OnPreTick() override;
	virtual void OnWorldReset() override;
	virtual void OnKill(class CPlayer *pPlayer) override;
	virtual void OnPlayerJoin(class CPlayer *pPlayer) override;
	virtual void OnPlayerLeave(class CPlayer *pPlayer) override;
	virtual bool OnPlayerTryRespawn(class CPlayer *pPlayer, vec2 Pos) override;
	virtual void OnPlayerChangeTeam(class CPlayer *pPlayer, int FromTeam, int ToTeam) override;
	virtual void OnCharacterSpawn(class CCharacter *pChr) override;
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	virtual bool CanDeadPlayerFollow(const class CPlayer *pSpectator, const class CPlayer *Target) override;
	virtual void DoWincheckMatch() override;
};

typedef CGameControllerInstagib<CGameControllerCatch> CGameControllerZCatch;

#endif // GAME_SERVER_GAMEMODES_CATCH_H
