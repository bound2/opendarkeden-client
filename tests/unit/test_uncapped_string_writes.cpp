//----------------------------------------------------------------------
// test_uncapped_string_writes.cpp
//
// The BYTE-length-prefixed string writers whose write() had no cap at
// all, and the two element counts that narrow the same way. Each field
// is driven over its cap, at its cap, and back through read() where
// read() takes the same length.
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "Exception.h"
#include "Packet.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"

#include "Cpackets/CGAddSMSAddress.h"
#include "Cpackets/CGModifyNickname.h"
#include "Cpackets/CGPartyLeave.h"
#include "Cpackets/CGRequestIP.h"
#include "Cpackets/CGPartySay.h"
#include "Cpackets/CGSMSSend.h"
#include "Cpackets/CGStoreSign.h"
#include "Cpackets/CGTypeStringList.h"

#include <string>
#include <vector>

namespace {

// Encrypting streams at code 0 over a never-used socket, as in
// test_string_write_caps.cpp.
struct UncappedOutFixture
{
	Socket				m_Socket;
	SocketEncryptOutputStream	m_Stream;

	UncappedOutFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket)
	{
		m_Stream.setEncryptCode(0);
	}
};

struct UncappedInFixture
{
	Socket				m_Socket;
	SocketEncryptInputStream	m_Stream;

	UncappedInFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket, 4096)
	{
		m_Stream.setEncryptCode(0);
	}
};

// One string field of one packet: how to set it, how to read it back,
// what write() refuses above, what is already in the ring when it does,
// and the longest length read() accepts (0 when no round-trip applies).
template <class PacketT>
struct UncappedCase
{
	void		(*m_pFill)(PacketT&, const std::string&);
	std::string	(*m_pGet)(PacketT&);
	size_t		m_Cap;
	size_t		m_Prefix;
	size_t		m_ReadCap;
};

template <class PacketT>
void	CheckUncappedStringWrite ( const UncappedCase<PacketT> & c )
{
	const size_t	lengths[] = { c.m_Cap + 1, 255, 256, 257, 256 + c.m_Cap, 300 };

	for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++)
	{
		PacketT			packet;
		UncappedOutFixture	out;
		bool			bThrew = false;

		c.m_pFill(packet, std::string(lengths[i], 'z'));

		try {
			packet.write(out.m_Stream);
		} catch (InvalidProtocolException&) {
			bThrew = true;
		}

		CHECK(bThrew);
		CHECK_EQ(c.m_Prefix, (size_t)out.m_Stream.size());
	}

	// Exactly the cap goes out whole, at exactly getPacketSize() bytes.
	{
		PacketT			packet;
		UncappedOutFixture	out;

		c.m_pFill(packet, std::string(c.m_Cap, 'z'));
		packet.write(out.m_Stream);

		CHECK_EQ((size_t)packet.getPacketSize(), (size_t)out.m_Stream.size());
	}

	// And the reader takes back the field that went in.
	if (c.m_ReadCap > 0)
	{
		const size_t	len = c.m_ReadCap < c.m_Cap ? c.m_ReadCap : c.m_Cap;
		const std::string value(len, 'z');

		PacketT			src;
		UncappedOutFixture	out;

		c.m_pFill(src, value);
		src.write(out.m_Stream);

		const std::vector<unsigned char> body =
			SocketOutputStreamTestAccess::Bytes(out.m_Stream);

		PacketT			dst;
		UncappedInFixture	in;

		SocketInputStreamTestAccess::Preload(in.m_Stream,
			body.empty() ? NULL : &body[0], (unsigned int)body.size());

		dst.read(in.m_Stream);

		CHECK(c.m_pGet(dst) == value);
	}
}

} // namespace

// Cap 128, what CGPartySayFactory::getPacketMaxSize() allows the message.
TEST(UncappedStringWrites, CGPartySayMessage)
{
	const UncappedCase<CGPartySay> c = {
		+[](CGPartySay& p, const std::string& s) { p.setColor(0); p.setMessage(s); },
		+[](CGPartySay& p) { return p.getMessage(); },
		128, 0, 128 };

	CheckUncappedStringWrite(c);
}

// Cap 10, what CGPartyLeaveFactory::getPacketMaxSize() allows the name.
TEST(UncappedStringWrites, CGPartyLeaveTargetName)
{
	const UncappedCase<CGPartyLeave> c = {
		+[](CGPartyLeave& p, const std::string& s) { p.setTargetName(s); },
		+[](CGPartyLeave& p) { return p.getTargetName(); },
		10, 0, 10 };

	CheckUncappedStringWrite(c);
}

