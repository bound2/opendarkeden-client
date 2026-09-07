//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGRideMotorCycle.h 
// Written By  : elca@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __CG_RIDE_MOTORCYCLE_H__
#define __CG_RIDE_MOTORCYCLE_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class CGRideMotorCycle;
//
//////////////////////////////////////////////////////////////////////

class CGRideMotorCycle : public Packet {

public :
	
	// constructor
	CGRideMotorCycle ();
	
	// destructor
	~CGRideMotorCycle ();

	
public :
	
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_CG_RIDE_MOTORCYCLE; }
	
	// get packet's body size
	PacketSize_t getPacketSize () const noexcept { return szObjectID + szCoord + szCoord ; }

	// get/set ObjectID
	ObjectID_t getObjectID() const noexcept  { return m_ObjectID; }
	void setObjectID( ObjectID_t ObjectID ) noexcept { m_ObjectID = ObjectID; }

	// get/set X
	Coord_t getX() const noexcept { return m_X; }
	void setX( Coord_t X ) noexcept { m_X = X; }

	// get/set Y
	Coord_t getY() const noexcept { return m_Y; }
	void setY( Coord_t Y ) noexcept { m_Y = Y; }
	
	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "CGRideMotorCycle"; }

		// get packet's debug std::string
		std::string toString () const;
	#endif

private :

	// ObjectID
	ObjectID_t m_ObjectID;

	// Coord X
	Coord_t m_X;

	// Coord Y
	Coord_t m_Y;
	
};


//////////////////////////////////////////////////////////////////////
//
// class CGRideMotorCycleFactory;
//
// Factory for CGRideMotorCycle
//
//////////////////////////////////////////////////////////////////////
class CGRideMotorCycleFactory : public PacketFactory {

public :
	
	// constructor
	CGRideMotorCycleFactory () {}
	
	// destructor
	virtual ~CGRideMotorCycleFactory () {}

	
public :
	
	// create packet
	Packet * createPacket () { return new CGRideMotorCycle(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "CGRideMotorCycle"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_CG_RIDE_MOTORCYCLE; }

	// get Packet Max Size
	PacketSize_t getPacketMaxSize() const noexcept { return szObjectID + szCoord + szCoord; }

};


//////////////////////////////////////////////////////////////////////
//
// class CGRideMotorCycleHandler;
//
//////////////////////////////////////////////////////////////////////
#ifndef __GAME_CLIENT__
	class CGRideMotorCycleHandler {

	public :

		// execute packet's handler
		static void execute ( CGRideMotorCycle * pCGRideMotorCycle , Player * pPlayer );

	};
#endif

#endif
