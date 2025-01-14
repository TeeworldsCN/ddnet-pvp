/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

// This file can be included several times.

#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc) ;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc) ;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc) ;
#endif

// server
MACRO_CONFIG_INT(SvWarmup, sv_warmup, 0, 0, 0, CFGFLAG_SERVER, "Number of seconds to do warmup before round starts")
MACRO_CONFIG_STR(SvMotd, sv_motd, 900, "", CFGFLAG_SERVER, "Message of the day to display for the clients")
MACRO_CONFIG_STR(SvGametype, sv_gametype, 32, "ddnet", CFGFLAG_SERVER, "Game type (ddnet, mod)")
MACRO_CONFIG_INT(SvTournamentMode, sv_tournament_mode, 0, 0, 1, CFGFLAG_SERVER, "Tournament mode. When enabled, players joins the server as spectator")
MACRO_CONFIG_INT(SvSpamprotection, sv_spamprotection, 1, 0, 1, CFGFLAG_SERVER, "Spam protection for: team change, chat, skin change, emotes and votes")

MACRO_CONFIG_INT(SvSpectatorSlots, sv_spectator_slots, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "Number of slots to reserve for spectators")
MACRO_CONFIG_INT(SvInactiveKickTime, sv_inactivekick_time, 0, 0, 1000, CFGFLAG_SERVER, "How many minutes to wait before taking care of inactive players")
MACRO_CONFIG_INT(SvInactiveKick, sv_inactivekick, 0, 0, 2, CFGFLAG_SERVER, "How to deal with inactive players (0=move to spectator, 1=move to free spectator slot/kick, 2=kick)")

