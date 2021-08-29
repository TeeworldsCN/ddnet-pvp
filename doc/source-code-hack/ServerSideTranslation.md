# Server side translation

# APIs:
`gamecontext.h`

	class CGameContext : public IGameServer
	{
	// ...
	LoadLanguages
	UnloadLanguages

	SendChatLocalizedVL
	SendChatLocalized

	// broadcast
	SendBroadcastVL
	SendBroadcast

# TODOs:
* localized broadcast: `SendLocalizedBroadcast(va_list)`
