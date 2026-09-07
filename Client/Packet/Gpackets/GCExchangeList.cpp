//////////////////////////////////////////////////////////////////////////////
// Filename    : GCExchangeList.cpp
// Written By  : Exchange System
// Description : Server sends listing list to client
//////////////////////////////////////////////////////////////////////////////

#include "GCExchangeList.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"

#include "Player.h"

#include <span>

// Out-of-line definitions so the constants may also be odr-used (bound to a
// reference) by the UI and tests, not only read as compile-time constants.
const PacketSize_t GCExchangeList::kMaxListingString;
const PacketSize_t GCExchangeList::kMaxListingsPerPage;

GCExchangeList::GCExchangeList()
{
	m_Page = 1;
	m_PageSize = 20;
	m_Total = 0;
}

GCExchangeList::~GCExchangeList()
{
}

void GCExchangeList::read(SocketInputStream & iStream)
{
	__BEGIN_TRY

	// Read page info
	iStream.read(m_Page);
	iStream.read(m_PageSize);
	iStream.read(m_Total);

	// Read listing count
	uint16_t count = 0;
	iStream.read(count);

	// This is the one packet whose body length is driven by a value inside the
	// body rather than by the packet header, so the "too large packet SIZE"
	// precheck that protects every other packet cannot bound it. An unbounded
	// count would make the loop below eat roughly 70 bytes per phantom listing
	// out of whatever follows on the connection - either silently swallowing
	// the next packets, which corrupts the stream permanently, or throwing
	// halfway through a listing. Refuse the packet instead. Nothing is
	// reserved from the untrusted count either; the vector grows as listings
	// actually arrive.
	if (count > kMaxListingsPerPage)
		throw InvalidProtocolException("too many exchange listings");

	// Read listings - this must stay an exact mirror of write() below and of
	// the server repo's GCExchangeList::write().
	m_Listings.clear();
	for (uint16_t i = 0; i < count; i++)
	{
		ExchangeListing listing;

		// The bounded view makes the 256-byte staging capacity part of every
		// string read. A BYTE length leaves room for the terminator at index
		// 255. Every string's else-branch stays so read() fully overwrites the
		// listing it fills, exactly as write() emits every field.
		char buf[256];
		std::span<char> buffer(buf);
		uint8_t len;

		// IDs are unsigned 64-bit values on the wire. The typed helper makes
		// that width and the protocol's little-endian requirement explicit.
		ulonglong listingID = 0;
		iStream.readWire(listingID);
		listing.listingID = (int64_t)listingID;

		ushort serverID = 0;
		iStream.read(serverID);
		listing.serverID = (int16_t)serverID;

		// SellerAccount
		iStream.read(len);
		if (len > 0)
		{
			iStream.read(buffer, len);
			buf[len] = '\0';
			listing.sellerAccount = buf;
		}
		else
		{
			listing.sellerAccount.clear();
		}

		// SellerPlayer
		iStream.read(len);
		if (len > 0)
		{
			iStream.read(buffer, len);
			buf[len] = '\0';
			listing.sellerPlayer = buf;
		}
		else
		{
			listing.sellerPlayer.clear();
		}

		iStream.read(listing.sellerRace);
		iStream.read(listing.itemClass);
		iStream.read(listing.itemType);

		ulonglong itemID = 0;
		iStream.readWire(itemID);
		listing.itemID = (int64_t)itemID;

		iStream.read(listing.objectID);
		iStream.read(listing.pricePoint);
		iStream.read(listing.currency);
		iStream.read(listing.status);

		// BuyerAccount
		iStream.read(len);
		if (len > 0)
		{
			iStream.read(buffer, len);
			buf[len] = '\0';
			listing.buyerAccount = buf;
		}
		else
		{
			listing.buyerAccount.clear();
		}

		// BuyerPlayer
		iStream.read(len);
		if (len > 0)
		{
			iStream.read(buffer, len);
			buf[len] = '\0';
			listing.buyerPlayer = buf;
		}
		else
		{
			listing.buyerPlayer.clear();
		}

		iStream.read(listing.taxRate);
		iStream.read(listing.taxAmount);

		// Timestamp strings (soldAt/cancelledAt/updatedAt are not on the wire)
		iStream.read(len);
		if (len > 0)
		{
			iStream.read(buffer, len);
			buf[len] = '\0';
			listing.createdAt = buf;
		}
		else
		{
			listing.createdAt.clear();
		}

		iStream.read(len);
		if (len > 0)
		{
			iStream.read(buffer, len);
			buf[len] = '\0';
			listing.expireAt = buf;
		}
		else
		{
			listing.expireAt.clear();
		}

		iStream.read(listing.version);

		// Snapshot fields
		// ItemName
		iStream.read(len);
		if (len > 0)
		{
			iStream.read(buffer, len);
			buf[len] = '\0';
			listing.itemName = buf;
		}
		else
		{
			listing.itemName.clear();
		}

		iStream.read(listing.enchantLevel);
		iStream.read(listing.grade);
		iStream.read(listing.durability);
		iStream.read(listing.silver);
		iStream.read(listing.optionType1);
		iStream.read(listing.optionType2);
		iStream.read(listing.optionType3);
		iStream.read(listing.optionValue1);
		iStream.read(listing.optionValue2);
		iStream.read(listing.optionValue3);
		iStream.read(listing.stackCount);

		m_Listings.push_back(listing);
	}

	__END_CATCH
}

