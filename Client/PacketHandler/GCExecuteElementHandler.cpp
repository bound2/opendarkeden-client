//////////////////////////////////////////////////////////////////////
//
// Filename    : GCExecuteElementHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCExecuteElement.h"

#include "DebugLog.h"
#include "VsUiPresentationHost.h"
#include "ZoneStateHost.h"
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCExecuteElementHandler::execute ( GCExecuteElement * pGCExecuteElement , Player * pPlayer )

{
	__BEGIN_TRY 
	(void)pPlayer;
//		__BEGIN_DEBUG_EX
		

	//------------------------------------------------------
	// The zone is not made yet
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
		
	}	
	//------------------------------------------------------
	// The condition names one of the element's four children
	// (Happen, Complete, Fail, Reward), which the quest window
	// looks up in a table of four.
	//------------------------------------------------------
	else if (pGCExecuteElement->getCondition() >= 4)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] GCExecuteElement: condition %d", pGCExecuteElement->getCondition());
	}
	//------------------------------------------------------
	// Otherwise
	//------------------------------------------------------
	else
	{
#ifdef __DEBUG_OUTPUT__
		DEBUG_ADD_FORMAT("[GCExecuteElementHandler] QuestID=%d, bCondition=%d, index = %d", pGCExecuteElement->getQuestID(), pGCExecuteElement->getCondition(), pGCExecuteElement->getIndex());
#endif
		VsUiPresentation::PushGQuestExecuteElement(pGCExecuteElement->getQuestID(), pGCExecuteElement->getCondition(), pGCExecuteElement->getIndex());

	}
	


//	__END_DEBUG_EX 
	__END_CATCH
}