TEST(UncappedStringWrites, CGPartyLeaveEmptyTargetNameStillWritesOneZeroByte)
{
	CGPartyLeave		packet;
	UncappedOutFixture	out;

	packet.setTargetName("");
	packet.write(out.m_Stream);

	const std::vector<unsigned char> body =
		SocketOutputStreamTestAccess::Bytes(out.m_Stream);

	CHECK_EQ((size_t)1, body.size());
	CHECK_EQ(0, (int)body[0]);
}

// Cap 10, what CGRequestIPFactory::getPacketMaxSize() allows the name.
TEST(UncappedStringWrites, CGRequestIPName)
{
	const UncappedCase<CGRequestIP> c = {
		+[](CGRequestIP& p, const std::string& s) { p.setName(s.c_str()); },
		+[](CGRequestIP& p) { return p.getName(); },
		10, 0, 10 };

	CheckUncappedStringWrite(c);
}

TEST(UncappedStringWrites, CGRequestIPEmptyNameStillWritesOneZeroByte)
{
	CGRequestIP			packet;
	UncappedOutFixture	out;

	packet.setName("");
	packet.write(out.m_Stream);

	const std::vector<unsigned char> body =
		SocketOutputStreamTestAccess::Bytes(out.m_Stream);

	CHECK_EQ((size_t)1, body.size());
	CHECK_EQ(0, (int)body[0]);
}

// Cap 80, what CGStoreSignFactory::getPacketMaxSize() allows the sign.
TEST(UncappedStringWrites, CGStoreSignSign)
{
	const UncappedCase<CGStoreSign> c = {
		+[](CGStoreSign& p, const std::string& s) { p.setSign(s); },
		+[](CGStoreSign& p) { return p.getSign(); },
		80, 0, 80 };

	CheckUncappedStringWrite(c);
}

// Cap MAX_NICKNAME_SIZE (22), what getPacketMaxSize() allows the string.
TEST(UncappedStringWrites, CGModifyNicknameNickname)
{
	const UncappedCase<CGModifyNickname> c = {
		+[](CGModifyNickname& p, const std::string& s) {
			p.setNicknameID(1); p.setNickname(s); },
		+[](CGModifyNickname& p) { return p.getNickname(); },
		MAX_NICKNAME_SIZE, szObjectID, MAX_NICKNAME_SIZE };

	CheckUncappedStringWrite(c);
}

// Caps 20 / 40 / 11, the three allowances in getPacketMaxSize().
TEST(UncappedStringWrites, CGAddSMSAddressCharacterName)
{
	const UncappedCase<CGAddSMSAddress> c = {
		+[](CGAddSMSAddress& p, const std::string& s) {
			p.setCharacterName(s); p.setCustomName("a"); p.setNumber("a"); },
		+[](CGAddSMSAddress& p) { return p.getCharacterName(); },
		20, 0, 20 };

	CheckUncappedStringWrite(c);
}

TEST(UncappedStringWrites, CGAddSMSAddressCustomName)
{
	// The character name and its length byte are already in the ring.
	const UncappedCase<CGAddSMSAddress> c = {
		+[](CGAddSMSAddress& p, const std::string& s) {
			p.setCharacterName("a"); p.setCustomName(s); p.setNumber("a"); },
		+[](CGAddSMSAddress& p) { return p.getCustomName(); },
		40, szBYTE + 1, 40 };

	CheckUncappedStringWrite(c);
}

TEST(UncappedStringWrites, CGAddSMSAddressNumber)
{
	// The character name and the custom name are already in the ring.
	const UncappedCase<CGAddSMSAddress> c = {
		+[](CGAddSMSAddress& p, const std::string& s) {
			p.setCharacterName("a"); p.setCustomName("a"); p.setNumber(s); },
		+[](CGAddSMSAddress& p) { return p.getNumber(); },
		11, (szBYTE + 1) * 2, 11 };

	CheckUncappedStringWrite(c);
}

// Cap MAX_STRING_LENGTH (50), read()'s own bound and what the factory allows.
TEST(UncappedStringWrites, CGTypeStringListString)
{
	const UncappedCase<CGTypeStringList> c = {
		+[](CGTypeStringList& p, const std::string& s) {
			p.setType(CGTypeStringList::STRING_TYPE_WAIT_FOR_MEET);
			p.clearString(); p.addString(s); p.setParam(0); },
		+[](CGTypeStringList& p) { return p.getSize() > 0 ? p.popString() : std::string(); },
		MAX_STRING_LENGTH, szBYTE + szBYTE, MAX_STRING_LENGTH };

	CheckUncappedStringWrite(c);
}

