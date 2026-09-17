#include "Client_PCH.h"
#include "MInteractionObject.h"
#include "MZone.h"

void
MInteractionObject::ChangeActionDoor()
{
	switch (m_CurrentFrame)
	{
		// Open
		case 0 :
			g_pZone->UnSetBlockAllSector( m_X, m_Y );
		break;

		// Closed
		case 1 :
			g_pZone->SetBlockAllSector( m_X, m_Y );
		break;
	}
}

//----------------------------------------------------------------------
// ChangeAction Door
//----------------------------------------------------------------------
void
MInteractionObject::ChangeActionTrap()
{
	switch (m_CurrentFrame)
	{
		// Hidden
		case 0 :			
		break;

		// Discovered
		case 1 :
		break;
	}
}

//----------------------------------------------------------------------
// ChangeAction Switch
//----------------------------------------------------------------------
void
MInteractionObject::ChangeActionSwitch()
{
	switch (m_CurrentFrame)
	{
		// UP
		case 0 :
			
		break;

		// DOWN
		case 1 :
		break;
	}
}
