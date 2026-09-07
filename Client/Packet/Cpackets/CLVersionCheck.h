//--------------------------------------------------------------------------------
// 
// Filename    : CLVersionCheck.h 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//--------------------------------------------------------------------------------

#ifndef __CL_VERSION_CHECK_H__
#define __CL_VERSION_CHECK_H__

// include files
#include "Packet.h"
#include "PacketFactory.h"

//--------------------------------------------------------------------------------
//
// class CLVersionCheck;
//
// 클라이언트가 로그인 서버에게 최초에 전송하는 패킷이다.
// 아이디와 패스워드가 암호화되어 있다.
//
//--------------------------------------------------------------------------------

class CLVersionCheck : public Packet {

public :
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_CL_VERSION_CHECK; }
	
	// get packet's body size
	PacketSize_t getPacketSize () const noexcept { return szDWORD; }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "CLVersionCheck"; }
		
		// get packet's debug string
		std::string toString () const;
	#endif

public :

	// get/set Client Version
	DWORD getVersion() const noexcept { return m_Version; }
	void setVersion( DWORD Version ) noexcept { m_Version = Version; }

private :

	// 클라이언트 버젼
	DWORD m_Version;

};


//--------------------------------------------------------------------------------
//
// class CLVersionCheckFactory;
//
// Factory for CLVersionCheck
//
//--------------------------------------------------------------------------------
class CLVersionCheckFactory : public PacketFactory {

public :
	
	// create packet
	Packet * createPacket () { return new CLVersionCheck(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "CLVersionCheck"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_CL_VERSION_CHECK; }

	// get packet's max body size
	PacketSize_t getPacketMaxSize () const noexcept { return szDWORD; }

};


//--------------------------------------------------------------------------------
//
// class CLVersionCheckHandler;
//
//--------------------------------------------------------------------------------
#ifndef __GAME_CLIENT__
	class CLVersionCheckHandler {

	public :

		// execute packet's handler
		static void execute ( CLVersionCheck * pPacket , Player * pPlayer );

	};
#endif

#endif
