#ifndef GAME_SERVER_GAMEMODES_H
#define GAME_SERVER_GAMEMODES_H

#include "gamemodes/ctf.h"
#include "gamemodes/dm.h"
#include "gamemodes/lms.h"
#include "gamemodes/lts.h"
#include "gamemodes/tdm.h"

#include "gamemodes/catch.h"
#include "gamemodes/instagib.h"
#include "gamemodes/block-down.h"

#endif

#ifdef REGISTER_GAME_TYPE
REGISTER_GAME_TYPE(dm, CGameControllerDM)
REGISTER_GAME_TYPE(tdm, CGameControllerTDM)
REGISTER_GAME_TYPE(ctf, CGameControllerCTF)
REGISTER_GAME_TYPE(lms, CGameControllerLMS)
REGISTER_GAME_TYPE(lts, CGameControllerLTS)
REGISTER_GAME_TYPE(idm, CGameControllerIDM)
REGISTER_GAME_TYPE(itdm, CGameControllerITDM)
REGISTER_GAME_TYPE(ictf, CGameControllerICTF)
REGISTER_GAME_TYPE(catch, CGameControllerCatch)
REGISTER_GAME_TYPE(zcatch, CGameControllerZCatch)
REGISTER_GAME_TYPE(block-down, CGameControllerBD)
#endif