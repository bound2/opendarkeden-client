//----------------------------------------------------------------------
// test_string_write_caps.cpp
//----------------------------------------------------------------------
//
// Every client packet write() that puts a BYTE-length-prefixed string on
// the wire, over the lengths that the length byte cannot express.
//
// The shape these all share, and the defect in it:
//
//	BYTE szMessage = m_Message.size();	// narrows
//	if (szMessage > 128) throw ...;		// caps the NARROWED value
//	oStream.write(szMessage);
//	oStream.write(m_Message);		// writes the WHOLE string
//
// (BYTE)276 is 20, so a 276-character message narrows below the cap,
// passes the test, and then goes out as 276 bytes behind a length byte
// claiming 20. The framing header is written from getPacketSize(), which
// counts the real size, so the frame is the right length and its
// CONTENTS are not: the peer takes 20 bytes as the message and parses
// the other 256 as whatever packet follows. This repo's frame-bounded
// reader rejects the tail; the server's legacy reader does not.
//
// The window is exactly 256 + 1 .. 256 + cap (and 512 + 1 .. 512 + cap,
// which no caller can build). Below it, 129..255 already threw; at 256
// the narrowed length is 0, which the "== 0" test rejects in the packets
// that have one - and in the packets that do NOT have one, 256 writes a
// zero length byte and no body against a header claiming 256 bytes,
// which desynchronises just as badly. So the tests below refuse
// everything above the cap the length byte can express, which is
// min(cap, 255).
//
// What each field is checked for:
//
//   - write() throws InvalidProtocolException at every length above that
//     cap, and the output ring holds exactly the bytes of the FIELDS
//     BEFORE it - nothing of the string, and nothing after it. The
//     framing header is not involved: these tests call write(stream)
//     directly rather than through write(const Packet*), which is what
//     puts the header in the ring (test_output_stream_rollback.cpp
//     covers that half);
//   - a string of exactly the cap is accepted, and the body write emits
//     exactly getPacketSize() bytes. That is the property the bounded
//     write exists to hold: the framing header, the length byte and the
//     bytes that follow it all agree;
//   - where read() accepts the same length, the field survives a
//     write/read cycle. The read cap is given per field because several
//     of these packets are ASYMMETRIC - CGWhisper::write caps a name at
//     128 and CGWhisper::read at 10 - and the round-trip runs at
//     whichever is smaller. The cycle compares the field, not the whole
//     stream: CLLogin::write emits a login-mode byte its own read() does
//     not consume, which is upstream's and none of this change's
//     business.
//
// CLRegisterPlayer is deliberately absent. It has the same eleven narrowed
// lengths, but every one of its setters truncates to the same maximum
// the write tests against (setID does `id.substr(0, maxIDLength)`), so
// no over-cap string can reach its write() at all. A test there would
// pin the setters, not the writer.
//
// Compiled with the packetwire defines (tests/CMakeLists.txt), so the
// Packet and stream definitions are identical to the library's.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "Datagram.h"
#include "Exception.h"
#include "Packet.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"

#include "Cpackets/CGAppointSubmaster.h"
#include "Cpackets/CGConnect.h"
#include "Cpackets/CGGlobalChat.h"
#include "Cpackets/CGGuildChat.h"
#include "Cpackets/CGJoinGuild.h"
#include "Cpackets/CGModifyGuildIntro.h"
#include "Cpackets/CGModifyGuildMember.h"
#include "Cpackets/CGModifyGuildMemberIntro.h"
#include "Cpackets/CGPhoneSay.h"
#include "Cpackets/CGPortCheck.h"
#include "Cpackets/CGRangerSay.h"
#include "Cpackets/CGRegistGuild.h"
#include "Cpackets/CGRequestPowerPoint.h"
#include "Cpackets/CGSay.h"
#include "Cpackets/CGSelectGuildMember.h"
#include "Cpackets/CGUseMessageItemFromInventory.h"
#include "Cpackets/CGWhisper.h"
#include "Cpackets/CLCreatePC.h"
#include "Cpackets/CLDeletePC.h"
#include "Cpackets/CLLogin.h"
#include "Cpackets/CLQueryCharacterName.h"
#include "Cpackets/CLQueryPlayerID.h"
#include "Cpackets/CLSelectPC.h"

