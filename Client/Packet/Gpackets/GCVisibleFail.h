//////////////////////////////////////////////////////////////////////
// 
// Filename    : GCVisibleFail.h 
// Written By  : crazydog
// Description :
// 
//////////////////////////////////////////////////////////////////////

#ifndef __GC_VISIBLE_FAIL_H__
#define __GC_VISIBLE_FAIL_H__

// include files
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class GCVisibleFail;
//
//////////////////////////////////////////////////////////////////////

class GCVisibleFail : public Packet {

public :

	// constructor
	GCVisibleFail () {}
	

public :

    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_GC_VISIBLE_FAIL; }
	
	// get packet's body size
	PacketSize_t getPacketSize () const noexcept { return 0; }
	
	#ifdef __DEBUG_OUTPUT__
		// get packet's name
		std::string getPacketName () const { return "GCVisibleFail"; }
		
		// get packet's debug std::string
		std::string toString () const;
	#endif
	

public :


private : 


};


//////////////////////////////////////////////////////////////////////
//
// class  GCVisibleFailFactory;
//
// Factory for  GCVisibleFail
//
//////////////////////////////////////////////////////////////////////

class  GCVisibleFailFactory : public PacketFactory {

public :
	
	// create packet
	Packet * createPacket () { return new GCVisibleFail(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "GCVisibleFail"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_GC_VISIBLE_FAIL; }

	// get packet's max body size
	PacketSize_t getPacketMaxSize () const noexcept { return 0; }
	
};


//////////////////////////////////////////////////////////////////////
//
// class  GCVisibleFailHandler;
//
//////////////////////////////////////////////////////////////////////

class  GCVisibleFailHandler {

public :

	// execute packet's handler
	static void execute ( GCVisibleFail * pPacket , Player * pPlayer );

};

#endif