void GCExchangeList::write(SocketOutputStream & oStream) const
{
	__BEGIN_TRY

	// Write page info
	oStream.write(m_Page);
	oStream.write(m_PageSize);
	oStream.write(m_Total);

	// Write listing count
	uint16_t count = (uint16_t)m_Listings.size();
	oStream.write(count);

	// Write listings - exact mirror of read() above.
	//
	// Every string goes out as a length byte followed by exactly that many
	// bytes, with the length clamped to kMaxListingString. The clamp is
	// repeated, deliberately and identically, in getPacketSize(): the packet
	// size goes into the stream header BEFORE write() is called, so the two
	// must agree for every possible value of every field. Writing a truncated
	// length byte but the full string body desynchronises the connection.
	// oStream.write(std::string) is NOT used here : it would emit the whole
	// string regardless of the length byte just written.
	for (size_t i = 0; i < m_Listings.size(); i++)
	{
		const ExchangeListing & listing = m_Listings[i];
		uint8_t len;

		ulonglong listingID = (ulonglong)listing.listingID;
		oStream.writeWire(listingID);

		oStream.write((ushort)listing.serverID);

		// SellerAccount
		len = (uint8_t)(listing.sellerAccount.length() > kMaxListingString ? kMaxListingString : listing.sellerAccount.length());
		oStream.write(len);
		if (len > 0)
			oStream.write(std::span<const char>(listing.sellerAccount.data(), len));

		// SellerPlayer
		len = (uint8_t)(listing.sellerPlayer.length() > kMaxListingString ? kMaxListingString : listing.sellerPlayer.length());
		oStream.write(len);
		if (len > 0)
			oStream.write(std::span<const char>(listing.sellerPlayer.data(), len));

		oStream.write(listing.sellerRace);
		oStream.write(listing.itemClass);
		oStream.write(listing.itemType);

		ulonglong itemID = (ulonglong)listing.itemID;
		oStream.writeWire(itemID);

		oStream.write(listing.objectID);
		oStream.write(listing.pricePoint);
		oStream.write(listing.currency);
		oStream.write(listing.status);

		// BuyerAccount
		len = (uint8_t)(listing.buyerAccount.length() > kMaxListingString ? kMaxListingString : listing.buyerAccount.length());
		oStream.write(len);
		if (len > 0)
			oStream.write(std::span<const char>(listing.buyerAccount.data(), len));

		// BuyerPlayer
		len = (uint8_t)(listing.buyerPlayer.length() > kMaxListingString ? kMaxListingString : listing.buyerPlayer.length());
		oStream.write(len);
		if (len > 0)
			oStream.write(std::span<const char>(listing.buyerPlayer.data(), len));

		oStream.write(listing.taxRate);
		oStream.write(listing.taxAmount);

		// Timestamp strings
		len = (uint8_t)(listing.createdAt.length() > kMaxListingString ? kMaxListingString : listing.createdAt.length());
		oStream.write(len);
		if (len > 0)
			oStream.write(std::span<const char>(listing.createdAt.data(), len));

		len = (uint8_t)(listing.expireAt.length() > kMaxListingString ? kMaxListingString : listing.expireAt.length());
		oStream.write(len);
		if (len > 0)
			oStream.write(std::span<const char>(listing.expireAt.data(), len));

		oStream.write(listing.version);

		// Snapshot fields
		len = (uint8_t)(listing.itemName.length() > kMaxListingString ? kMaxListingString : listing.itemName.length());
		oStream.write(len);
		if (len > 0)
			oStream.write(std::span<const char>(listing.itemName.data(), len));

		oStream.write(listing.enchantLevel);
		oStream.write(listing.grade);
		oStream.write(listing.durability);
		oStream.write(listing.silver);
		oStream.write(listing.optionType1);
		oStream.write(listing.optionType2);
		oStream.write(listing.optionType3);
		oStream.write(listing.optionValue1);
		oStream.write(listing.optionValue2);
		oStream.write(listing.optionValue3);
		oStream.write(listing.stackCount);
	}

	__END_CATCH
}

