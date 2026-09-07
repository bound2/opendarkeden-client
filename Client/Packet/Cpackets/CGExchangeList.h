//////////////////////////////////////////////////////////////////////////////
// Filename    : CGExchangeList.h
// Written By  : Exchange System
// Description : Client requests listing list from server
//////////////////////////////////////////////////////////////////////////////

#ifndef __CG_EXCHANGE_LIST_H__
#define __CG_EXCHANGE_LIST_H__

#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////////////
// class CGExchangeList;
//////////////////////////////////////////////////////////////////////////////

class CGExchangeList : public Packet
{
public:
	// Maximum wire length, in bytes, of m_SellerFilter. write(), read() and
	// getPacketSize() all clamp to this value, and the factory's max size is
	// derived from it. MUST stay equal to the server repo's constant of the
	// same name.
	static const PacketSize_t kMaxSellerFilter = 255;

	CGExchangeList();
	virtual ~CGExchangeList();

	void read(SocketInputStream& iStream);
	void write(SocketOutputStream& oStream) const;

	// page + pageSize + minPrice + maxPrice (4 ints) + itemClass (BYTE)
	// + itemType (ushort) + sellerFilter length byte + clamped filter body.
	// The clamp is the same one write() applies, so this equals the number of
	// bytes write() emits for ANY value of m_SellerFilter.
	PacketSize_t getPacketSize() const
	{
		return szint * 4 + sizeof(BYTE) + sizeof(ushort)
			+ sizeof(BYTE)
			+ (PacketSize_t)(m_SellerFilter.length() > kMaxSellerFilter ? kMaxSellerFilter : m_SellerFilter.length());
	}
	PacketID_t getPacketID() const { return PACKET_CG_EXCHANGE_LIST; }
	string getPacketName() const { return "CGExchangeList"; }
	string toString() const;

	// Getters
	int getPage() const { return m_Page; }
	int getPageSize() const { return m_PageSize; }
	BYTE getItemClass() const { return m_ItemClass; }
	ushort getItemType() const { return m_ItemType; }
	int getMinPrice() const { return m_MinPrice; }
	int getMaxPrice() const { return m_MaxPrice; }
	const string& getSellerFilter() const { return m_SellerFilter; }

	// Setters
	void setPage(int page) { m_Page = page; }
	void setPageSize(int size) { m_PageSize = size; }
	void setItemClass(BYTE itemClass) { m_ItemClass = itemClass; }
	void setItemType(ushort itemType) { m_ItemType = itemType; }
	void setMinPrice(int minPrice) { m_MinPrice = minPrice; }
	void setMaxPrice(int maxPrice) { m_MaxPrice = maxPrice; }
	void setSellerFilter(const string& sellerFilter) { m_SellerFilter = sellerFilter; }

private:
	int		m_Page;
	int		m_PageSize;
	BYTE	m_ItemClass;	// 0xFF = all
	ushort	m_ItemType;		// 0xFFFF = all
	int		m_MinPrice;
	int		m_MaxPrice;
	string	m_SellerFilter;	// empty = no filter
};

//////////////////////////////////////////////////////////////////////////////
// class CGExchangeListFactory;
//////////////////////////////////////////////////////////////////////////////

class CGExchangeListFactory : public PacketFactory
{
public:
	Packet* createPacket() { return new CGExchangeList(); }
	string getPacketName() const { return "CGExchangeList"; }
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_CG_EXCHANGE_LIST; }
	// page(4) + pageSize(4) + itemClass(1) + itemType(2) + minPrice(4)
	// + maxPrice(4) + sellerFilter length byte(1) + sellerFilter body(255)
	// = 275. write() clamps the body to kMaxSellerFilter, so getPacketSize()
	// can never exceed this.
	// Must stay identical to the server repo's CGExchangeListFactory::getPacketMaxSize()
	PacketSize_t getPacketMaxSize() const noexcept { return 4 + 4 + 1 + 2 + 4 + 4 + 1 + CGExchangeList::kMaxSellerFilter; }
};

#endif
