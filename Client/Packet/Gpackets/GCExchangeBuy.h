//////////////////////////////////////////////////////////////////////////////
// Filename    : GCExchangeBuy.h
// Written By  : Exchange System
// Description : Server replies to the client's exchange buy request
//////////////////////////////////////////////////////////////////////////////

#ifndef __GC_EXCHANGE_BUY_H__
#define __GC_EXCHANGE_BUY_H__

#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////////////
// class GCExchangeBuy;
//////////////////////////////////////////////////////////////////////////////

class GCExchangeBuy : public Packet
{
public:
	// Maximum wire length, in bytes, of m_Message. write(), read() and
	// getPacketSize() all clamp to this value, and the factory's max size is
	// derived from it. MUST stay equal to the server repo's constant of the
	// same name.
	static const PacketSize_t kMaxMessage = 255;

	GCExchangeBuy();
	virtual ~GCExchangeBuy();

	void read(SocketInputStream & iStream);
	void write(SocketOutputStream & oStream) const;

	// uint8 success + bstr message (length byte always written, body clamped
	// to kMaxMessage exactly as write() clamps it) + uint64 orderID
	PacketSize_t getPacketSize() const
	{
		return sizeof(BYTE) + sizeof(BYTE)
			+ (PacketSize_t)(m_Message.length() > kMaxMessage ? kMaxMessage : m_Message.length())
			+ sizeof(ulonglong);
	}
	PacketID_t getPacketID() const noexcept { return PACKET_GC_EXCHANGE_BUY; }
	string getPacketName() const { return "GCExchangeBuy"; }
	string toString() const;

	// Getters
	bool isSuccess() const { return m_Success; }
	const string& getMessage() const { return m_Message; }
	ulonglong getOrderID() const { return m_OrderID; }

	// Setters
	void setSuccess(bool success) { m_Success = success; }
	void setMessage(const string& message) { m_Message = message; }
	void setOrderID(ulonglong orderID) { m_OrderID = orderID; }

private:
	bool		m_Success;	// transported as uint8
	string		m_Message;
	ulonglong	m_OrderID;
};

//////////////////////////////////////////////////////////////////////////////
// class GCExchangeBuyFactory;
//////////////////////////////////////////////////////////////////////////////

class GCExchangeBuyFactory : public PacketFactory
{
public:
	Packet* createPacket() { return new GCExchangeBuy(); }
	string getPacketName() const { return "GCExchangeBuy"; }
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_GC_EXCHANGE_BUY; }
	// success(1) + message length byte(1) + message body(255) + orderID(8)
	// = 265. write() clamps the body to kMaxMessage, so getPacketSize() can
	// never exceed this.
	// Must stay identical to the server repo's GCExchangeBuyFactory::getPacketMaxSize()
	PacketSize_t getPacketMaxSize() const noexcept { return 1 + 1 + GCExchangeBuy::kMaxMessage + 8; }
};

//////////////////////////////////////////////////////////////////////////////
// class GCExchangeBuyHandler;
//////////////////////////////////////////////////////////////////////////////

class GCExchangeBuyHandler
{
public:
	static void execute(GCExchangeBuy* pPacket, Player* pPlayer);
};

#endif // __GC_EXCHANGE_BUY_H__