#include <string>
#include <vector>

namespace {

//----------------------------------------------------------------------
// Streams over a never-used socket (see tests/support/packet_stream_access.h).
//
// The ENCRYPTING streams, at code 0, which is the plain branch of every
// packet that has one and byte-identical to the plain stream (the same
// choice test_packet_skill_family_wire.cpp makes, against goldens).
// CGUseItemFromInventory::write dynamic_casts its stream and Asserts the
// cast under __USE_ENCRYPTER__, so a plain SocketOutputStream throws an
// Error out of that one before it reaches any cap at all.
//----------------------------------------------------------------------
struct CapOutFixture
{
	Socket				m_Socket;
	SocketEncryptOutputStream	m_Stream;

	CapOutFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket)
	{
		m_Stream.setEncryptCode(0);
	}
};

struct CapInFixture
{
	Socket				m_Socket;
	SocketEncryptInputStream	m_Stream;

	CapInFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket, 4096)
	{
		m_Stream.setEncryptCode(0);
	}
};

//----------------------------------------------------------------------
// The whole point of the exercise, spelled once: what a length byte can
// say. A cap of 255 or 256 in the source is a cap of 255 on the wire.
//----------------------------------------------------------------------
size_t	WireCap ( size_t cap )
{
	return cap < 255 ? cap : 255;
}

//----------------------------------------------------------------------
// One string field of one packet. m_pFill builds a whole valid packet
// with the field under test set to the given string; m_pGet reads that
// same field back. m_Prefix is what write() has already put in the ring
// when it refuses - the fields ahead of this one.
//----------------------------------------------------------------------
template <class PacketT>
struct CapCase
{
	void		(*m_pFill)(PacketT&, const std::string&);
	std::string	(*m_pGet)(PacketT&);

	// What write() refuses ABOVE, as the source spells it.
	size_t		m_Cap;

	// Bytes in the output ring when it throws.
	size_t		m_Prefix;

	// The longest string read() accepts for this field; 0 when the
	// round-trip does not apply (read() is not compiled, or does not
	// come from a SocketInputStream).
	size_t		m_ReadCap;
};

//----------------------------------------------------------------------
// The lengths that must be refused, and the one that must not.
//----------------------------------------------------------------------
template <class PacketT>
void	CheckStringWriteCap ( const CapCase<PacketT> & c )
{
	const size_t	cap = WireCap(c.m_Cap);

	// cap + 1 and 255 always threw. 256 narrows to zero. 257 and
	// 256 + cap are the window the narrowing hid, and 300 is a plain
	// long chat line - 100 Korean characters out of the chat box.
	const size_t	lengths[] = { cap + 1, 255, 256, 257, 256 + cap, 300 };

	for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++)
	{
		// A 255-byte string is inside a 255-byte cap, and a
		// 300-byte one is not over a 511-byte one. Only the
		// lengths this field really must refuse are asserted.
		if (lengths[i] <= cap)
			continue;

		PacketT		packet;
		CapOutFixture	out;
		bool		bThrew = false;

		c.m_pFill(packet, std::string(lengths[i], 'z'));

		try {
			packet.write(out.m_Stream);
		} catch (InvalidProtocolException&) {
			bThrew = true;
		}

		CHECK(bThrew);
		CHECK_EQ(c.m_Prefix, (size_t)out.m_Stream.size());
	}

	// Exactly the cap goes out whole, and the body is as long as the
	// framing header says it is - so the length byte cannot disagree
	// with the bytes after it.
	{
		PacketT		packet;
		CapOutFixture	out;

		c.m_pFill(packet, std::string(cap, 'z'));
		packet.write(out.m_Stream);

		CHECK_EQ((size_t)packet.getPacketSize(), (size_t)out.m_Stream.size());
	}

	// And what the reader takes back out of those bytes is the field
	// that went in.
	if (c.m_ReadCap > 0)
	{
		const size_t	len = c.m_ReadCap < cap ? c.m_ReadCap : cap;
		const std::string value(len, 'z');

		PacketT		src;
		CapOutFixture	out;

		c.m_pFill(src, value);
		src.write(out.m_Stream);

		const std::vector<unsigned char> body =
			SocketOutputStreamTestAccess::Bytes(out.m_Stream);

		PacketT		dst;
		CapInFixture	in;

		SocketInputStreamTestAccess::Preload(in.m_Stream,
			body.empty() ? NULL : &body[0], (unsigned int)body.size());

		dst.read(in.m_Stream);

		CHECK(c.m_pGet(dst) == value);
	}
}

} // namespace

