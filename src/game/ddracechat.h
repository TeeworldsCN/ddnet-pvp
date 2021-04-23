/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */

// This file can be included several times.

#ifndef CHAT_COMMAND
#define CHAT_COMMAND(name, params, flags, callback, userdata, help)
#endif

CHAT_COMMAND("credits", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConCredits, this, "Shows the credits of the DDNet mod")
CHAT_COMMAND("rules", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConRules, this, "Shows the server rules")
CHAT_COMMAND("emote", "?s[emote name] i[duration in seconds]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConEyeEmote, this, "Sets your tee's eye emote")
CHAT_COMMAND("eyeemote", "?s['on'|'off'|'toggle']", CFGFLAG_CHAT | CFGFLAG_SERVER, ConSetEyeEmote, this, "Toggles use of standard eye-emotes on/off, eyeemote s, where s = on for on, off for off, toggle for toggle and nothing to show current status")
CHAT_COMMAND("help", "?r[command]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConHelp, this, "Shows help to command r, general help if left blank")
CHAT_COMMAND("info", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConInfo, this, "Shows info about this server")
CHAT_COMMAND("list", "?s[filter]", CFGFLAG_CHAT, ConList, this, "List connected players with optional case-insensitive substring matching filter")
CHAT_COMMAND("me", "r[message]", CFGFLAG_CHAT | CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConMe, this, "Like the famous irc command '/me says hi' will display '- <yourname>: hi'")
CHAT_COMMAND("all", "r[message]", CFGFLAG_CHAT | CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConMe, this, "Like the famous irc command '/me says hi' will display '- <yourname>: hi'")
CHAT_COMMAND("w", "s[player name] r[message]", CFGFLAG_CHAT | CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConWhisper, this, "Whisper something to someone (private message)")
CHAT_COMMAND("whisper", "s[player name] r[message]", CFGFLAG_CHAT | CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConWhisper, this, "Whisper something to someone (private message)")
CHAT_COMMAND("c", "r[message]", CFGFLAG_CHAT | CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConConverse, this, "Converse with the last person you whispered to (private message)")
CHAT_COMMAND("converse", "r[message]", CFGFLAG_CHAT | CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConConverse, this, "Converse with the last person you whispered to (private message)")
CHAT_COMMAND("pause", "?r[player name]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConTogglePause, this, "Toggles pause")
CHAT_COMMAND("spec", "?r[player name]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConToggleSpec, this, "Toggles spec (if not available behaves as /pause)")
CHAT_COMMAND("pausevoted", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConTogglePauseVoted, this, "Toggles pause on the currently voted player")
CHAT_COMMAND("specvoted", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConToggleSpecVoted, this, "Toggles spec on the currently voted player")
CHAT_COMMAND("dnd", "", CFGFLAG_CHAT | CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConDND, this, "Toggle Do Not Disturb (no chat and server messages)")
CHAT_COMMAND("timeout", "?s[code]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConTimeout, this, "Set timeout protection code s")

CHAT_COMMAND("team", "?i[id]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConJoinTeam, this, "Lets you join room i (shows your team if left blank)")
CHAT_COMMAND("room", "?i[id]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConJoinTeam, this, "Lets you join room i (shows your team if left blank)")
CHAT_COMMAND("lock", "?i['0'|'1']", CFGFLAG_CHAT | CFGFLAG_SERVER, ConLockTeam, this, "Toggle team lock so no one else can join and so the team restarts when a player dies. /lock 0 to unlock, /lock 1 to lock.")
CHAT_COMMAND("unlock", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConUnlockTeam, this, "Unlock a team")
CHAT_COMMAND("invite", "r[player name]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConInviteTeam, this, "Invite a person to a locked team")

CHAT_COMMAND("showothers", "?i['0'|'1'|'2']", CFGFLAG_CHAT | CFGFLAG_SERVER, ConShowOthers, this, "Whether to show other teams by default, (2 = with distracting stuff)")
// CHAT_COMMAND("specteam", "?i['0'|'1']", CFGFLAG_CHAT | CFGFLAG_SERVER, ConSpecTeam, this, "Whether to show players in other rooms when spectating (on by default), optional i = 0 for off else for on")

#undef CHAT_COMMAND
