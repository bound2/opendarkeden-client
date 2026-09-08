//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGAttack 
// Written By  : elca@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __CG_ATTACK_H__
#define __CG_ATTACK_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"


//////////////////////////////////////////////////////////////////////
//
// class CGAttack;
//
//////////////////////////////////////////////////////////////////////

class CGAttack : public Packet {

public :
	
	// constructor
	CGAttack ();
	
	// destructor
	~CGAttack ();

	
public :
	
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_CG_ATTACK; }
	
	// get packet's body size
	PacketSize_t getPacketSize () const noexcept { return szObjectID + szCoord + szCoord + szDir; }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "CGAttack"; }
		
		// get packet's debug std::string
		std::string toString () const;
	#endif
	
	// get/set X Coordicate
	Coord_t getX () const noexcept { return m_X; }
	void setX ( Coord_t x ) noexcept { m_X = x; }

	// get/set Y Coordicate
	Coord_t getY () const noexcept { return m_Y; }
	void setY ( Coord_t y ) noexcept { m_Y = y; }

	// get/set Direction
	Dir_t getDir () const noexcept { return m_Dir; }
	void setDir ( Dir_t dir ) noexcept { m_Dir = dir; }
	
	// get/set ObjectID
	ObjectID_t getObjectID() const noexcept { return m_ObjectID; }
	void setObjectID (ObjectID_t ObjectID) noexcept { m_ObjectID = ObjectID; }
	
private :
	
	ObjectID_t m_ObjectID;  // ObjectID
	Coord_t m_X;			// X 좌표
	Coord_t m_Y;			// Y 좌표
	Dir_t m_Dir;			// 방향

};


//////////////////////////////////////////////////////////////////////
//
// class CGAttackFactory;
//
// Factory for CGAttack
//
//////////////////////////////////////////////////////////////////////
class CGAttackFactory : public PacketFactory {

public :
	
	// constructor
	CGAttackFactory () {}
	
	// destructor
	virtual ~CGAttackFactory () {}

	
public :
	
	// create packet
	Packet * createPacket () { return new CGAttack(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "CGAttack"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_CG_ATTACK; }

	// get Packet Max Size
	PacketSize_t getPacketMaxSize() const noexcept { return szObjectID + szCoord + szCoord + szDir; }

};

//////////////////////////////////////////////////////////////////////
//
// class CGAttackHandler;
//
//////////////////////////////////////////////////////////////////////
#ifndef __GAME_CLIENT__
	class CGAttackHandler {
		
	public :

		// execute packet's handler
		static void execute ( CGAttack * pCGAttack , Player * player );
	};

#endif
#endif
