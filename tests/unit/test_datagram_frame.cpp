//----------------------------------------------------------------------
// test_datagram_frame.cpp
//----------------------------------------------------------------------
//
// The UDP framing contract of Datagram, the peer-to-peer and
// login-server-port-check transport: what Datagram::read(DatagramPacket*&)
// accepts and refuses, and what the two bounded primitives every
// datagram packet parses through do at the end of the buffer.
//
// The bytes Datagram::write puts down are pinned as goldens in
// test_packet_goldens.cpp (CGPortCheck.datagram, RCPositionInfo.datagram),
// not here. Here the frame is built by hand, so the read side is
// checked against the wire layout and not against the writer.
//
// The read path creates the packet through g_pPacketFactoryManager, so
// these tests install a real manager for their duration, the way
// test_player_base.cpp does.
//
// Compiled with the packetwire defines (tests/CMakeLists.txt).
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "Datagram.h"
#include "DatagramPacket.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactoryManager.h"
#include "Rpackets/RCPositionInfo.h"

#include <cstring>
#include <string>
#include <vector>

namespace {

class FactoryManagerScope
{
public:
	explicit FactoryManagerScope(PacketFactoryManager* pManager)
	: m_pPrevious(g_pPacketFactoryManager)
	{
		g_pPacketFactoryManager = pManager;
	}

	~FactoryManagerScope()
	{
		g_pPacketFactoryManager = m_pPrevious;
	}

private:
	PacketFactoryManager* m_pPrevious;
};

// A datagram holding exactly `bytes`.
void	Load(Datagram& datagram, const std::vector<unsigned char>& bytes)
{
	std::vector<char> buffer(bytes.begin(), bytes.end());
	datagram.setData(buffer.empty() ? NULL : &buffer[0], (uint)buffer.size());
}

void	AppendLE(std::vector<unsigned char>& out, unsigned long long value, size_t width)
{
	for (size_t i = 0; i < width; i++)
		out.push_back((unsigned char)((value >> (8 * i)) & 0xFF));
}

// RCPositionInfo's body as a peer would put it on the wire: a length
// byte, the name, then ZoneID (2), X (1), Y (1).
const char*	kPeerName = "Nosferatu";

std::vector<unsigned char>	PositionInfoBody()
{
	std::vector<unsigned char> body;
	const size_t nameLength = std::strlen(kPeerName);
	body.push_back((unsigned char)nameLength);
	body.insert(body.end(), kPeerName, kPeerName + nameLength);
	AppendLE(body, 0x8A9B, szZoneID);
	body.push_back(0xC5);
	body.push_back(0xD6);
	return body;
}

// A whole datagram: id, size field, body, then the one-byte pad both
// peers count in the length. `sizeField` and `length` default to the
// honest values and can be pushed off them.
std::vector<unsigned char>	Frame(PacketID_t id, const std::vector<unsigned char>& body,
				      long sizeDrift = 0, long padBytes = 1)
{
	std::vector<unsigned char> frame;
	AppendLE(frame, id, szPacketID);
	AppendLE(frame, (unsigned long long)((long)body.size() + sizeDrift), szPacketSize);
	frame.insert(frame.end(), body.begin(), body.end());
	for (long i = 0; i < padBytes; i++)
		frame.push_back(0x00);
	return frame;
}

enum ReadOutcome
{
	READ_PACKET,
	READ_INVALID_PROTOCOL,
	READ_ERROR,
	READ_OTHER
};

// Drive Datagram::read on `frame` and report how it ended. A packet
// that came back is handed to the caller through `ppPacket`.
ReadOutcome	ReadFrame(const std::vector<unsigned char>& frame, DatagramPacket** ppPacket = NULL)
{
	Datagram datagram;
	Load(datagram, frame);
	DatagramPacket* pPacket = NULL;
	try {
		datagram.read(pPacket);
	} catch (InvalidProtocolException&) {
		delete pPacket;
		return READ_INVALID_PROTOCOL;
	} catch (Error&) {
		delete pPacket;
		return READ_ERROR;
	} catch (Throwable&) {
		delete pPacket;
		return READ_OTHER;
	}
	if (ppPacket != NULL)
		*ppPacket = pPacket;
	else
		delete pPacket;
	return READ_PACKET;
}

} // namespace

