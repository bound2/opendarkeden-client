//////////////////////////////////////////////////////////////////////
// 
// Filename    : GCMove.h 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __GC_MOVE_H__
#define __GC_MOVE_H__

// include files
#include "Packet.h"
#include "PacketFactory.h"


//////////////////////////////////////////////////////////////////////
//
// class GCMove;
//
// 게임 서버에서 특정 사용자가 움직였다는 정보를 클라이언트로 보내줄 
// 때 사용하는 패킷 객체이다. (ObjectID,X,Y,DIR) 을 포함한다.
//
//////////////////////////////////////////////////////////////////////

class GCMove : public Packet {

public :

	// constructor
	GCMove ()
	{
	}

	GCMove ( ObjectID_t objectID , Coord_t x , Coord_t y , Dir_t dir )
		: m_ObjectID(objectID), m_X(x), m_Y(y), m_Dir(dir)
	{
	}


public :
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_GC_MOVE; }
	
	// get packet's body size
	// *OPTIMIZATION HINT*
	// const static GCMovePacketSize 를 정의, 리턴하라.
	PacketSize_t getPacketSize () const noexcept { return szObjectID + szCoord + szCoord + szDir ; }

	#ifdef __DEBUG_OUTPUT__
		// get packet's name
		std::string getPacketName () const { return "GCMove"; }
		
		// get packet's debug std::string
		std::string toString () const;
	#endif


public :

	// get/set Creature ID 
	ObjectID_t getObjectID () const noexcept { return m_ObjectID; }
	void setObjectID ( ObjectID_t objectID ) noexcept { m_ObjectID = objectID; }

	// get/set X
	Coord_t getX () const noexcept { return m_X; }
	void setX ( Coord_t x ) noexcept { m_X = x; }
	
	// get/set Y
	Coord_t getY () const noexcept { return m_Y; }
	void setY ( Coord_t y ) noexcept { m_Y = y; }

	// get/set Dir
	Dir_t getDir () const noexcept { return m_Dir; }
	void setDir ( Dir_t dir ) noexcept { m_Dir = dir; }


private :
	
	ObjectID_t m_ObjectID;		// 크리처 아이디
	Coord_t m_X;				// X 좌표
	Coord_t m_Y;				// Y 좌표
	Dir_t m_Dir;				// 방향

};


//////////////////////////////////////////////////////////////////////
//
// class GCMoveFactory;
//
// Factory for GCMove
//
//////////////////////////////////////////////////////////////////////

class GCMoveFactory : public PacketFactory {

public :
	
	// create packet
	Packet * createPacket () { return new GCMove(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "GCMove"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_GC_MOVE; }

	// get packet's max body size
	// *OPTIMIZATION HINT*
	// const static GCMovePacketSize 를 정의, 리턴하라.
	PacketSize_t getPacketMaxSize () const noexcept { return szObjectID + szCoord + szCoord + szDir ; }

};


//////////////////////////////////////////////////////////////////////
//
// class GCMoveHandler;
//
//////////////////////////////////////////////////////////////////////

class GCMoveHandler {

public :

	// execute packet's handler
	static void execute ( GCMove * pPacket , Player * pPlayer );

};

#endif
