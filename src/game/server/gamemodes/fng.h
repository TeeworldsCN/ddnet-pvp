#ifndef GAME_SERVER_GAMEMODES_FNG_H
#define GAME_SERVER_GAMEMODES_FNG_H

#include <game/server/gamecontroller.h>

class CGameControllerSoloFNG : public IGameController
{
private:
	int m_MeltHammerScaleX;
	int m_MeltHammerScaleY;
	int m_HammerScaleX;
	int m_HammerScaleY;

	int m_TeamScoreNormal;
	int m_TeamScoreTeam;
	int m_TeamScoreGold;
	int m_TeamScoreGreen;
	int m_TeamScorePurple;
	int m_TeamScoreFalse;
	int m_PlayerScoreNormal;
	int m_PlayerScoreTeam;
	int m_PlayerScoreGold;
	int m_PlayerScoreGreen;
	int m_PlayerScorePurple;
	int m_PlayerScoreFalse;
	int m_PlayerFreezeScore;
	int m_TeamFreezeScore;

	int m_HammeredBy[MAX_CLIENTS];
	int m_HookedBy[MAX_CLIENTS];
	bool m_IsSaved[MAX_CLIENTS];
	int m_LastHookedBy[MAX_CLIENTS];

protected:
	enum
	{
		TILE_SPIKE_GOLD = 7,
		TILE_SPIKE_NORMAL,
		TILE_SPIKE_TEAM_RED,
		TILE_SPIKE_TEAM_BLUE,
		TILE_SCORE_RED,
		TILE_SCORE_BLUE,

		TILE_SPIKE_GREEN = 14,
		TILE_SPIKE_PURPLE,
	};

public:
	CGameControllerSoloFNG();

	// event
	virtual void OnPreTick() override;
	virtual bool OnCharacterProximateTile(class CCharacter *pChr, int MapIndex) override;
	virtual bool OnCharacterTile(class CCharacter *pChr, int MapIndex) override;
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	virtual void OnCharacterSpawn(class CCharacter *pChr) override;
	virtual int OnCharacterTakeDamage(class CCharacter *pChr, vec2 &Force, int &Dmg, int From, int Weapon, int WeaponType, bool IsExplosion) override;
	virtual bool OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number) override;
	virtual void OnKill(class CPlayer *pPlayer) override;
	virtual bool CanChangeTeam(class CPlayer *pPlayer, int JoinTeam) const override;
	virtual bool CanBeMovedOnBalance(class CPlayer *pPlayer) const override;
	virtual bool IsDisruptiveLeave(class CPlayer *pPlayer) const override;
	virtual float SpawnPosDangerScore(vec2 Pos, int SpawningTeam, class CCharacter *pChar) const;
	virtual bool CanPause(int RequestedTicks) override;
};

class CGameControllerFNG : public CGameControllerSoloFNG
{
private:
public:
	CGameControllerFNG();
};

#endif // GAME_SERVER_GAMEMODES_FNG_H