//----------------------------------------------------------------------
// The read path rebuilds the packet the id names
//----------------------------------------------------------------------
TEST(Datagram, ReadRebuildsRCPositionInfoThroughTheFactory)
{
	PacketFactoryManager manager;
	manager.init();
	FactoryManagerScope scope(&manager);

	const std::vector<unsigned char> frame =
		Frame(Packet::PACKET_RC_POSITION_INFO, PositionInfoBody());
	CHECK_EQ(szPacketHeader + PositionInfoBody().size(), frame.size());

	Datagram datagram;
	Load(datagram, frame);
	datagram.setHost("127.0.0.1");
	datagram.setPort(0x9ABC);

	DatagramPacket* pPacket = NULL;
	datagram.read(pPacket);
	CHECK(pPacket != NULL);
	if (pPacket == NULL)
		return;

	RCPositionInfo* pInfo = dynamic_cast<RCPositionInfo*>(pPacket);
	CHECK(pInfo != NULL);
	if (pInfo != NULL)
	{
		CHECK_EQ((long long)Packet::PACKET_RC_POSITION_INFO, (long long)pInfo->getPacketID());
		CHECK(std::string(kPeerName) == pInfo->getName());
		CHECK_EQ(0x8A9B, pInfo->getZoneID());
		CHECK_EQ(0xC5, pInfo->getZoneX());
		CHECK_EQ(0xD6, pInfo->getZoneY());
		// The sender's address travels with the packet.
		CHECK(std::string("127.0.0.1") == pInfo->getHost());
		CHECK_EQ(0x9ABCu, pInfo->getPort());
	}
	delete pPacket;
}

//----------------------------------------------------------------------
// What the read path refuses, and with which exception
//----------------------------------------------------------------------
TEST(Datagram, ReadRefusesAnIdAtOrPastPacketMax)
{
	PacketFactoryManager manager;
	manager.init();
	FactoryManagerScope scope(&manager);

	const std::vector<unsigned char> body = PositionInfoBody();
	CHECK_EQ(READ_INVALID_PROTOCOL, ReadFrame(Frame((PacketID_t)Packet::PACKET_MAX, body)));
	CHECK_EQ(READ_INVALID_PROTOCOL, ReadFrame(Frame((PacketID_t)0xFFFF, body)));
}

TEST(Datagram, ReadRefusesASizeFieldOverTheFactoryMaximum)
{
	PacketFactoryManager manager;
	manager.init();
	FactoryManagerScope scope(&manager);

	const PacketSize_t maxSize = manager.getPacketMaxSize(Packet::PACKET_RC_POSITION_INFO);
	std::vector<unsigned char> body(maxSize + 1, 0x00);
	body[0] = 0x00;	// an empty name, so the parser would not be the refuser
	CHECK_EQ(READ_INVALID_PROTOCOL, ReadFrame(Frame(Packet::PACKET_RC_POSITION_INFO, body)));
}

// Both peers count the datagram as szPacketHeader + size field: one
// byte more than the id, the size and the body. A datagram shorter
// than that is "not read whole", a longer one "several at once", and
// both are refused before any packet is created.
TEST(Datagram, ReadRefusesALengthThatDisagreesWithTheSizeField)
{
	PacketFactoryManager manager;
	manager.init();
	FactoryManagerScope scope(&manager);

	const std::vector<unsigned char> body = PositionInfoBody();
	CHECK_EQ(READ_PACKET, ReadFrame(Frame(Packet::PACKET_RC_POSITION_INFO, body, 0, 1)));
	CHECK_EQ(READ_ERROR, ReadFrame(Frame(Packet::PACKET_RC_POSITION_INFO, body, 0, 0)));
	CHECK_EQ(READ_ERROR, ReadFrame(Frame(Packet::PACKET_RC_POSITION_INFO, body, 0, 2)));
	CHECK_EQ(READ_ERROR, ReadFrame(Frame(Packet::PACKET_RC_POSITION_INFO, body, 1, 1)));
	CHECK_EQ(READ_ERROR, ReadFrame(Frame(Packet::PACKET_RC_POSITION_INFO, body, -1, 1)));
}

//----------------------------------------------------------------------
// The two bounded primitives every datagram packet parses through
//----------------------------------------------------------------------
TEST(Datagram, ReadingPastTheEndUnderflows)
{
	std::vector<unsigned char> bytes;
	bytes.push_back(0xA1);
	bytes.push_back(0xB2);
	bytes.push_back(0xC3);

	{
		Datagram datagram;
		Load(datagram, bytes);
		char buffer[4] = { 0, 0, 0, 0 };
		datagram.read(buffer, 3);
		CHECK_EQ(0, std::memcmp(buffer, "\xA1\xB2\xC3", 3));
		bool bThrew = false;
		try {
			datagram.read(buffer, 1);
		} catch (InsufficientDataException&) {
			bThrew = true;
		}
		CHECK(bThrew);
	}
	{
		Datagram datagram;
		Load(datagram, bytes);
		std::string text;
		datagram.read(text, 2);
		CHECK(std::string("\xA1\xB2") == text);
		bool bThrew = false;
		try {
			datagram.read(text, 2);
		} catch (InsufficientDataException&) {
			bThrew = true;
		}
		CHECK(bThrew);
		// A refused read leaves the string as it was.
		CHECK(std::string("\xA1\xB2") == text);
	}
}
