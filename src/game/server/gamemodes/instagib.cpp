#include "instagib.h"

template<>
CGameControllerInstagib<CGameControllerDM>::CGameControllerInstagib() :
	CGameControllerDM()
{
	m_pGameType = "iDM";
	RegisterConfig();
}

template<>
CGameControllerInstagib<CGameControllerTDM>::CGameControllerInstagib() :
	CGameControllerTDM()
{
	m_pGameType = "iTDM";
	RegisterConfig();
}

template<>
CGameControllerInstagib<CGameControllerCTF>::CGameControllerInstagib() :
	CGameControllerCTF()
{
	m_pGameType = "iCTF";
	RegisterConfig();
}
