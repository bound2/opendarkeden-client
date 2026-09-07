
//////////////////////////////////////////////////////////////////////////////
// Filename    : GCRemoveStoreItem.h 
// Written By  : excel96
// Description : 
//////////////////////////////////////////////////////////////////////////////

#ifndef __GC_REMOVE_STORE_ITEM_H__
#define __GC_REMOVE_STORE_ITEM_H__

#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////////////
// class GCRemoveStoreItem;
//////////////////////////////////////////////////////////////////////////////

class GCRemoveStoreItem : public Packet
{
public:
	GCRemoveStoreItem() { }
	virtual ~GCRemoveStoreItem();

public:
	void read(SocketInputStream & iStream);
	void write(SocketOutputStream & oStream) const;
	PacketID_t getPacketID() const noexcept { return PACKET_GC_REMOVE_STORE_ITEM; }
	PacketSize_t getPacketSize() const noexcept { return szObjectID + szBYTE; }
#ifdef __DEBUG_OUTPUT__	
	string getPacketName() const { return "GCRemoveStoreItem"; }
	string toString() const;
#endif
	ObjectID_t	getOwnerObjectID() const { return m_OwnerObjectID; }
	void		setOwnerObjectID(ObjectID_t oid) { m_OwnerObjectID = oid; }

	BYTE		getIndex() const { return m_Index; }
	void		setIndex(BYTE index) { m_Index = index; }

private:
	ObjectID_t	m_OwnerObjectID;
	BYTE		m_Index;
};

//////////////////////////////////////////////////////////////////////////////
// class GCRemoveStoreItemFactory;
//////////////////////////////////////////////////////////////////////////////

class GCRemoveStoreItemFactory : public PacketFactory 
{
public:
	Packet* createPacket() { return new GCRemoveStoreItem(); }
#ifdef __DEBUG_OUTPUT__
	string getPacketName() const { return "GCRemoveStoreItem"; }
#endif
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_GC_REMOVE_STORE_ITEM; }
	PacketSize_t getPacketMaxSize() const
	{
		return szObjectID + szBYTE;
	}
};

//////////////////////////////////////////////////////////////////////////////
// class GCRemoveStoreItemHandler;
//////////////////////////////////////////////////////////////////////////////

class GCRemoveStoreItemHandler 
{
public:
	static void execute(GCRemoveStoreItem* pPacket, Player* pPlayer);
};

#endif
