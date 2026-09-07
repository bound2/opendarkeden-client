//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGUnburrow.h 
// Written By  : crazydog
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __CG_UNBURROW_H__
#define __CG_UNBURROW_H__

// include files
#include "Packet.h"
#include "PacketFactory.h"


//////////////////////////////////////////////////////////////////////
//
// class CGUnburrow;
//
//////////////////////////////////////////////////////////////////////

class CGUnburrow : public Packet {

public :
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_CG_UNBURROW; }
	
	// get packet's body size
	// *OPTIMIZATION HINT*
	// const static CGUnburrowPacketSize 를 정의해서 리턴하라.
	PacketSize_t getPacketSize () const noexcept { return szCoord + szCoord + szDir; }

	#ifndef __GAME_CLIENT__
		// get packet name (required when not GAME_CLIENT)
		std::string getPacketName () const { return "CGUnburrow"; }

		// get packet's debug string (required when not GAME_CLIENT)
		std::string toString () const { return "CGUnburrow"; }
	#endif

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "CGUnburrow"; }
		
		// get packet's debug std::string
		std::string toString () const;
	#endif	
	
public :

	// get/set X Coordicate
	Coord_t getX () const noexcept { return m_X; }
	void setX ( Coord_t x ) noexcept { m_X = x; }

	// get/set Y Coordicate
	Coord_t getY () const noexcept { return m_Y; }
	void setY ( Coord_t y ) noexcept { m_Y = y; }

	// get/set Direction
	Dir_t getDir () const noexcept { return m_Dir; }
	void setDir ( Dir_t dir ) noexcept { m_Dir = dir; }
	
private :
	
	Coord_t m_X;			// X 좌표
	Coord_t m_Y;			// Y 좌표
	Dir_t m_Dir;			// 방향

};


//////////////////////////////////////////////////////////////////////
//
// class CGUnburrowFactory;
//
// Factory for CGUnburrow
//
//////////////////////////////////////////////////////////////////////
class CGUnburrowFactory : public PacketFactory {

public :
	
	// create packet
	Packet * createPacket () { return new CGUnburrow(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "CGUnburrow"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_CG_UNBURROW; }

	// get packet's max body size
	// *OPTIMIZATION HINT*
	// const static CGUnburrowPacketSize 를 정의해서 리턴하라.
	PacketSize_t getPacketMaxSize () const noexcept { return szCoord + szCoord + szDir; }

};


//////////////////////////////////////////////////////////////////////
//
// class CGUnburrowHandler;
//
//////////////////////////////////////////////////////////////////////
#ifndef __GAME_CLIENT__
	class CGUnburrowHandler {
		
	public :

		// execute packet's handler
		static void execute ( CGUnburrow * pPacket , Player * player );
	};
#endif

#endif
