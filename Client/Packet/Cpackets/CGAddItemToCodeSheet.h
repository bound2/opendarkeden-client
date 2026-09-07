//////////////////////////////////////////////////////////////////////////////
// Filename    : CGAddItemToCodeSheet.h 
// Written By  : elca@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////

#ifndef __CG_ADD_ITEM_TO_CODE_SHEET_H__
#define __CG_ADD_ITEM_TO_CODE_SHEET_H__

#include "Packet.h"
#include "PacketFactory.h"


//////////////////////////////////////////////////////////////////////////////
// class CGAddItemToCodeSheet;
//////////////////////////////////////////////////////////////////////////////

class CGAddItemToCodeSheet : public Packet 
{
public:
    void read(SocketInputStream & iStream);
    void write(SocketOutputStream & oStream) const;
	PacketID_t getPacketID() const noexcept { return PACKET_CG_ADD_ITEM_TO_CODE_SHEET; }
	PacketSize_t getPacketSize() const noexcept { return szObjectID + szCoordInven + szCoordInven; }

#ifdef __DEBUG_OUTPUT__
	std::string getPacketName() const { return "CGAddItemToCodeSheet"; }
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
// class CGAddItemToCodeSheetFactory;
//////////////////////////////////////////////////////////////////////////////

class CGAddItemToCodeSheetFactory : public PacketFactory 
{
public:
	Packet* createPacket() { return new CGAddItemToCodeSheet(); }
	string getPacketName() const { return "CGAddItemToCodeSheet"; }
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_CG_ADD_ITEM_TO_CODE_SHEET; }
	PacketSize_t getPacketMaxSize() const noexcept { return szObjectID + szCoordInven + szCoordInven; }
};

//////////////////////////////////////////////////////////////////////////////
// class CGAddItemToCodeSheetHandler;
//////////////////////////////////////////////////////////////////////////////

class CGAddItemToCodeSheetHandler 
{
public:
	static void execute(CGAddItemToCodeSheet* pCGAddItemToCodeSheet, Player* pPlayer);

};

#endif