//----------------------------------------------------------------------
// Chat: the reachable half of this family
//
// The chat box bounds its input with LineEditor::m_Limit, which counts
// CHARACTERS (the editor holds UTF-32) while the packet counts BYTES of
// the UTF-8 it hands over. C_VS_UI_GAME_COMMON sets that limit to 100,
// so a line of 100 Korean characters is 300 bytes - inside the window
// these tests cover, and typed by an ordinary player.
//----------------------------------------------------------------------
TEST(StringWriteCaps, CGSayMessage)
{
	const CapCase<CGSay> c = {
		+[](CGSay& p, const std::string& s) { p.setColor(0); p.setMessage(s); },
		+[](CGSay& p) { return p.getMessage(); },
		CGSay::MAX_MESSAGE_SIZE, szuint, CGSay::MAX_MESSAGE_SIZE };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CGGlobalChatMessage)
{
	const CapCase<CGGlobalChat> c = {
		+[](CGGlobalChat& p, const std::string& s) { p.setColor(0); p.setMessage(s); },
		+[](CGGlobalChat& p) { return p.getMessage(); },
		128, szuint, 128 };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CGGuildChatMessage)
{
	const CapCase<CGGuildChat> c = {
		+[](CGGuildChat& p, const std::string& s) {
			p.SetType(0); p.setColor(0); p.setMessage(s); },
		+[](CGGuildChat& p) { return p.getMessage(); },
		128, szBYTE + szuint, 128 };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CGRangerSayMessage)
{
	const CapCase<CGRangerSay> c = {
		+[](CGRangerSay& p, const std::string& s) { p.setMessage(s); },
		+[](CGRangerSay& p) { return p.getMessage(); },
		128, 0, 128 };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CGPhoneSayMessage)
{
	const CapCase<CGPhoneSay> c = {
		+[](CGPhoneSay& p, const std::string& s) { p.setSlotID(0); p.setMessage(s); },
		+[](CGPhoneSay& p) { return p.getMessage(); },
		128, szSlotID, 128 };

	CheckStringWriteCap(c);
}

//----------------------------------------------------------------------
// Whisper: two strings, and two caps that do not match its reader
//
// write() bounds the name at 128 while read() bounds it at 10, which is
// upstream's asymmetry and not this change's to close - the round-trip
// below therefore runs at 10.
//----------------------------------------------------------------------
TEST(StringWriteCaps, CGWhisperName)
{
	const CapCase<CGWhisper> c = {
		+[](CGWhisper& p, const std::string& s) {
			p.setName(s); p.setColor(0); p.setMessage("a"); },
		+[](CGWhisper& p) { return p.getName(); },
		128, 0, 10 };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CGWhisperMessage)
{
	const CapCase<CGWhisper> c = {
		+[](CGWhisper& p, const std::string& s) {
			p.setName("a"); p.setColor(0); p.setMessage(s); },
		+[](CGWhisper& p) { return p.getMessage(); },
		128, szBYTE + 1 + szuint, 128 };

	CheckStringWriteCap(c);
}

