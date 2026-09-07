//////////////////////////////////////////////////////////////////////////////
// Filename    : CGExchangeBuy.h
// Written By  : Exchange System
// Description : Client buys an item from the exchange
//////////////////////////////////////////////////////////////////////////////

#ifndef __CG_EXCHANGE_BUY_H__
#define __CG_EXCHANGE_BUY_H__

#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////////////
// class CGExchangeBuy;
//////////////////////////////////////////////////////////////////////////////

class CGExchangeBuy : public Packet
{
public:
	// Maximum wire length, in bytes, of m_IdempotencyKey. write(), read() and
	// getPacketSize() all clamp to this value, and the factory's max size is
	// derived from it. MUST stay equal to the server repo's constant of the
	// same name.
	//
	// Deliberately 64 and not the 255 a BYTE length prefix would allow: the
	// server stores the key in a VARCHAR(64) UNIQUE column. A longer key would
	// be silently truncated on insert, so two different keys sharing a 64-byte
	// prefix would collide on the unique index and the duplicate-purchase
	// guard would misfire. Clamping on the wire keeps what the client sends,
	// what the server reads and what the database stores identical.
	static const PacketSize_t kMaxIdempotencyKey = 64;

	CGExchangeBuy();
	virtual ~CGExchangeBuy();

	void read(SocketInputStream& iStream);
	void write(SocketOutputStream& oStream) const;

	// listingID (u64) + idempotency key length byte + clamped key body.
	// The clamp is the same one write() applies, so this equals the number of
	// bytes write() emits for ANY value of m_IdempotencyKey.
	PacketSize_t getPacketSize() const
	{
		return sizeof(ulonglong) + sizeof(BYTE)
			+ (PacketSize_t)(m_IdempotencyKey.length() > kMaxIdempotencyKey ? kMaxIdempotencyKey : m_IdempotencyKey.length());
	}
	PacketID_t getPacketID() const { return PACKET_CG_EXCHANGE_BUY; }
	string getPacketName() const { return "CGExchangeBuy"; }
	string toString() const;

	// Getters
	ulonglong getListingID() const { return m_ListingID; }
	const string& getIdempotencyKey() const { return m_IdempotencyKey; }

	// Setters
	void setListingID(ulonglong listingID) { m_ListingID = listingID; }
	void setIdempotencyKey(const string& idempotencyKey) { m_IdempotencyKey = idempotencyKey; }

private:
	ulonglong	m_ListingID;
	string		m_IdempotencyKey;
};

//////////////////////////////////////////////////////////////////////////////
// class CGExchangeBuyFactory;
//////////////////////////////////////////////////////////////////////////////

class CGExchangeBuyFactory : public PacketFactory
{
public:
	Packet* createPacket() { return new CGExchangeBuy(); }
	string getPacketName() const { return "CGExchangeBuy"; }
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_CG_EXCHANGE_BUY; }
	// listingID(8) + idempotency key length byte(1) + key body(64) = 73.
	// write() clamps the body to kMaxIdempotencyKey, so getPacketSize() can
	// never exceed this.
	// Must stay identical to the server repo's CGExchangeBuyFactory::getPacketMaxSize()
	PacketSize_t getPacketMaxSize() const noexcept { return 8 + 1 + CGExchangeBuy::kMaxIdempotencyKey; }
};

#endif
