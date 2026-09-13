//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGVerifyTime.h 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __CG_VERIFY_TIME_H__
#define __CG_VERIFY_TIME_H__

// include files

#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class CGVerifyTime;
//
// 클라이언트가 서버에게 보내는 VerifyTime 패킷이다.
// 내부에 VerifyTime String 만을 데이타 필드로 가진다.
//
//////////////////////////////////////////////////////////////////////

class Player;
class GamePlayer;

class CGVerifyTime : public Packet {

public :
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_CG_VERIFY_TIME; }
	
	// get packet's body size
	PacketSize_t getPacketSize () const noexcept { return 0; }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "CGVerifyTime"; }
		
		// get packet's debug std::string
		std::string toString () const;
	#endif

private :
	
};


//////////////////////////////////////////////////////////////////////
//
// class CGVerifyTimeFactory;
//
// Factory for CGVerifyTime
//
//////////////////////////////////////////////////////////////////////
class CGVerifyTimeFactory : public PacketFactory {

public :
	
	// create packet
	Packet * createPacket () { return new CGVerifyTime(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "CGVerifyTime"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_CG_VERIFY_TIME; }

	// get packet's max body size
	// message 의 최대 크기에 대한 설정이 필요하다.
	PacketSize_t getPacketMaxSize () const noexcept { return 0; }

};


#endif