MACRO_CONFIG_INT(SvStrictSpectateMode, sv_strict_spectate_mode, 0, 0, 1, CFGFLAG_SERVER, "Restricts information in spectator mode")
MACRO_CONFIG_INT(SvVoteSpectate, sv_vote_spectate, 1, 0, 1, CFGFLAG_SERVER, "Allow voting to move players to spectators")
MACRO_CONFIG_INT(SvVoteSpectateRejoindelay, sv_vote_spectate_rejoindelay, 3, 0, 1000, CFGFLAG_SERVER, "How many minutes to wait before a player can rejoin after being moved to spectators by vote")
MACRO_CONFIG_INT(SvVoteKick, sv_vote_kick, 1, 0, 1, CFGFLAG_SERVER, "Allow voting to kick players")
MACRO_CONFIG_INT(SvVoteKickMin, sv_vote_kick_min, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "Minimum number of players required to start a kick vote")
MACRO_CONFIG_INT(SvVoteKickBantime, sv_vote_kick_bantime, 5, 0, 1440, CFGFLAG_SERVER, "The time in seconds to ban a player if kicked by vote. 0 makes it just use kick")
MACRO_CONFIG_INT(SvJoinVoteDelay, sv_join_vote_delay, 300, 0, 1000, CFGFLAG_SERVER, "Add a delay before recently joined players can call any vote or participate in a kick/spec vote (in seconds)")
MACRO_CONFIG_INT(SvOldTeleportWeapons, sv_old_teleport_weapons, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Teleporting of all weapons (deprecated, use special entities instead)")
MACRO_CONFIG_INT(SvOldTeleportHook, sv_old_teleport_hook, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Hook through teleporter (deprecated, use special entities instead)")
MACRO_CONFIG_INT(SvTeleportHoldHook, sv_teleport_hold_hook, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Hold hook when teleported")
MACRO_CONFIG_INT(SvTeleportLoseWeapons, sv_teleport_lose_weapons, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Lose weapons when teleported (useful for some race maps)")
MACRO_CONFIG_INT(SvDeepfly, sv_deepfly, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Allow fire non auto weapons when deep")
MACRO_CONFIG_INT(SvDestroyBulletsOnDeath, sv_destroy_bullets_on_death, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Destroy bullets when their owner dies")
MACRO_CONFIG_INT(SvDestroyLasersOnDeath, sv_destroy_lasers_on_death, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Destroy lasers when their owner dies")

MACRO_CONFIG_INT(SvMapUpdateRate, sv_mapupdaterate, 5, 1, 100, CFGFLAG_SERVER, "64 player id <-> vanilla id players map update rate")

MACRO_CONFIG_STR(SvServerType, sv_server_type, 64, "none", CFGFLAG_SERVER, "Type of the server (novice, moderate, ...)")

MACRO_CONFIG_INT(SvSendVotesPerTick, sv_send_votes_per_tick, 5, 1, 15, CFGFLAG_SERVER, "Number of vote options being send per tick")

MACRO_CONFIG_INT(SvRescue, sv_rescue, 0, 0, 1, CFGFLAG_SERVER, "Allow /rescue command so players can teleport themselves out of freeze (setting only works in initial config)")
MACRO_CONFIG_INT(SvRescueDelay, sv_rescue_delay, 1, 0, 1000, CFGFLAG_SERVER, "Number of seconds between two rescues")
MACRO_CONFIG_INT(SvPractice, sv_practice, 1, 0, 1, CFGFLAG_SERVER, "Enable practice mode for teams. Means you can use /rescue, but in turn your rank doesn't count.")

// debug
#ifdef CONF_DEBUG
MACRO_CONFIG_INT(DbgDummies, dbg_dummies, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "Add debug dummies to server (Debug build only)")
#endif

MACRO_CONFIG_STR(Password, password, 256, "", CFGFLAG_CLIENT | CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, "Password to the server")
MACRO_CONFIG_INT(Events, events, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Enable triggering of events, (eye emotes on some holidays in server, christmas skins in client).")

MACRO_CONFIG_STR(Logfile, logfile, 128, "", CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Filename to log all output to")
MACRO_CONFIG_INT(Logappend, logappend, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Append to logfile instead of overwriting it every time")
MACRO_CONFIG_INT(Loglevel, loglevel, 0, -3, 2, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Adjusts the amount of information in the logfile (-3 = none, -2 = error only, -1 = warn, 0 = info, 1 = debug, 2 = trace)")
MACRO_CONFIG_INT(StdoutOutputLevel, stdout_output_level, 0, -3, 2, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Adjusts the amount of information in the system console (-3 = none, -2 = error only, -1 = warn, 0 = info, 1 = debug, 2 = trace)")
MACRO_CONFIG_INT(ConsoleOutputLevel, console_output_level, 0, -3, 2, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Adjusts the amount of information in the local/remote console (-3 = none, -2 = error only, -1 = warn, 0 = info, 1 = debug, 2 = trace)")
MACRO_CONFIG_INT(ConsoleEnableColors, console_enable_colors, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Enable colors in console output")

MACRO_CONFIG_STR(SvName, sv_name, 128, "unnamed server", CFGFLAG_SERVER, "Server name")
MACRO_CONFIG_STR(Bindaddr, bindaddr, 128, "", CFGFLAG_CLIENT | CFGFLAG_SERVER | CFGFLAG_MASTER, "Address to bind the client/server to")
MACRO_CONFIG_INT(SvIpv4Only, sv_ipv4only, 0, 0, 1, CFGFLAG_SERVER, "Whether to bind only to ipv4, otherwise bind to all available interfaces")
MACRO_CONFIG_INT(SvPort, sv_port, 0, 0, 65535, CFGFLAG_SERVER, "Port to use for the server (Only ports 8303-8310 work in LAN server browser, 0 to automatically find a free port in 8303-8310)")
MACRO_CONFIG_STR(SvHostname, sv_hostname, 128, "", CFGFLAG_SERVER, "Server hostname (0.7 only)")
MACRO_CONFIG_STR(SvMap, sv_map, 128, "Sunny Side Up", CFGFLAG_SERVER, "Map to use on the server")
MACRO_CONFIG_INT(SvMaxClients, sv_max_clients, MAX_CLIENTS, 1, MAX_CLIENTS, CFGFLAG_SERVER, "Maximum number of clients that are allowed on a server")
MACRO_CONFIG_INT(SvMaxClientsPerIp, sv_max_clients_per_ip, 4, 1, MAX_CLIENTS, CFGFLAG_SERVER, "Maximum number of clients with the same IP that can connect to the server")
MACRO_CONFIG_INT(SvHighBandwidth, sv_high_bandwidth, 0, 0, 1, CFGFLAG_SERVER, "Use high bandwidth mode. Doubles the bandwidth required for the server. LAN use only")
MACRO_CONFIG_STR(SvRegister, sv_register, 16, "1", CFGFLAG_SERVER, "Register server with master server for public listing, can also accept a comma-separated list of protocols to register on, like 'ipv4,ipv6'")
MACRO_CONFIG_STR(SvRegisterExtra, sv_register_extra, 256, "", CFGFLAG_SERVER, "Extra headers to send to the register endpoint, comma separated 'Header: Value' pairs")
MACRO_CONFIG_STR(SvRegisterUrl, sv_register_url, 128, "https://master1.ddnet.org/ddnet/15/register", CFGFLAG_SERVER, "Masterserver URL to register to")
MACRO_CONFIG_STR(SvMapsBaseUrl, sv_maps_base_url, 128, "", CFGFLAG_SERVER, "Base path used to provide HTTPS map download URL to the clients")
MACRO_CONFIG_STR(SvRconPassword, sv_rcon_password, 128, "", CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, "Remote console password (full access)")
MACRO_CONFIG_STR(SvRconModPassword, sv_rcon_mod_password, 128, "", CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, "Remote console password for moderators (limited access)")
MACRO_CONFIG_STR(SvRconHelperPassword, sv_rcon_helper_password, 128, "", CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, "Remote console password for helpers (limited access)")
MACRO_CONFIG_INT(SvRconMaxTries, sv_rcon_max_tries, 30, 0, 100, CFGFLAG_SERVER, "Maximum number of tries for remote console authentication")
MACRO_CONFIG_INT(SvRconBantime, sv_rcon_bantime, 5, 0, 1440, CFGFLAG_SERVER, "The time a client gets banned if remote console authentication fails. 0 makes it just use kick")
MACRO_CONFIG_INT(SvAutoDemoRecord, sv_auto_demo_record, 0, 0, 1, CFGFLAG_SERVER, "Automatically record demos")
MACRO_CONFIG_INT(SvAutoDemoMax, sv_auto_demo_max, 10, 0, 1000, CFGFLAG_SERVER, "Maximum number of automatically recorded demos (0 = no limit)")
MACRO_CONFIG_INT(SvTeeHistorian, sv_tee_historian, 0, 0, 1, CFGFLAG_SERVER, "Activate the tee historian that writes complete gameplay data to disk (WARNING: This will use a lot of disk space)")
MACRO_CONFIG_INT(SvVanillaAntiSpoof, sv_vanilla_antispoof, 1, 0, 1, CFGFLAG_SERVER, "Enable vanilla Antispoof")
MACRO_CONFIG_INT(SvDnsbl, sv_dnsbl, 0, 0, 1, CFGFLAG_SERVER, "Enable DNSBL (DNS-based Blackhole List)")
MACRO_CONFIG_STR(SvDnsblHost, sv_dnsbl_host, 128, "", CFGFLAG_SERVER, "Hostname of DNSBL provider to use for IP Verification")
MACRO_CONFIG_STR(SvDnsblKey, sv_dnsbl_key, 128, "", CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, "Optional Authentication Key for the specified DNSBL provider")
MACRO_CONFIG_INT(SvDnsblVote, sv_dnsbl_vote, 0, 0, 1, CFGFLAG_SERVER, "Block votes by blacklisted addresses")
MACRO_CONFIG_INT(SvDnsblBan, sv_dnsbl_ban, 0, 0, 1, CFGFLAG_SERVER, "Automatically ban blacklisted addresses")
MACRO_CONFIG_STR(SvDnsblBanReason, sv_dnsbl_ban_reason, 128, "VPN detected, try connecting without. Contact admin if mistaken", CFGFLAG_SERVER, "Ban reason for 'sv_dnsbl_ban'")
MACRO_CONFIG_INT(SvDnsblChat, sv_dnsbl_chat, 0, 0, 1, CFGFLAG_SERVER, "Don't allow chat from blacklisted addresses")
MACRO_CONFIG_INT(SvRconVote, sv_rcon_vote, 0, 0, 1, CFGFLAG_SERVER, "Only allow authed clients to call votes")

MACRO_CONFIG_INT(SvPlayerDemoRecord, sv_player_demo_record, 0, 0, 1, CFGFLAG_SERVER, "Automatically record demos for each player")
MACRO_CONFIG_INT(SvDemoChat, sv_demo_chat, 0, 0, 1, CFGFLAG_SERVER, "Record chat for demos")
MACRO_CONFIG_INT(SvServerInfoPerSecond, sv_server_info_per_second, 50, 0, 10000, CFGFLAG_SERVER, "Maximum number of complete server info responses that are sent out per second (0 for no limit)")
MACRO_CONFIG_INT(SvVanConnPerSecond, sv_van_conn_per_second, 10, 0, 10000, CFGFLAG_SERVER, "Antispoof specific ratelimit (0 for no limit)")
MACRO_CONFIG_INT(SvSixup, sv_sixup, 1, 0, 1, CFGFLAG_SERVER, "Enable sixup connections")
MACRO_CONFIG_INT(SvSkillLevel, sv_skill_level, 1, SERVERINFO_LEVEL_MIN, SERVERINFO_LEVEL_MAX, CFGFLAG_SERVER, "Difficulty level for Teeworlds 0.7 (0: Casual, 1: Normal, 2: Competitive)")

MACRO_CONFIG_STR(EcBindaddr, ec_bindaddr, 128, "localhost", CFGFLAG_ECON, "Address to bind the external console to. Anything but 'localhost' is dangerous")
MACRO_CONFIG_INT(EcPort, ec_port, 0, 0, 65535, CFGFLAG_ECON, "Port to use for the external console")
MACRO_CONFIG_STR(EcPassword, ec_password, 128, "", CFGFLAG_ECON, "External console password. This option is required to be set for econ to be enabled.")
MACRO_CONFIG_INT(EcBantime, ec_bantime, 0, 0, 1440, CFGFLAG_ECON, "The time a client gets banned if econ authentication fails. 0 just closes the connection")
MACRO_CONFIG_INT(EcAuthTimeout, ec_auth_timeout, 30, 1, 120, CFGFLAG_ECON, "Time in seconds before the the econ authentication times out")
MACRO_CONFIG_INT(EcOutputLevel, ec_output_level, 0, -3, 2, CFGFLAG_ECON, "Adjusts the amount of information in the external console (-3 = none, -2 = error only, -1 = warn, 0 = info, 1 = debug, 2 = trace)")

MACRO_CONFIG_INT(Debug, debug, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SERVER, "Debug mode")
MACRO_CONFIG_INT(DbgSql, dbg_sql, 1, 0, 1, CFGFLAG_SERVER, "Debug SQL")
MACRO_CONFIG_INT(DbgCurl, dbg_curl, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SERVER, "Debug curl")

MACRO_CONFIG_INT(HttpAllowInsecure, http_allow_insecure, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SERVER, "Allow insecure HTTP protocol in addition to the secure HTTPS one. Mostly useful for testing.")

// DDRace
MACRO_CONFIG_STR(SvWelcome, sv_welcome, 256, "", CFGFLAG_SERVER, "Message that will be displayed to players who join the server")
MACRO_CONFIG_INT(SvReservedSlots, sv_reserved_slots, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "The number of slots that are reserved for special players")
MACRO_CONFIG_STR(SvReservedSlotsPass, sv_reserved_slots_pass, 256, "", CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, "The password that is required to use a reserved slot")
MACRO_CONFIG_INT(SvReservedSlotsAuthLevel, sv_reserved_slots_auth_level, 1, 1, 4, CFGFLAG_SERVER, "Minimum rcon auth level needed to use a reserved slot. 4 = rcon auth disabled")
MACRO_CONFIG_INT(SvHit, sv_hit, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether players can hammer/grenade/laser each other or not")
MACRO_CONFIG_INT(SvEndlessDrag, sv_endless_drag, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Turns endless hooking on/off")
MACRO_CONFIG_INT(SvTestingCommands, sv_test_cmds, 0, 0, 1, CFGFLAG_SERVER, "Turns testing commands aka cheats on/off (setting only works in initial config)")
MACRO_CONFIG_INT(SvFreezeDelay, sv_freeze_delay, 3, 1, 30, CFGFLAG_SERVER | CFGFLAG_GAME, "How many seconds the players will remain frozen (applies to all except delayed freeze in switch layer & deepfreeze)")

MACRO_CONFIG_INT(SvEndlessSuperHook, sv_endless_super_hook, 0, 0, 1, CFGFLAG_SERVER, "Endless hook for super players on/off")
MACRO_CONFIG_INT(SvHideScore, sv_hide_score, 0, 0, 1, CFGFLAG_SERVER, "Whether players scores will be announced or not")
MACRO_CONFIG_INT(SvSaveWorseScores, sv_save_worse_scores, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether to save worse scores when you already have a better one")
MACRO_CONFIG_INT(SvPauseable, sv_pauseable, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether players can pause their char or not")
MACRO_CONFIG_INT(SvPauseMessages, sv_pause_messages, 0, 0, 1, CFGFLAG_SERVER, "Whether to show messages when a player pauses and resumes")
MACRO_CONFIG_INT(SvSpecFrequency, sv_pause_frequency, 1, 0, 9999, CFGFLAG_SERVER, "The minimum allowed delay between /spec")
MACRO_CONFIG_INT(SvInvite, sv_invite, 1, 0, 1, CFGFLAG_SERVER, "Whether players can invite other players to teams")
MACRO_CONFIG_INT(SvInviteFrequency, sv_invite_frequency, 1, 0, 9999, CFGFLAG_SERVER, "The minimum allowed delay between invites")
MACRO_CONFIG_INT(SvTeleOthersAuthLevel, sv_tele_others_auth_level, 1, 1, 3, CFGFLAG_SERVER, "The auth level you need to tele others")
MACRO_CONFIG_INT(SvRegionalRankings, sv_regional_rankings, 1, 0, 1, CFGFLAG_SERVER, "Display regional rankings in /rank, /top5 and /top5team")

MACRO_CONFIG_INT(SvEmotionalTees, sv_emotional_tees, 1, -1, 1, CFGFLAG_SERVER, "Whether eye change of tees is enabled with emoticons = 1, not = 0, -1 not at all")
MACRO_CONFIG_INT(SvEmoticonMsDelay, sv_emoticon_ms_delay, 3000, 20, 999999999, CFGFLAG_SERVER, "The time in ms a player has to wait before allowing the next over-head emoticons")
MACRO_CONFIG_INT(SvGlobalEmoticonMsDelay, sv_global_emoticon_ms_delay, 3000, 20, 999999999, CFGFLAG_SERVER, "The time in ms a player has to wait before allowing the next over-head emoticons that is send to all clients (this config must be higher or equal to sv_emoticon_ms_delay to have an effect)")
MACRO_CONFIG_INT(SvEyeEmoteChangeDelay, sv_eye_emote_change_delay, 1, 0, 9999, CFGFLAG_SERVER, "The time in seconds between eye emoticons change")

MACRO_CONFIG_INT(SvChatDelay, sv_chat_delay, 1, 0, 9999, CFGFLAG_SERVER, "The time in seconds between chat messages")
MACRO_CONFIG_INT(SvTeamChangeDelay, sv_team_change_delay, 3, 0, 9999, CFGFLAG_SERVER, "The time in seconds between team changes (spectator/in game)")
MACRO_CONFIG_INT(SvInfoChangeDelay, sv_info_change_delay, 5, 0, 9999, CFGFLAG_SERVER, "The time in seconds between info changes (name/skin/color), to avoid ranbow mod set this to a very high time")
MACRO_CONFIG_INT(SvVoteTime, sv_vote_time, 25, 1, 60, CFGFLAG_SERVER, "The time in seconds a vote lasts")
MACRO_CONFIG_INT(SvVoteMapTimeDelay, sv_vote_map_delay, 0, 0, 9999, CFGFLAG_SERVER, "The minimum time in seconds between map votes")
MACRO_CONFIG_INT(SvVoteDelay, sv_vote_delay, 3, 0, 9999, CFGFLAG_SERVER, "The time in seconds between any vote")
MACRO_CONFIG_INT(SvVoteKickDelay, sv_vote_kick_delay, 0, 0, 9999, CFGFLAG_SERVER, "The minimum time in seconds between kick votes")
MACRO_CONFIG_INT(SvVoteYesPercentage, sv_vote_yes_percentage, 50, 1, 99, CFGFLAG_SERVER, "More than this percentage of players need to agree for a vote to succeed")
MACRO_CONFIG_INT(SvVoteMajority, sv_vote_majority, 0, 0, 1, CFGFLAG_SERVER, "Whether non-voting players are considered as votes for \"no\" (0) or are ignored (1)")
MACRO_CONFIG_INT(SvVoteMaxTotal, sv_vote_max_total, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "How many players can participate in a vote at max (0 = no limit)")
MACRO_CONFIG_INT(SvVoteVetoTime, sv_vote_veto_time, 20, 0, 1000, CFGFLAG_SERVER, "Minutes of time on a server until a player can veto map change votes (0 = disabled)")
MACRO_CONFIG_INT(SvKillDelay, sv_kill_delay, 1, 0, 9999, CFGFLAG_SERVER, "The minimum time in seconds between kills")

MACRO_CONFIG_INT(SvMapWindow, sv_map_window, 15, 0, 100, CFGFLAG_SERVER, "Map downloading send-ahead window")
MACRO_CONFIG_INT(SvFastDownload, sv_fast_download, 1, 0, 1, CFGFLAG_SERVER, "Enables fast download of maps")

MACRO_CONFIG_INT(SvShotgunBulletSound, sv_shotgun_bullet_sound, 0, 0, 1, CFGFLAG_SERVER, "Crazy shotgun bullet sound on/off")

MACRO_CONFIG_STR(SvRegionName, sv_region_name, 5, "UNK", CFGFLAG_SERVER, "Server region. Used for regional bans")
MACRO_CONFIG_STR(SvSqlServerName, sv_sql_servername, 5, "UNK", CFGFLAG_SERVER, "SQL Server name that is inserted into record table")
MACRO_CONFIG_INT(SvSaveGames, sv_savegames, 1, 0, 1, CFGFLAG_SERVER, "Enables savegames (/save and /load)")
MACRO_CONFIG_INT(SvSaveSwapGamesDelay, sv_saveswapgames_delay, 30, 0, 10000, CFGFLAG_SERVER, "Delay in seconds for loading a savegame or before swapping")
MACRO_CONFIG_INT(SvSaveSwapGamesPenalty, sv_saveswapgames_penalty, 60, 0, 10000, CFGFLAG_SERVER, "Penalty in seconds for saving or swapping position")
MACRO_CONFIG_INT(SvSwapTimeout, sv_swap_timeout, 180, 0, 10000, CFGFLAG_SERVER, "Timeout in seconds before option to swap expires")
MACRO_CONFIG_INT(SvSwap, sv_swap, 1, 0, 1, CFGFLAG_SERVER, "Enable /swap")
MACRO_CONFIG_INT(SvTeam0Mode, sv_team0mode, 1, 0, 1, CFGFLAG_SERVER, "Enables /team0mode")
MACRO_CONFIG_INT(SvUseSql, sv_use_sql, 0, 0, 1, CFGFLAG_SERVER, "Enables MySQL backend instead of SQLite backend (sv_sqlite_file is still used as fallback write server when no MySQL server is reachable)")
MACRO_CONFIG_INT(SvSqlQueriesDelay, sv_sql_queries_delay, 1, 0, 20, CFGFLAG_SERVER, "Delay in seconds between SQL queries of a single player")
MACRO_CONFIG_STR(SvSqliteFile, sv_sqlite_file, 64, "ddnet-server.sqlite", CFGFLAG_SERVER, "File to store ranks in case sv_use_sql is turned off or used as backup sql server")

#if defined(CONF_UPNP)
MACRO_CONFIG_INT(SvUseUPnP, sv_use_upnp, 0, 0, 1, CFGFLAG_SERVER, "Enables UPnP support. (Requires -DCONF_UPNP=ON when compiling)")
#endif

MACRO_CONFIG_INT(SvDDRaceRules, sv_ddrace_rules, 1, 0, 1, CFGFLAG_SERVER, "Whether the default mod rules are displayed or not")
MACRO_CONFIG_STR(SvRulesLine1, sv_rules_line1, 128, "", CFGFLAG_SERVER, "Rules line 1")
MACRO_CONFIG_STR(SvRulesLine2, sv_rules_line2, 128, "", CFGFLAG_SERVER, "Rules line 2")
MACRO_CONFIG_STR(SvRulesLine3, sv_rules_line3, 128, "", CFGFLAG_SERVER, "Rules line 3")
MACRO_CONFIG_STR(SvRulesLine4, sv_rules_line4, 128, "", CFGFLAG_SERVER, "Rules line 4")
MACRO_CONFIG_STR(SvRulesLine5, sv_rules_line5, 128, "", CFGFLAG_SERVER, "Rules line 5")
MACRO_CONFIG_STR(SvRulesLine6, sv_rules_line6, 128, "", CFGFLAG_SERVER, "Rules line 6")
MACRO_CONFIG_STR(SvRulesLine7, sv_rules_line7, 128, "", CFGFLAG_SERVER, "Rules line 7")
MACRO_CONFIG_STR(SvRulesLine8, sv_rules_line8, 128, "", CFGFLAG_SERVER, "Rules line 8")
MACRO_CONFIG_STR(SvRulesLine9, sv_rules_line9, 128, "", CFGFLAG_SERVER, "Rules line 9")
MACRO_CONFIG_STR(SvRulesLine10, sv_rules_line10, 128, "", CFGFLAG_SERVER, "Rules line 10")

MACRO_CONFIG_INT(SvTeam, sv_team, 1, 0, 3, CFGFLAG_SERVER | CFGFLAG_GAME, "Teams configuration (0 = off, 1 = on but optional, 2 = must play only with teams, 3 = forced random team only for you)")
MACRO_CONFIG_INT(SvMinTeamSize, sv_min_team_size, 2, 1, MAX_CLIENTS, CFGFLAG_SERVER | CFGFLAG_GAME, "Minimum team size (finishing in a team smaller than this size gives you no teamrank)")
MACRO_CONFIG_INT(SvMaxTeamSize, sv_max_team_size, MAX_CLIENTS, 1, MAX_CLIENTS, CFGFLAG_SERVER | CFGFLAG_GAME, "Maximum team size")
MACRO_CONFIG_INT(SvMapVote, sv_map_vote, 1, 0, 1, CFGFLAG_SERVER, "Whether to allow /map")

MACRO_CONFIG_STR(SvAnnouncementFileName, sv_announcement_filename, IO_MAX_PATH_LENGTH, "announcement.txt", CFGFLAG_SERVER, "File which contains the announcements, one on each line")
MACRO_CONFIG_INT(SvAnnouncementInterval, sv_announcement_interval, 120, 1, 9999, CFGFLAG_SERVER, "The time (minutes) for how often an announcement will be displayed from the announcement file")
MACRO_CONFIG_INT(SvAnnouncementRandom, sv_announcement_random, 1, 0, 1, CFGFLAG_SERVER, "Whether announcements are sequential or random")

MACRO_CONFIG_INT(SvOldLaser, sv_old_laser, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether lasers can hit you if you shot them and that they pull you towards the bounce origin (0 for all new maps) or lasers can't hit you if you shot them, and they pull others towards the shooter")
MACRO_CONFIG_INT(SvSlashMe, sv_slash_me, 0, 0, 1, CFGFLAG_SERVER, "Whether /me is active on the server or not")
MACRO_CONFIG_INT(SvRejoinTeam0, sv_rejoin_team_0, 1, 0, 1, CFGFLAG_SERVER, "Make a team automatically rejoin team 0 after finish (only if not locked)")

MACRO_CONFIG_INT(SvNoWeakHook, sv_no_weak_hook, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether to use an alternative calculation for world ticks, that makes the hook behave like all players have strong.")

MACRO_CONFIG_INT(ConnTimeout, conn_timeout, 100, 5, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Network timeout")
MACRO_CONFIG_INT(ConnTimeoutProtection, conn_timeout_protection, 1000, 5, 10000, CFGFLAG_SERVER, "Network timeout protection")

MACRO_CONFIG_INT(SvResetPickups, sv_reset_pickups, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether the weapons are reset on passing the start tile or not")

// menu background map

MACRO_CONFIG_INT(SvShowOthers, sv_show_others, 1, 0, 1, CFGFLAG_SERVER, "Whether players can use the command showothers or not")
MACRO_CONFIG_INT(SvShowOthersDefault, sv_show_others_default, 0, 0, 2, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether players see others by default (2 for own team)")
MACRO_CONFIG_INT(SvShowAllDefault, sv_show_all_default, 0, 0, 1, CFGFLAG_SERVER, "Whether players see all tees by default")
MACRO_CONFIG_INT(SvMaxAfkTime, sv_max_afk_time, 300, 0, 9999, CFGFLAG_SERVER, "The time in seconds a player to be afk (0 = disabled)")
MACRO_CONFIG_INT(SvPlasmaRange, sv_plasma_range, 700, 1, 99999, CFGFLAG_SERVER | CFGFLAG_GAME, "How far will the plasma gun track tees")
MACRO_CONFIG_INT(SvPlasmaPerSec, sv_plasma_per_sec, 3, 0, 50, CFGFLAG_SERVER | CFGFLAG_GAME, "How many shots does the plasma gun fire per seconds")
MACRO_CONFIG_INT(SvDraggerRange, sv_dragger_range, 700, 1, 99999, CFGFLAG_SERVER | CFGFLAG_GAME, "How far will the dragger track tees")
MACRO_CONFIG_INT(SvVotePause, sv_vote_pause, 1, 0, 1, CFGFLAG_SERVER, "Allow voting to pause players (instead of moving to spectators)")
MACRO_CONFIG_INT(SvVotePauseTime, sv_vote_pause_time, 10, 0, 360, CFGFLAG_SERVER, "The time (in seconds) players have to wait in pause when paused by vote")
MACRO_CONFIG_INT(SvTuneReset, sv_tune_reset, 1, 0, 1, CFGFLAG_SERVER, "Whether tuning is reset after each map change or not")
MACRO_CONFIG_STR(SvResetFile, sv_reset_file, 128, "reset.cfg", CFGFLAG_SERVER, "File to execute on map change or reload to set the default server settings")
MACRO_CONFIG_STR(SvInputFifo, sv_input_fifo, 128, "", CFGFLAG_SERVER, "Fifo file (non-Windows) or Named Pipe (Windows) to use as input for server console")
MACRO_CONFIG_INT(SvDDRaceTuneReset, sv_ddrace_tune_reset, 1, 0, 1, CFGFLAG_SERVER, "Whether DDRace tuning (sv_hit, sv_endless_drag and sv_old_laser) is reset after each map change or not")
MACRO_CONFIG_INT(SvNamelessScore, sv_nameless_score, 1, 0, 1, CFGFLAG_SERVER, "Whether nameless tee has a score or not")
MACRO_CONFIG_INT(SvTimeInBroadcastInterval, sv_time_in_broadcast_interval, 1, 0, 60, CFGFLAG_SERVER, "How often to update the broadcast time")
MACRO_CONFIG_INT(SvDefaultTimerType, sv_default_timer_type, 0, 0, 3, CFGFLAG_SERVER, "Default way of displaying time either game/round timer or broadcast. 0 = game/round timer, 1 = broadcast, 2 = 0+1, 3 = none")

// these might need some fine tuning
MACRO_CONFIG_INT(SvChatInitialDelay, sv_chat_initial_delay, 0, 0, 360, CFGFLAG_SERVER, "The time in seconds before the first message can be sent")
MACRO_CONFIG_INT(SvChatPenalty, sv_chat_penalty, 250, 50, 1000, CFGFLAG_SERVER, "chat score will be increased by this on every message, and decremented by 1 on every tick.")
MACRO_CONFIG_INT(SvChatThreshold, sv_chat_threshold, 1000, 50, 10000, CFGFLAG_SERVER, "if chats score exceeds this, the player will be muted for sv_spam_mute_duration seconds")
MACRO_CONFIG_INT(SvSpamMuteDuration, sv_spam_mute_duration, 60, 0, 3600, CFGFLAG_SERVER, "how many seconds to mute, if player triggers mute on spam. 0 = off")

MACRO_CONFIG_INT(SvShutdownWhenEmpty, sv_shutdown_when_empty, 0, 0, 1, CFGFLAG_SERVER, "Shutdown server as soon as no one is on it anymore")
MACRO_CONFIG_INT(SvReloadWhenEmpty, sv_reload_when_empty, 0, 0, 2, CFGFLAG_SERVER, "Reload map when server is empty (1 = reload once, 2 = reload every time server gets empty)")
MACRO_CONFIG_INT(SvKillProtection, sv_kill_protection, 20, 0, 9999, CFGFLAG_SERVER, "0 - Disable, 1-9999 minutes")
MACRO_CONFIG_INT(SvSoloServer, sv_solo_server, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Set server to solo mode (no player interactions, has to be set before loading the map)")
MACRO_CONFIG_STR(SvClientSuggestion, sv_client_suggestion, 128, "Get DDNet client from DDNet.org to use all features on DDNet!", CFGFLAG_SERVER, "Broadcast to display to players without DDNet client")
MACRO_CONFIG_STR(SvClientSuggestionOld, sv_client_suggestion_old, 128, "Your DDNet client is old, update it on DDNet.org!", CFGFLAG_SERVER, "Broadcast to display to players with an old version of DDNet client")
MACRO_CONFIG_STR(SvClientSuggestionBot, sv_client_suggestion_bot, 128, "Your client has bots and can be remotely controlled!\nPlease use another client like DDNet client from DDNet.org", CFGFLAG_SERVER, "Broadcast to display to players with a known botting client")
MACRO_CONFIG_STR(SvBannedVersions, sv_banned_versions, 128, "", CFGFLAG_SERVER, "Comma separated list of banned clients to be kicked on join")

// netlimit
MACRO_CONFIG_INT(SvNetlimit, sv_netlimit, 0, 0, 10000, CFGFLAG_SERVER, "Netlimit: Maximum amount of traffic a client is allowed to use (in kb/s)")
MACRO_CONFIG_INT(SvNetlimitAlpha, sv_netlimit_alpha, 50, 1, 100, CFGFLAG_SERVER, "Netlimit: Alpha of Exponention moving average")

MACRO_CONFIG_INT(SvConnlimit, sv_connlimit, 5, 0, 100, CFGFLAG_SERVER, "Connlimit: Number of connections an IP is allowed to do in a timespan")
MACRO_CONFIG_INT(SvConnlimitTime, sv_connlimit_time, 20, 0, 1000, CFGFLAG_SERVER, "Connlimit: Time in which IP's connections are counted")

#if defined(CONF_FAMILY_UNIX)
MACRO_CONFIG_STR(SvConnLoggingServer, sv_conn_logging_server, 128, "", CFGFLAG_SERVER, "Unix socket server for IP address logging (Unix only)")
#endif