//----------------------------------------------------------------------
// The guild packets
//
// Four of these cap at 255 or 256 - tests that a BYTE could never fail,
// so before this change their writers had no working cap at all.
//----------------------------------------------------------------------
TEST(StringWriteCaps, CGAppointSubmasterName)
{
	const CapCase<CGAppointSubmaster> c = {
		+[](CGAppointSubmaster& p, const std::string& s) {
			p.setGuildID(1); p.setName(s); },
		+[](CGAppointSubmaster& p) { return p.getName(); },
		20, 0, 20 };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CGModifyGuildMemberName)
{
	const CapCase<CGModifyGuildMember> c = {
		+[](CGModifyGuildMember& p, const std::string& s) {
			p.setGuildID(1); p.setName(s); p.setGuildMemberRank(0); },
		+[](CGModifyGuildMember& p) { return p.getName(); },
		20, 0, 20 };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CGSelectGuildMemberName)
{
	const CapCase<CGSelectGuildMember> c = {
		+[](CGSelectGuildMember& p, const std::string& s) {
			p.setGuildID(1); p.setName(s); },
		+[](CGSelectGuildMember& p) { return p.getName(); },
		20, 0, 20 };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CGRegistGuildName)
{
	const CapCase<CGRegistGuild> c = {
		+[](CGRegistGuild& p, const std::string& s) {
			p.setGuildName(s); p.setGuildIntro("a"); },
		+[](CGRegistGuild& p) { return p.getGuildName(); },
		30, 0, 30 };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CGRegistGuildIntro)
{
	// The source caps this at 256, which no BYTE ever exceeded.
	const CapCase<CGRegistGuild> c = {
		+[](CGRegistGuild& p, const std::string& s) {
			p.setGuildName("a"); p.setGuildIntro(s); },
		+[](CGRegistGuild& p) { return p.getGuildIntro(); },
		256, 0, 255 };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CGJoinGuildMemberIntro)
{
	const CapCase<CGJoinGuild> c = {
		+[](CGJoinGuild& p, const std::string& s) {
			p.setGuildID(1); p.setGuildMemberRank(0); p.setGuildMemberIntro(s); },
		+[](CGJoinGuild& p) { return p.getGuildMemberIntro(); },
		256, 0, 255 };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CGModifyGuildIntro)
{
	const CapCase<CGModifyGuildIntro> c = {
		+[](CGModifyGuildIntro& p, const std::string& s) {
			p.setGuildID(1); p.setGuildIntro(s); },
		+[](CGModifyGuildIntro& p) { return p.getGuildIntro(); },
		255, 0, 255 };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CGModifyGuildMemberIntro)
{
	const CapCase<CGModifyGuildMemberIntro> c = {
		+[](CGModifyGuildMemberIntro& p, const std::string& s) {
			p.setGuildID(1); p.setGuildMemberIntro(s); },
		+[](CGModifyGuildMemberIntro& p) { return p.getGuildMemberIntro(); },
		255, 0, 255 };

	CheckStringWriteCap(c);
}

//----------------------------------------------------------------------
// The remaining game-server writers
//----------------------------------------------------------------------
TEST(StringWriteCaps, CGUseMessageItemFromInventoryMessage)
{
	// read() is compiled only under __DEBUG_OUTPUT__, so there is no
	// round-trip half here.
	const CapCase<CGUseMessageItemFromInventory> c = {
		+[](CGUseMessageItemFromInventory& p, const std::string& s) {
			p.setObjectID(1); p.setX(0); p.setY(0); p.setMessage(s); },
		+[](CGUseMessageItemFromInventory& p) { return p.getMessage(); },
		128, szObjectID + szCoordInven + szCoordInven, 0 };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CGRequestPowerPointCellNum)
{
	const CapCase<CGRequestPowerPoint> c = {
		+[](CGRequestPowerPoint& p, const std::string& s) { p.setCellNum(s); },
		+[](CGRequestPowerPoint& p) { return p.getCellNum(); },
		12, 0, 12 };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CGConnectPCName)
{
	const CapCase<CGConnect> c = {
		+[](CGConnect& p, const std::string& s) {
			const BYTE mac[6] = { 0, 0, 0, 0, 0, 0 };
			p.setKey(0); p.setPCType(PC_SLAYER);
			p.setMacAddress(mac); p.setPCName(s); },
		+[](CGConnect& p) { return p.getPCName(); },
		20, szDWORD + szPCType, 20 };

	CheckStringWriteCap(c);
}