// Caps MAX_NUMBER_LENGTH (11), what the SMS dialog produces, for both numbers.
TEST(UncappedStringWrites, CGSMSSendNumber)
{
	// The list count is already in the ring when the entry is refused.
	const UncappedCase<CGSMSSend> c = {
		+[](CGSMSSend& p, const std::string& s) {
			p.clearString(); p.addString(s);
			p.setCallerNumber("a"); p.setMessage("a"); },
		+[](CGSMSSend& p) {
			return p.getNumbersList().empty()
				? std::string() : p.getNumbersList().front(); },
		MAX_NUMBER_LENGTH, szBYTE, 0 };

	CheckUncappedStringWrite(c);
}

TEST(UncappedStringWrites, CGSMSSendCallerNumber)
{
	// read() Asserts below MAX_NUMBER_LENGTH, so the round-trip runs one under.
	const UncappedCase<CGSMSSend> c = {
		+[](CGSMSSend& p, const std::string& s) {
			p.clearString(); p.setCallerNumber(s); p.setMessage("a"); },
		+[](CGSMSSend& p) { return p.getCallerNumber(); },
		MAX_NUMBER_LENGTH, szBYTE, MAX_NUMBER_LENGTH - 1 };

	CheckUncappedStringWrite(c);
}

// Cap MAX_MESSAGE_LENGTH - 1 (39), read()'s own bound.
TEST(UncappedStringWrites, CGSMSSendMessage)
{
	const UncappedCase<CGSMSSend> c = {
		+[](CGSMSSend& p, const std::string& s) {
			p.clearString(); p.setCallerNumber("a"); p.setMessage(s); },
		+[](CGSMSSend& p) { return p.getMessage(); },
		MAX_MESSAGE_LENGTH - 1, szBYTE + szBYTE + 1, MAX_MESSAGE_LENGTH - 1 };

	CheckUncappedStringWrite(c);
}

// Count cap MAX_STRING_NUM (20), what getPacketMaxSize() sizes the list for.
TEST(UncappedStringWrites, CGTypeStringListElementCount)
{
	const size_t	counts[] = { MAX_STRING_NUM + 1, 256, 300 };

	for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); i++)
	{
		CGTypeStringList	packet;
		UncappedOutFixture	out;
		bool			bThrew = false;

		packet.setType(CGTypeStringList::STRING_TYPE_WAIT_FOR_MEET);
		packet.setParam(0);
		for (size_t j = 0; j < counts[i]; j++)
			packet.addString("a");

		try {
			packet.write(out.m_Stream);
		} catch (InvalidProtocolException&) {
			bThrew = true;
		}

		CHECK(bThrew);
		CHECK_EQ((size_t)szBYTE, (size_t)out.m_Stream.size());
	}

	// Exactly the maximum still goes out, whole.
	{
		CGTypeStringList	packet;
		UncappedOutFixture	out;

		packet.setType(CGTypeStringList::STRING_TYPE_WAIT_FOR_MEET);
		packet.setParam(0);
		for (size_t j = 0; j < MAX_STRING_NUM; j++)
			packet.addString("a");

		packet.write(out.m_Stream);

		CHECK_EQ((size_t)packet.getPacketSize(), (size_t)out.m_Stream.size());
	}
}

// Count cap MAX_RECEVIER_NUM (5), what the SMS dialog sends at most.
TEST(UncappedStringWrites, CGSMSSendNumberCount)
{
	const size_t	counts[] = { MAX_RECEVIER_NUM + 1, 256, 300 };

	for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); i++)
	{
		CGSMSSend		packet;
		UncappedOutFixture	out;
		bool			bThrew = false;

		packet.clearString();
		packet.setCallerNumber("a");
		packet.setMessage("a");
		for (size_t j = 0; j < counts[i]; j++)
			packet.addString("1");

		try {
			packet.write(out.m_Stream);
		} catch (InvalidProtocolException&) {
			bThrew = true;
		}

		CHECK(bThrew);
		CHECK_EQ((size_t)0, (size_t)out.m_Stream.size());
	}

	// Exactly the maximum still goes out, whole.
	{
		CGSMSSend		packet;
		UncappedOutFixture	out;

		packet.clearString();
		packet.setCallerNumber("a");
		packet.setMessage("a");
		for (size_t j = 0; j < MAX_RECEVIER_NUM; j++)
			packet.addString("1");

		packet.write(out.m_Stream);

		CHECK_EQ((size_t)packet.getPacketSize(), (size_t)out.m_Stream.size());
	}
}
