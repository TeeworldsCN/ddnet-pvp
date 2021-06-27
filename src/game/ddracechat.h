/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */

// This file can be included several times.

#ifndef CHAT_COMMAND
#define CHAT_COMMAND(name, params, flags, callback, userdata, help)
#endif

CHAT_COMMAND("credits", "", CFGFLAG_SERVER, ConCredits, this, "Shows the credits of the DDNet mod")
CHAT_COMMAND("rules", "", CFGFLAG_SERVER, ConRules, this, "Shows the server rules")
CHAT_COMMAND("emote", "?s[emote name] i[duration in seconds]", CFGFLAG_SERVER, ConEyeEmote, this, "Sets your tee's eye emote")
CHAT_COMMAND("eyeemote", "?s['on'|'off'|'toggle']", CFGFLAG_SERVER, ConSetEyeEmote, this, "Toggles use of standard eye-emotes on/off, eyeemote s, where s = on for on, off for off, toggle for toggle and nothing to show current status")
CHAT_COMMAND("help", "?r[command]", CFGFLAG_SERVER, ConHelp, this, "Shows help to command r, general help if left blank")
CHAT_COMMAND("info", "", CFGFLAG_SERVER, ConInfo, this, "Shows info about this server")
CHAT_COMMAND("list", "?s[filter]", CFGFLAG_CHAT, ConList, this, "List connected players with optional case-insensitive substring matching filter")
CHAT_COMMAND("me", "r[message]", CFGFLAG_SERVER, ConMe, this, "Like the famous irc command '/me says hi' will display '- <yourname>: hi'")
CHAT_COMMAND("all", "r[message]", CFGFLAG_SERVER, ConMe, this, "Like the famous irc command '/me says hi' will display '- <yourname>: hi'")
CHAT_COMMAND("w", "s[player name] r[message]", CFGFLAG_SERVER, ConWhisper, this, "Whisper something to someone (private message)")
CHAT_COMMAND("whisper", "s[player name] r[message]", CFGFLAG_SERVER, ConWhisper, this, "Whisper something to someone (private message)")
CHAT_COMMAND("c", "r[message]", CFGFLAG_SERVER, ConConverse, this, "Converse with the last person you whispered to (private message)")
CHAT_COMMAND("converse", "r[message]", CFGFLAG_SERVER, ConConverse, this, "Converse with the last person you whispered to (private message)")
CHAT_COMMAND("pause", "?r[player name]", CFGFLAG_SERVER, ConTogglePause, this, "Toggles pause")
CHAT_COMMAND("spec", "?r[player name]", CFGFLAG_SERVER, ConTogglePause, this, "Toggles pause")
CHAT_COMMAND("dnd", "", CFGFLAG_SERVER, ConDND, this, "Toggle Do Not Disturb (no chat and server messages)")
CHAT_COMMAND("timeout", "?s[code]", CFGFLAG_SERVER, ConTimeout, this, "Set timeout protection code s")

CHAT_COMMAND("team", "?i[id]", CFGFLAG_SERVER | CFGFLAG_NO_CONSENT, ConJoinTeam, this, "Lets you join room i (shows your room id if left blank)")
CHAT_COMMAND("room", "", CFGFLAG_SERVER | CFGFLAG_NO_CONSENT, ConJoinTeam, this, "Shows your room number")
CHAT_COMMAND("join", "i[id]", CFGFLAG_SERVER | CFGFLAG_NO_CONSENT, ConJoinTeam, this, "Lets you join room i")
CHAT_COMMAND("create", "?r[gametype]", CFGFLAG_SERVER | CFGFLAG_NO_CONSENT, ConCreateTeam, this, "Creates a new room with specified gametype")
CHAT_COMMAND("lock", "?i['0'|'1']", CFGFLAG_SERVER | CFGFLAG_NO_CONSENT, ConLockTeam, this, "Toggle team lock so no one else can join and so the team restarts when a player dies. /lock 0 to unlock, /lock 1 to lock.")
CHAT_COMMAND("unlock", "", CFGFLAG_SERVER | CFGFLAG_NO_CONSENT, ConUnlockTeam, this, "Unlock a team")
CHAT_COMMAND("invite", "r[player name]", CFGFLAG_SERVER, ConInviteTeam, this, "Invite a person to a locked team")
CHAT_COMMAND("ready", "", CFGFLAG_SERVER, ConReady, this, "Toggle ready state")
CHAT_COMMAND("r", "", CFGFLAG_SERVER, ConReady, this, "Toggle ready state")

CHAT_COMMAND("showothers", "?i['0'|'1'|'2']", CFGFLAG_SERVER | CFGFLAG_NO_CONSENT, ConShowOthers, this, "Whether to show other teams by default, (2 = with distracting stuff)")

CHAT_COMMAND("setting", "?r[command]", CFGFLAG_SERVER, ConInstanceCommand, this, "Invoke a command in current room (i.e. you can change room settings)")

#undef CHAT_COMMAND
