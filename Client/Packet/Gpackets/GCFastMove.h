//////////////////////////////////////////////////////////////////////
// 
// Filename    : GCFastMove.h 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __GC_FAST_MOVE_H__
#define __GC_FAST_MOVE_H__

// include files
#include "Packet.h"
#include "PacketFactory.h"


//////////////////////////////////////////////////////////////////////
//
// class GCFastMove;
//
// 게임 서버에서 특정 사용자가 움직였다는 정보를 클라이언트로 보내줄 
// 때 사용하는 패킷 객체이다. (ObjectID,X,Y,DIR) 을 포함한다.
//
//////////////////////////////////////////////////////////////////////

class GCFastMove : public Packet {

public :

	// constructor
	GCFastMove ()
	{
	}

	GCFastMove ( ObjectID_t objectID , Coord_t x , Coord_t y , Coord_t x2, Coord_t y2)
		: m_ObjectID(objectID), m_FromX(x), m_FromY(y), m_ToX(x2), m_ToY(y2)
	{
	}


public :
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_GC_FAST_MOVE; }
	
	// get packet's body size
	// *OPTIMIZATION HINT*
	// const static GCFastMovePacketSize 를 정의, 리턴하라.
	PacketSize_t getPacketSize () const noexcept { return szObjectID + 4*szCoord + szSkillType; }

	#ifdef __DEBUG_OUTPUT__
		// get packet's name
		std::string getPacketName () const { return "GCFastMove"; }
		
		// get packet's debug std::string
		std::string toString () const;
	#endif


public :

	// get/set Creature ID 
	ObjectID_t getObjectID () const noexcept { return m_ObjectID; }
	void setObjectID ( ObjectID_t objectID ) noexcept { m_ObjectID = objectID; }

	void setXY( Coord_t x, Coord_t y, Coord_t x2, Coord_t y2)
	{
		m_FromX = x; m_FromY = y; m_ToX = x2; m_ToY = y2;
	}

	// get
	Coord_t getFromX () const noexcept { return m_FromX; }
	Coord_t getFromY () const noexcept { return m_FromY; }
	Coord_t getToX () const noexcept { return m_ToX; }
	Coord_t getToY () const noexcept { return m_ToY; }
	
	void setSkillType( SkillType_t skillType ) { m_SkillType = skillType; }
	SkillType_t getSkillType() const { return m_SkillType; }


private :
	
	ObjectID_t m_ObjectID;		// 크리처 아이디
	Coord_t m_FromX,m_FromY, m_ToX, m_ToY;				// 출발좌표, 도착좌표 
	SkillType_t m_SkillType;	// 사용한 스킬 타입
};


//////////////////////////////////////////////////////////////////////
//
// class GCFastMoveFactory;
//
// Factory for GCFastMove
//
//////////////////////////////////////////////////////////////////////

class GCFastMoveFactory : public PacketFactory {

public :
	
	// create packet
	Packet * createPacket () { return new GCFastMove(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "GCFastMove"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_GC_FAST_MOVE; }

	// get packet's max body size
	// *OPTIMIZATION HINT*
	// const static GCFastMovePacketSize 를 정의, 리턴하라.
	PacketSize_t getPacketMaxSize () const noexcept { return szObjectID + 4*szCoord + szSkillType; }

};


//////////////////////////////////////////////////////////////////////
//
// class GCFastMoveHandler;
//
//////////////////////////////////////////////////////////////////////

class GCFastMoveHandler {

public :

	// execute packet's handler
	static void execute ( GCFastMove * pPacket , Player * pPlayer );

};

#endif
