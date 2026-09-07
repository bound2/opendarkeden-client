//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGThrowItem.h 
// Written By  : elca@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __CG_THROW_ITEM_H__
#define __CG_THROW_ITEM_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class CGThrowItem;
//
//////////////////////////////////////////////////////////////////////

class CGThrowItem : public Packet {

public :
	
	// constructor
	CGThrowItem ();
	
	// destructor
	~CGThrowItem ();

	
public :
	
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_CG_THROW_ITEM; }
	
	// get packet's body size
	PacketSize_t getPacketSize () const noexcept { return szObjectID + szObjectID + szCoordInven + szCoordInven; }

	// get/set ItemObjectID
	ObjectID_t getObjectID() const noexcept  { return m_ObjectID; }
	void setObjectID( ObjectID_t ObjectID ) noexcept { m_ObjectID = ObjectID; }

	// get / set TargetObjectID
	ObjectID_t getTargetObjectID() const noexcept  { return m_TargetObjectID; }
	void setTargetObjectID( ObjectID_t TargetObjectID ) noexcept { m_TargetObjectID = TargetObjectID; }

	// get/set InvenX
	CoordInven_t getX() const noexcept { return m_InvenX; }
	void setX( CoordInven_t InvenX ) noexcept { m_InvenX = InvenX; }

	// get/set InvenY
	CoordInven_t getY() const noexcept { return m_InvenY; }
	void setY( CoordInven_t InvenY ) noexcept { m_InvenY = InvenY; }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "CGThrowItem"; }

		// get packet's debug std::string
		std::string toString () const;
	#endif


private :

	// Item Object ID
	ObjectID_t m_ObjectID;

	// TargetObjectID
	ObjectID_t m_TargetObjectID;

	// InvenX / Y
	CoordInven_t m_InvenX;
	CoordInven_t m_InvenY;
	
};


//////////////////////////////////////////////////////////////////////
//
// class CGThrowItemFactory;
//
// Factory for CGThrowItem
//
//////////////////////////////////////////////////////////////////////
class CGThrowItemFactory : public PacketFactory {

public :
	
	// constructor
	CGThrowItemFactory () {}
	
	// destructor
	virtual ~CGThrowItemFactory () {}

	
public :
	
	// create packet
	Packet * createPacket () { return new CGThrowItem(); }

#ifdef __DEBUG_OUTPUT__
	// get packet name
	std::string getPacketName () const { return "CGThrowItem"; }
#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_CG_THROW_ITEM; }

	// get Packet Max Size
	PacketSize_t getPacketMaxSize () const noexcept { return szObjectID + szObjectID + szCoordInven + szCoordInven; }
};


//////////////////////////////////////////////////////////////////////
//
// class CGThrowItemHandler;
//
//////////////////////////////////////////////////////////////////////
#ifndef __GAME_CLIENT__
	class CGThrowItemHandler {

	public :

		// execute packet's handler
		static void execute ( CGThrowItem * pCGThrowItem , Player * pPlayer );

	};
#endif

#endif
