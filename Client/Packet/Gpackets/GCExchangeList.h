//////////////////////////////////////////////////////////////////////////////
// Filename    : GCExchangeList.h
// Written By  : Exchange System
// Description : Server sends listing list to client
//////////////////////////////////////////////////////////////////////////////

#ifndef __GC_EXCHANGE_LIST_H__
#define __GC_EXCHANGE_LIST_H__

#include <string>
#include <vector>

#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////////////
// ExchangeListing structure (partial, for packet transfer)
//
// Field names and wire widths mirror the server repo's ExchangeListing in
// src/Core/GCExchangeList.h. Multibyte values are little-endian raw; every
// string travels as a BYTE length prefix followed by that many raw bytes.
//////////////////////////////////////////////////////////////////////////////

struct ExchangeListing
{
	ExchangeListing()
		: listingID(0), serverID(0), sellerRace(0), itemClass(0), itemType(0),
		  itemID(0), objectID(0), pricePoint(0), currency(0), status(0),
		  taxRate(0), taxAmount(0), version(0), enchantLevel(0), grade(0),
		  durability(0), silver(0), optionType1(0), optionType2(0), optionType3(0),
		  optionValue1(0), optionValue2(0), optionValue3(0), stackCount(0)
	{
	}

	int64_t		listingID;		// u64 on the wire
	int16_t		serverID;		// u16 on the wire
	std::string	sellerAccount;
	std::string	sellerPlayer;
	uint8_t		sellerRace;
	uint8_t		itemClass;
	uint16_t	itemType;
	int64_t		itemID;			// u64 on the wire
	int			objectID;
	int			pricePoint;
	uint8_t		currency;
	uint8_t		status;
	std::string	buyerAccount;
	std::string	buyerPlayer;
	uint8_t		taxRate;
	int			taxAmount;
	std::string	createdAt;
	std::string	expireAt;
	int			version;

	// Snapshot fields for UI display
	std::string	itemName;
	uint8_t		enchantLevel;
	uint16_t	grade;
	int			durability;
	uint16_t	silver;
	uint8_t		optionType1;
	uint8_t		optionType2;
	uint8_t		optionType3;
	uint16_t	optionValue1;
	uint16_t	optionValue2;
	uint16_t	optionValue3;
	int			stackCount;
};

//-----------------------------------------------------------------------------
// GCExchangeList
//-----------------------------------------------------------------------------

class GCExchangeList : public Packet
{
public:
	// Maximum wire length, in bytes, of EACH per-listing string: sellerAccount,
	// sellerPlayer, buyerAccount, buyerPlayer, createdAt, expireAt and itemName.
	// write(), read() and getPacketSize() all clamp to this value, and the
	// factory's max size is derived from it. MUST stay equal to the server
	// repo's constant of the same name.
	static const PacketSize_t kMaxListingString = 255;

	// Maximum number of listings one packet may carry, and the 20 in the
	// factory's max size formula. read() rejects a count above this value, and
	// the server clamps its page size to it before filling the listing vector,
	// which is what keeps getPacketSize() at or below getPacketMaxSize().
	// MUST stay equal to the server repo's constant of the same name.
	static const PacketSize_t kMaxListingsPerPage = 20;

	GCExchangeList();
	virtual ~GCExchangeList();

	void read(SocketInputStream & iStream);
	void write(SocketOutputStream & oStream) const;

	PacketSize_t getPacketSize() const;
	PacketID_t getPacketID() const noexcept { return PACKET_GC_EXCHANGE_LIST; }
	string getPacketName() const { return "GCExchangeList"; }
	string toString() const;

	// Getters
	int getPage() const { return m_Page; }
	int getPageSize() const { return m_PageSize; }
	int getTotal() const { return m_Total; }
	const std::vector<ExchangeListing>& getListings() const { return m_Listings; }

	// Setters
	void setPage(int page) { m_Page = page; }
	void setPageSize(int pageSize) { m_PageSize = pageSize; }
	void setTotal(int total) { m_Total = total; }
	void setListings(const std::vector<ExchangeListing>& listings) { m_Listings = listings; }
	void addListing(const ExchangeListing& listing) { m_Listings.push_back(listing); }

private:
	int		m_Page;
	int		m_PageSize;
	int		m_Total;
	std::vector<ExchangeListing>	m_Listings;
};

//-----------------------------------------------------------------------------
// GCExchangeListFactory
//-----------------------------------------------------------------------------

class GCExchangeListFactory : public PacketFactory
{
public:
	Packet* createPacket() { return new GCExchangeList(); }
	string getPacketName() const { return "GCExchangeList"; }
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_GC_EXCHANGE_LIST; }

	// Must equal the server repo's GCExchangeListFactory::getPacketMaxSize().
	// Header : int32 page + int32 pageSize + int32 total + uint16 count = 14,
	// then kMaxListingsPerPage listings with every string at its cap. write()
	// clamps each string to kMaxListingString and read() refuses a count above
	// kMaxListingsPerPage, so getPacketSize() can never exceed this.
	// 14 + 20 * 1855 = 37114
	PacketSize_t getPacketMaxSize() const
	{
		// One string at its cap : length byte + body = 256
		const PacketSize_t kMaxString = 1 + GCExchangeList::kMaxListingString;
		const PacketSize_t kMaxListing =
			  8 + 2					// listingID, serverID
			+ kMaxString * 2		// sellerAccount, sellerPlayer
			+ 1 + 1 + 2 + 8			// sellerRace, itemClass, itemType, itemID
			+ 4 + 4 + 1 + 1			// objectID, pricePoint, currency, status
			+ kMaxString * 2		// buyerAccount, buyerPlayer
			+ 1 + 4					// taxRate, taxAmount
			+ kMaxString * 2		// createdAt, expireAt
			+ 4						// version
			+ kMaxString			// itemName
			+ 1 + 2 + 4 + 2			// enchantLevel, grade, durability, silver
			+ 1 + 1 + 1				// optionType1..3
			+ 2 + 2 + 2				// optionValue1..3
			+ 4;					// stackCount
									// = 1855
		return 4 + 4 + 4 + 2 + kMaxListing * GCExchangeList::kMaxListingsPerPage;
	}
};

#endif // __GC_EXCHANGE_LIST_H__
