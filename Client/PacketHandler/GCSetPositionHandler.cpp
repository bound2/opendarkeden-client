//----------------------------------------------------------------------
//
// Filename    : GCSetPositionHandler.cc
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSetPosition.h"
#include "ClientDef.h"

	#include "ClientPlayer.h"

//----------------------------------------------------------------------
// 
// GCSetPositionHander::execute
//
//----------------------------------------------------------------------
void GCSetPositionHandler::execute ( GCSetPosition * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
		

	ClientPlayer * pClientPlayer = dynamic_cast<ClientPlayer*>(pPlayer);

	DEBUG_ADD_FORMAT("Set Position To ( %d, %d ) to Dir(%d)", (int)pPacket->getX(), (int)pPacket->getY(), (int)pPacket->getDir());		
	
	pClientPlayer->setXY( pPacket->getX() , pPacket->getY() );

	pClientPlayer->setPlayerStatus( CPS_NORMAL ); 


	//--------------------------------------------------------
	// Player의 위치 지정
	//--------------------------------------------------------
	InitPlayer(	pPacket->getX(), 
				pPacket->getY(),
				pPacket->getDir());

	//--------------------------------------------------------
	// 게임 시작..
	//--------------------------------------------------------
	SetMode( MODE_GAME );

	
	__END_CATCH
}
