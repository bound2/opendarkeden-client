//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGAddMouseToQuickSlot.h 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __CG_ADD_MOUSE_TO_QUICKSLOT_H__
#define __CG_ADD_MOUSE_TO_QUICKSLOT_H__

// include files
#include "Packet.h"
#include "PacketFactory.h"


//////////////////////////////////////////////////////////////////////
//
// class CGAddMouseToQuickSlot;
//
//////////////////////////////////////////////////////////////////////

class CGAddMouseToQuickSlot : public Packet {
public :

	// constructor
	CGAddMouseToQuickSlot();

	// destructor
	~CGAddMouseToQuickSlot();

public :
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_CG_ADD_MOUSE_TO_QUICKSLOT; }
	
	// get packet's body size
	// *OPTIMIZATION HINT*
	// const static CGAddMouseToQuickSlotPacketSize 를 정의해서 리턴하라.
	PacketSize_t getPacketSize () const noexcept { return szObjectID + szSlotID; }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "CGAddMouseToQuickSlot"; }
	
		// get packet's debug std::string
		std::string toString () const;
	#endif
	
public :

	// get / set ObjectID
	ObjectID_t getObjectID() const noexcept { return m_ObjectID; }
	void setObjectID( ObjectID_t ObjectID ) noexcept { m_ObjectID = ObjectID; }

	// get / set QuickSlotID
	SlotID_t getSlotID() const noexcept { return m_SlotID; }
	void setSlotID( SlotID_t SlotID ) noexcept { m_SlotID = SlotID; }


private :
	
	// ObjectID
	ObjectID_t m_ObjectID;

	// QuickSlot의 ID
	SlotID_t m_SlotID;

};


//////////////////////////////////////////////////////////////////////
//
// class CGAddMouseToQuickSlotFactory;
//
// Factory for CGAddMouseToQuickSlot
//
//////////////////////////////////////////////////////////////////////
class CGAddMouseToQuickSlotFactory : public PacketFactory {

public :
	
	// create packet
	Packet * createPacket () { return new CGAddMouseToQuickSlot(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "CGAddMouseToQuickSlot"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_CG_ADD_MOUSE_TO_QUICKSLOT; }

	// get packet's max body size
	// *OPTIMIZATION HINT*
	// const static CGAddMouseToQuickSlotPacketSize 를 정의해서 리턴하라.
	PacketSize_t getPacketMaxSize () const noexcept { return szObjectID + szSlotID; }

};


//////////////////////////////////////////////////////////////////////
//
// class CGAddMouseToQuickSlotHandler;
//
//////////////////////////////////////////////////////////////////////
#ifndef __GAME_CLIENT__
	class CGAddMouseToQuickSlotHandler {
		
	public :

		// execute packet's handler
		static void execute ( CGAddMouseToQuickSlot * pPacket , Player * player );
	};

#endif
#endif
