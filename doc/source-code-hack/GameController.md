# GameController (not finish yet)

> The game controller controls the main game logic. 
> 
> Keeping track of team and player score,
> winning conditions and specific game logic.

# 1. Classes & Interfaces:

* `IGameController`: game controller interface
* `CGameControllerDDRace`: DDRace game controller

For any other mods, create a class which implements the interface `IGameController`.

# X members

## X.Y `m_TeleOuts` and `m_TeleCheckOuts`
from `base/vmath.h`:

* `vector2_base`: `template<typename T> class vector2_base`, a template class indicates 2-dimension vector (T x,T y)
* `vec2`: `typedef vector2_base<float> vec2`

#### X.Y.1 TeleOuts: `std::map<int, std::vector<vec2>> m_TeleOuts`
teleport number -> positions
each teleport has multiple tele-positions (?)

#### X.Y.2 TeleCheckOuts: `std::map<int, std::vector<vec2>> m_TeleCheckOuts`
teleport with checkpoints -> positions

### X.Y.3 
__a. init:__

Check all tiles on tele layer. 
Insert tiles' number and position to the map,
which's type is `TILE_TELEOUT` or `TILE_TELECHECKOUT`.


`src/game/server/gamemodes/DDRace.cpp : InitTeleporter()`

        for(int i = 0; i < Width * Height; i++)
        {
                int Number = GameServer()->Collision()->TeleLayer()[i].m_Number;
                int Type = GameServer()->Collision()->TeleLayer()[i].m_Type;
                if(Number > 0)
                {
                        if(Type == TILE_TELEOUT)
                        {
                                m_TeleOuts[Number - 1].push_back(
                                        vec2(i % Width * 32 + 16, i / Width * 32 + 16));
                        }
                        else if(Type == TILE_TELECHECKOUT)
                        {
                                m_TeleCheckOuts[Number - 1].push_back(
                                        vec2(i % Width * 32 + 16, i / Width * 32 + 16));
                        }
                }
        }

__b. typical usage__
get a random teleposion from a spcific teleport number (for example, to the teleport `TeleTo`)

	// pick up a random position from `TeleTo`'s position list.
	int TeleOut = pSelf->m_World.m_Core.RandomOr0(pGameControllerDDRace->m_TeleOuts[TeleTo - 1].size())

	// get the position's coordinate
	vec2 TelePos = pGameControllerDDRace->m_TeleCheckOuts[TeleTo - 1][TeleOut];

	// then... use it
