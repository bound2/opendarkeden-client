//////////////////////////////////////////////////////////////////////
//
// Filename    : LCRegisterPlayerOKHandler.cpp
// Written By  : Reiot
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Lpackets/LCRegisterPlayerOK.h"
#include "UserInformation.h"
#include "ClientDef.h"
#include "UIFunction.h"

#ifdef __GAME_CLIENT__

	#include "ClientPlayer.h"
	#include "Cpackets/CLGetWorldList.h"

#endif

extern BOOL g_bNeedUpdate;

//////////////////////////////////////////////////////////////////////
//
// The login server created the account and treats this session as
// logged in with it. From here the flow is the one LCLoginOKHandler
// runs after a normal login: ask for the world list and wait for it.
//
//////////////////////////////////////////////////////////////////////
void LCRegisterPlayerOKHandler::execute ( LCRegisterPlayerOK * pPacket , Player * pPlayer )

{
	__BEGIN_TRY

#ifdef __GAME_CLIENT__

	// Remember the ID the way a successful login does.
	if( g_pUserInformation->UserID.GetLength() >= 15 )
		UI_BackupLoginID( "DarkEden" );
	else
		UI_BackupLoginID( g_pUserInformation->UserID );

	if (!g_bNeedUpdate)
	{
		ClientPlayer * pClientPlayer = dynamic_cast<ClientPlayer*>(pPlayer);

		g_pUserInformation->pLogInClientPlayer = pClientPlayer;

		// The rest of the handshake (world list, server list, PC list)
		// is validated against the post-login status set, so join it.
		pClientPlayer->setPlayerStatus( CPS_AFTER_SENDING_CL_LOGIN );

		CLGetWorldList clGetWorldList;
		pClientPlayer->sendPacket( &clGetWorldList );

		SetMode( MODE_WAIT_WORLD_LIST );

		//------------------------------------------------------------
		// Current server group.
		//------------------------------------------------------------
		SetServerGroupName( pPacket->getGroupName().c_str() );

		//------------------------------------------------------------
		// Gore level - same rule as LCLoginOKHandler: the server's
		// adult flag is not trusted, only the teen-version option.
		//------------------------------------------------------------
		bool bGoreLevel = !g_pUserOption->UseTeenVersion;

		g_pUserInformation->GoreLevel = bGoreLevel;

		SetGoreLevel( bGoreLevel );
	}

#endif

	__END_CATCH
}
