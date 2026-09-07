//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGUntransform.h 
// Written By  : crazydog
// Description : 박쥐나 늑대에서 원래모습으로 돌아오고싶을때..
// 
//////////////////////////////////////////////////////////////////////

#ifndef __CG_UNTRANSFORM_H__
#define __CG_UNTRANSFORM_H__

// include files
#include "Packet.h"
#include "PacketFactory.h"


//////////////////////////////////////////////////////////////////////
//
// class CGUntransform;
//
//////////////////////////////////////////////////////////////////////

class CGUntransform : public Packet {

public :
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_CG_UNTRANSFORM; }
	
	// get packet's body size
	// *OPTIMIZATION HINT*
	// const static CGUntransformPacketSize 를 정의해서 리턴하라.
	PacketSize_t getPacketSize () const noexcept { return 0; }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "CGUntransform"; }
		
		// get packet's debug std::string
		std::string toString () const;
	#endif
};


//////////////////////////////////////////////////////////////////////
//
// class CGUntransformFactory;
//
// Factory for CGUntransform
//
//////////////////////////////////////////////////////////////////////
class CGUntransformFactory : public PacketFactory {

public :
	
	// create packet
	Packet * createPacket () { return new CGUntransform(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "CGUntransform"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_CG_UNTRANSFORM; }

	// get packet's max body size
	// *OPTIMIZATION HINT*
	// const static CGUntransformPacketSize 를 정의해서 리턴하라.
	PacketSize_t getPacketMaxSize () const noexcept { return 0; }
	

};


//////////////////////////////////////////////////////////////////////
//
// class CGUntransformHandler;
//
//////////////////////////////////////////////////////////////////////
#ifndef __GAME_CLIENT__
	class CGUntransformHandler {
		
	public :

		// execute packet's handler
		static void execute ( CGUntransform * pPacket , Player * player );
	};

#endif

#endif