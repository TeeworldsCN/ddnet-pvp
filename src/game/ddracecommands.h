/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */

// This file can be included several times.

#ifndef CONSOLE_COMMAND
#define CONSOLE_COMMAND(name, params, flags, callback, userdata, help)
#endif

CONSOLE_COMMAND("kill_pl", "v[id]", CFGFLAG_SERVER, ConKillPlayer, this, "Kills player v and announces the kill")
CONSOLE_COMMAND("totele", "i[number]", CFGFLAG_SERVER | CMDFLAG_TEST, ConToTeleporter, this, "Teleports you to teleporter v")
CONSOLE_COMMAND("totelecp", "i[number]", CFGFLAG_SERVER | CMDFLAG_TEST, ConToCheckTeleporter, this, "Teleports you to checkpoint teleporter v")
CONSOLE_COMMAND("tele", "?i[id] ?i[id]", CFGFLAG_SERVER | CMDFLAG_TEST, ConTeleport, this, "Teleports player i (or you) to player i (or you to where you look at)")
CONSOLE_COMMAND("unjetpack", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnJetpack, this, "Removes the jetpack from you")
CONSOLE_COMMAND("super", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConSuper, this, "Makes you super")
CONSOLE_COMMAND("unsuper", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnSuper, this, "Removes super from you")
CONSOLE_COMMAND("endless_hook", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConEndlessHook, this, "Gives you endless hook")
CONSOLE_COMMAND("unendless_hook", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnEndlessHook, this, "Removes endless hook from you")
CONSOLE_COMMAND("unsolo", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnSolo, this, "Puts you out of solo mode")
CONSOLE_COMMAND("undeep", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConUnDeep, this, "Puts you out of deep freeze")
CONSOLE_COMMAND("left", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConGoLeft, this, "Makes you move 1 tile left")
CONSOLE_COMMAND("right", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConGoRight, this, "Makes you move 1 tile right")
CONSOLE_COMMAND("up", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConGoUp, this, "Makes you move 1 tile up")
CONSOLE_COMMAND("down", "", CFGFLAG_SERVER | CMDFLAG_TEST, ConGoDown, this, "Makes you move 1 tile down")

CONSOLE_COMMAND("move", "i[x] i[y]", CFGFLAG_SERVER | CMDFLAG_TEST, ConMove, this, "Moves to the tile with x/y-number ii")
CONSOLE_COMMAND("move_raw", "i[x] i[y]", CFGFLAG_SERVER | CMDFLAG_TEST, ConMoveRaw, this, "Moves to the point with x/y-coordinates ii")

CONSOLE_COMMAND("uninvite", "v[id] ?i[team]", CFGFLAG_SERVER, ConUninvite, this, "Uninvite player from team")

CONSOLE_COMMAND("vote_mute", "v[id] i[seconds]", CFGFLAG_SERVER, ConVoteMute, this, "Remove v's right to vote for i seconds")
CONSOLE_COMMAND("vote_unmute", "v[id]", CFGFLAG_SERVER, ConVoteUnmute, this, "Give back v's right to vote.")
CONSOLE_COMMAND("vote_mutes", "", CFGFLAG_SERVER, ConVoteMutes, this, "List the current active vote mutes.")
CONSOLE_COMMAND("mute", "", CFGFLAG_SERVER, ConMute, this, "")
CONSOLE_COMMAND("muteid", "v[id] i[seconds] ?r[reason]", CFGFLAG_SERVER, ConMuteID, this, "")
CONSOLE_COMMAND("muteip", "s[ip] i[seconds] ?r[reason]", CFGFLAG_SERVER, ConMuteIP, this, "")
CONSOLE_COMMAND("unmute", "v[id]", CFGFLAG_SERVER, ConUnmute, this, "")
CONSOLE_COMMAND("mutes", "", CFGFLAG_SERVER, ConMutes, this, "")
CONSOLE_COMMAND("moderate", "", CFGFLAG_SERVER, ConModerate, this, "Enables/disables active moderator mode for the player")
CONSOLE_COMMAND("vote_no", "", CFGFLAG_SERVER, ConVoteNo, this, "Same as \"vote no\"")

#undef CONSOLE_COMMAND
