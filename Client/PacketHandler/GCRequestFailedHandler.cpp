//////////////////////////////////////////////////////////////////////
//
// Filename    : GCRequestFailedHandler.cpp
// Written By  : 김성민
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCRequestFailed.h"
#include "RequestUserManager.h"
#include "ClientDef.h"

void GCRequestFailedHandler::execute ( GCRequestFailed * pPacket , Player * pPlayer )
	 

{
	__BEGIN_TRY
	
#ifdef __GAME_CLIENT__

	if (g_Mode==MODE_GAME
		&& g_pRequestUserManager!=NULL)
	{
		// REQUEST_FAILED_IP used to drop the whisper queued for that name
		// and print a chat line; that went with the peer-to-peer whisper
		// path (docs/RESTRUCTURING.md task 5.2, seventh slice). Whatever
		// the code, the pending request is forgotten.
		g_pRequestUserManager->RemoveRequestingUser( pPacket->getName().c_str() );
	}

#endif

	__END_CATCH
}