PacketSize_t GCExchangeList::getPacketSize() const
{
	PacketSize_t size = szint * 3		// page, pageSize, total
		+ sizeof(uint16_t);				// count

	// Every string uses the SAME clamp as write(), so this equals the number
	// of bytes write() emits for ANY listing contents.
	for (size_t i = 0; i < m_Listings.size(); i++)
	{
		const ExchangeListing & listing = m_Listings[i];

		size += 8;							// listingID (u64 on the wire)
		size += sizeof(uint16_t);			// serverID (u16 on the wire)
		size += sizeof(uint8_t)
			+ (PacketSize_t)(listing.sellerAccount.length() > kMaxListingString ? kMaxListingString : listing.sellerAccount.length());
		size += sizeof(uint8_t)
			+ (PacketSize_t)(listing.sellerPlayer.length() > kMaxListingString ? kMaxListingString : listing.sellerPlayer.length());
		size += sizeof(listing.sellerRace);
		size += sizeof(listing.itemClass);
		size += sizeof(listing.itemType);
		size += 8;							// itemID (u64 on the wire)
		size += sizeof(listing.objectID);
		size += sizeof(listing.pricePoint);
		size += sizeof(listing.currency);
		size += sizeof(listing.status);
		size += sizeof(uint8_t)
			+ (PacketSize_t)(listing.buyerAccount.length() > kMaxListingString ? kMaxListingString : listing.buyerAccount.length());
		size += sizeof(uint8_t)
			+ (PacketSize_t)(listing.buyerPlayer.length() > kMaxListingString ? kMaxListingString : listing.buyerPlayer.length());
		size += sizeof(listing.taxRate);
		size += sizeof(listing.taxAmount);
		size += sizeof(uint8_t)
			+ (PacketSize_t)(listing.createdAt.length() > kMaxListingString ? kMaxListingString : listing.createdAt.length());
		size += sizeof(uint8_t)
			+ (PacketSize_t)(listing.expireAt.length() > kMaxListingString ? kMaxListingString : listing.expireAt.length());
		size += sizeof(listing.version);
		size += sizeof(uint8_t)
			+ (PacketSize_t)(listing.itemName.length() > kMaxListingString ? kMaxListingString : listing.itemName.length());
		size += sizeof(listing.enchantLevel);
		size += sizeof(listing.grade);
		size += sizeof(listing.durability);
		size += sizeof(listing.silver);
		size += sizeof(listing.optionType1);
		size += sizeof(listing.optionType2);
		size += sizeof(listing.optionType3);
		size += sizeof(listing.optionValue1);
		size += sizeof(listing.optionValue2);
		size += sizeof(listing.optionValue3);
		size += sizeof(listing.stackCount);
	}

	return size;
}

string GCExchangeList::toString() const
{
	StringStream msg;
	msg << "GCExchangeList("
		<< "Page:" << m_Page
		<< ",PageSize:" << m_PageSize
		<< ",Total:" << m_Total
		<< ",Count:" << (int)m_Listings.size()
		<< ")";
	return msg.toString();
}

