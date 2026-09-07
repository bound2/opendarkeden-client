//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGAddInventoryToMouse.h 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __CG_ADD_INVENTORY_TO_MOUSE_H__
#define __CG_ADD_INVENTORY_TO_MOUSE_H__

// include files
#include "Packet.h"
#include "PacketFactory.h"


//////////////////////////////////////////////////////////////////////
//
// class CGAddInventoryToMouse;
//
//////////////////////////////////////////////////////////////////////

class CGAddInventoryToMouse : public Packet {
public :

	// constructor
	CGAddInventoryToMouse();

	// destructor
	~CGAddInventoryToMouse();

public :
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_CG_ADD_INVENTORY_TO_MOUSE; }
	
	// get packet's body size
	// *OPTIMIZATION HINT*
	// const static CGAddInventoryToMousePacketSize 를 정의해서 리턴하라.
	//modify by viva
	//PacketSize_t getPacketSize () const throw () { return szObjectID + szObjectID + szCoordInven + szCoordInven; }
	PacketSize_t getPacketSize () const noexcept { return szObjectID + szCoordInven + szCoordInven; }
	//end
	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "CGAddInventoryToMouse"; }
		
		// get packet's debug std::string
		std::string toString () const;
	#endif
	
public :

	// get / set ObjectID
	ObjectID_t getObjectID() noexcept { return m_ObjectID; }
	void setObjectID( ObjectID_t ObjectID ) noexcept { m_ObjectID = ObjectID; }

	ObjectID_t getInventoryItemObjectID() noexcept { return m_InventoryItemObjectID; }
	void setInventoryItemObjectID(ObjectID_t InventoryItemObjectID) noexcept { m_InventoryItemObjectID = InventoryItemObjectID; }

	// get / set Ivnentory X Coordicate
	CoordInven_t getX() const noexcept { return m_InvenX; }
	void setX( CoordInven_t InvenX ) noexcept { m_InvenX = InvenX; }

	// get / set Inventory Y Coordicate
	CoordInven_t getY() const noexcept { return m_InvenY; }
	void setY( CoordInven_t InvenY ) { m_InvenY = InvenY; }

private :
	
	// ObjectID
	ObjectID_t m_ObjectID;

	// 보조 인벤토리 아이템의 오브젝트 아이디. 0이면 메인 인벤토리에서 꺼냄
	ObjectID_t m_InventoryItemObjectID;

	// Inventory의 X, Y 좌표.
	CoordInven_t m_InvenX;
	CoordInven_t m_InvenY;

};


//////////////////////////////////////////////////////////////////////
//
// class CGAddInventoryToMouseFactory;
//
// Factory for CGAddInventoryToMouse
//
//////////////////////////////////////////////////////////////////////
class CGAddInventoryToMouseFactory : public PacketFactory {

public :
	
	// create packet
	Packet * createPacket () { return new CGAddInventoryToMouse(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "CGAddInventoryToMouse"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_CG_ADD_INVENTORY_TO_MOUSE; }

	// get packet's max body size
	// *OPTIMIZATION HINT*
	// const static CGAddInventoryToMousePacketSize 를 정의해서 리턴하라.
	//modify by viva
	//PacketSize_t getPacketMaxSize () const throw () { return szObjectID + szObjectID + szCoordInven + szCoordInven; }
	PacketSize_t getPacketMaxSize () const noexcept { return szObjectID + szCoordInven + szCoordInven; }
	//end
};


//////////////////////////////////////////////////////////////////////
//
// class CGAddInventoryToMouseHandler;
//
//////////////////////////////////////////////////////////////////////
#ifndef __GAME_CLIENT__
	class CGAddInventoryToMouseHandler {
		
	public :

		// execute packet's handler
		static void execute ( CGAddInventoryToMouse * pPacket , Player * player );
	};

#endif
#endif