//----------------------------------------------------------------------
// The login-server writers
//----------------------------------------------------------------------
TEST(StringWriteCaps, CLCreatePCName)
{
	const CapCase<CLCreatePC> c = {
		+[](CLCreatePC& p, const std::string& s) {
			p.setName(s); p.setSlot(SLOT1); p.setSex(MALE);
			p.setSTR(10); p.setDEX(10); p.setINT(10); p.setRace(0); },
		+[](CLCreatePC& p) { return p.getName(); },
		20, 0, 20 };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CLDeletePCName)
{
	const CapCase<CLDeletePC> c = {
		+[](CLDeletePC& p, const std::string& s) {
			p.setName(s); p.setSlot(SLOT1); p.setSSN("a"); },
		+[](CLDeletePC& p) { return p.getName(); },
		20, 0, 20 };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CLDeletePCSSN)
{
	const CapCase<CLDeletePC> c = {
		+[](CLDeletePC& p, const std::string& s) {
			p.setName("a"); p.setSlot(SLOT1); p.setSSN(s); },
		+[](CLDeletePC& p) { return p.getSSN(); },
		18, szBYTE + 1 + szSlot, 18 };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CLLoginID)
{
	const CapCase<CLLogin> c = {
		+[](CLLogin& p, const std::string& s) { p.setID(s); p.setPassword("a"); },
		+[](CLLogin& p) { return p.getID(); },
		30, 0, 30 };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CLLoginPassword)
{
	const CapCase<CLLogin> c = {
		+[](CLLogin& p, const std::string& s) { p.setID("a"); p.setPassword(s); },
		+[](CLLogin& p) { return p.getPassword(); },
		20, szBYTE + 1, 20 };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CLQueryCharacterNameName)
{
	const CapCase<CLQueryCharacterName> c = {
		+[](CLQueryCharacterName& p, const std::string& s) { p.setCharacterName(s); },
		+[](CLQueryCharacterName& p) { return p.getCharacterName(); },
		20, 0, 20 };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CLQueryPlayerIDPlayerID)
{
	const CapCase<CLQueryPlayerID> c = {
		+[](CLQueryPlayerID& p, const std::string& s) { p.setPlayerID(s); },
		+[](CLQueryPlayerID& p) { return p.getPlayerID(); },
		20, 0, 20 };

	CheckStringWriteCap(c);
}

TEST(StringWriteCaps, CLSelectPCPCName)
{
	const CapCase<CLSelectPC> c = {
		+[](CLSelectPC& p, const std::string& s) {
			p.setPCName(s); p.setPCType(PC_SLAYER); },
		+[](CLSelectPC& p) { return p.getPCName(); },
		20, 0, 20 };

	CheckStringWriteCap(c);
}

//----------------------------------------------------------------------
// The one that writes a Datagram rather than a stream
//
// CGPortCheck::write takes a Datagram, which exposes no output offset,
// so only the refusal itself is observable here. Its one caller in
// GameMain.cpp is commented out, so this is a guard rather than a
// reachable path.
//----------------------------------------------------------------------
TEST(StringWriteCaps, CGPortCheckPCName)
{
	const size_t	lengths[] = { 21, 255, 256, 257, 276, 300 };

	for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++)
	{
		CGPortCheck	packet;
		Datagram	datagram;
		bool		bThrew = false;

		datagram.setData(1024);
		packet.setPCName(std::string(lengths[i], 'z'));

		try {
			packet.write(datagram);
		} catch (InvalidProtocolException&) {
			bThrew = true;
		}

		CHECK(bThrew);
	}

	// And a name at the cap is still written.
	{
		CGPortCheck	packet;
		Datagram	datagram;

		datagram.setData(1024);
		packet.setPCName(std::string(20, 'z'));
		packet.write(datagram);

		CHECK_EQ(20, (int)datagram.getData()[0]);
	}
}
