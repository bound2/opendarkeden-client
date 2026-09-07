//////////////////////////////////////////////////////////////////////
// 
// Filename    :  GCMoveOK.h 
// Written By  :  Elca
// 
//////////////////////////////////////////////////////////////////////

#ifndef __GC_MOVE_OK_H__
#define __GC_MOVE_OK_H__

// include files
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class  GCMoveOK;
//
// 게임 서버에서 특정 사용자가 움직였다는 정보를 클라이언트로 보내줄 
// 때 사용하는 패킷 객체이다. (CreatureID,X,Y,DIR) 을 포함한다.
//
//////////////////////////////////////////////////////////////////////

class GCMoveOK : public Packet {

public :

	// constructor
	GCMoveOK () {}
	GCMoveOK ( Coord_t x , Coord_t y , Dir_t dir ) : m_X(x), m_Y(y), m_Dir(dir) {}


public :
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_GC_MOVE_OK; }
	
	// get packet body size
	// *OPTIMIZATION HINT*
	// const static GCMoveOKPacketSize 를 정의, 리턴하라.
	PacketSize_t getPacketSize () const noexcept { return szCoord + szCoord + szDir; }
	
	#ifdef __DEBUG_OUTPUT__
		// get packet's name
		std::string getPacketName () const { return "GCMoveOK"; }
		
		// get packet's debug std::string
		std::string toString () const;
	#endif
	

public :

	// get/set X
	Coord_t getX() const noexcept { return m_X; }
	void setX( Coord_t x) noexcept { m_X = x; }
	
	// get/set Y
	Coord_t getY() const noexcept { return m_Y; }
	void setY( Coord_t y) noexcept { m_Y = y ; }

	// get/set Dir
	Dir_t getDir() const noexcept { return m_Dir; }
	void setDir( Dir_t dir) noexcept { m_Dir = dir; }
	

private : 

	Coord_t m_X;   // 목표 X 좌표
	Coord_t m_Y;   // 목표 Y 좌표
	Dir_t m_Dir;   // 목표 방향

};


//////////////////////////////////////////////////////////////////////
//
// class GCMoveOKFactory;
//
// Factory for GCMoveOK
//
//////////////////////////////////////////////////////////////////////

class  GCMoveOKFactory : public PacketFactory {

public :
	
	// create packet
	Packet * createPacket () { return new GCMoveOK(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "GCMoveOK"; }
	#endif

	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_GC_MOVE_OK; }

	// get packet's max body size
	// *OPTIMIZATION HINT*
	// const static GCMoveOKPacketSize 를 정의, 리턴하라.
	PacketSize_t getPacketMaxSize () const noexcept { return szCoord + szCoord + szDir; }
	
};


//////////////////////////////////////////////////////////////////////
//
// class  GCMoveOKHandler;
//
//////////////////////////////////////////////////////////////////////

class  GCMoveOKHandler {

public :

	// execute packet's handler
	static void execute (  GCMoveOK * pPacket , Player * pPlayer );

};

#endif
