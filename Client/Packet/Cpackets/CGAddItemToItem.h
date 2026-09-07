//////////////////////////////////////////////////////////////////////////////
// Filename    : CGAddItemToItem.h 
// Written By  : elca@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////

#ifndef __CG_ADD_ITEM_TO_ITEM_H__
#define __CG_ADD_ITEM_TO_ITEM_H__

#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////////////
// class CGAddItemToItem;
//////////////////////////////////////////////////////////////////////////////

class CGAddItemToItem : public Packet 
{
public:
    void read(SocketInputStream & iStream);
    void write(SocketOutputStream & oStream) const;
	PacketID_t getPacketID() const noexcept { return PACKET_CG_ADD_ITEM_TO_ITEM; }
	PacketSize_t getPacketSize() const noexcept { return szObjectID + szCoordInven + szCoordInven; }

#ifdef __DEBUG_OUTPUT__
	std::string getPacketName() const { return "CGAddItemToItem"; }
	std::string toString() const;
#endif

public:
	ObjectID_t getObjectID() const noexcept { return m_ObjectID; }
	void setObjectID(ObjectID_t ObjectID) noexcept { m_ObjectID = ObjectID; }

	CoordInven_t getX() const noexcept { return m_X; }
	void setX(Coord_t X) noexcept { m_X = X; }

	CoordInven_t getY() const noexcept { return m_Y; }
	void setY(Coord_t Y) noexcept { m_Y = Y; }

private :
	ObjectID_t   m_ObjectID;	// ObjectID
	CoordInven_t m_X;			// Coord X
	CoordInven_t m_Y;			// Coord Y
};

//////////////////////////////////////////////////////////////////////////////
// class CGAddItemToItemFactory;
//////////////////////////////////////////////////////////////////////////////
class CGAddItemToItemFactory : public PacketFactory 
{
public:
	Packet* createPacket() { return new CGAddItemToItem(); }
	std::string getPacketName() const { return "CGAddItemToItem"; }
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_CG_ADD_ITEM_TO_ITEM; }
	PacketSize_t getPacketMaxSize() const noexcept { return szObjectID + szCoordInven + szCoordInven; }
};

#ifndef __GAME_CLIENT__
//////////////////////////////////////////////////////////////////////////////
// class CGAddItemToItemHandler;
//////////////////////////////////////////////////////////////////////////////

class CGAddItemToItemHandler 
{
public:
	static void execute(CGAddItemToItem* pCGAddItemToItem, Player* pPlayer);
};
#endif

#endif
