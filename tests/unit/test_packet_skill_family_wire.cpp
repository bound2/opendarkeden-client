//----------------------------------------------------------------------
// test_packet_skill_family_wire.cpp
//----------------------------------------------------------------------
//
// The client skill-activation family - CGSkillToSelf, CGSkillToObject,
// CGSkillToTile and CGSkillToNamed - after its migration onto the C++20
// std::span and readWire/writeWire entry points (the second slice of
// the priority-3 and priority-4 items in
// docs/cpp17-cpp20-compatibility-assessment-2026-09-04.md).
//
// The wire BYTES of these four are pinned in test_packet_goldens.cpp,
// which is the contract: three of them against goldens recorded for all
// six encrypt codes before this work started, the fourth against a
// golden recorded in the commit before the migration. This file pins
// what a byte golden cannot see:
//
//   - every FIELD survives a write/read cycle under every encrypt code,
//     checked through the packet's own getters, and the body is exactly
//     getPacketSize() long, so a width change on either side shows up.
//     Field ORDER is the goldens' job: two same-width fields swapped in
//     both directions read back symmetric here and pass, and only the
//     byte pin in test_packet_goldens.cpp catches that;
//   - a truncated body is refused with InsufficientDataException, the
//     same exception the raw pointer/length read threw, because both
//     spellings now reach the same bounded core (failUnderflow in
//     SocketInputStream::read(std::span<char>));
//   - CGSkillToNamed's length byte and name body always agree, which is
//     the property the bounded write exists to hold.
//
// Only the plain branch of read()/write() was migrated; the encrypter
// branch (SHUFFLE_STATEMENT_*/readEncrypt/writeEncrypt) is untouched.
// The round-trips therefore run over every encrypt code, so a change
// that reached only one of the two branches shows up here.
//
// Compiled with the packetwire defines (tests/CMakeLists.txt).
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "Exception.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"

#include "Cpackets/CGSkillToNamed.h"
#include "Cpackets/CGSkillToObject.h"
#include "Cpackets/CGSkillToSelf.h"
#include "Cpackets/CGSkillToTile.h"

#include <string>
#include <vector>

namespace {

const uchar	kSkillEncryptCodes[] = { 0, 1, 2, 3, 4, 5 };
const size_t	kSkillEncryptCodeCount =
	sizeof(kSkillEncryptCodes) / sizeof(kSkillEncryptCodes[0]);

struct SkillOutFixture
{
	Socket				m_Socket;
	SocketEncryptOutputStream	m_Stream;

	SkillOutFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket)
	{
	}
};

struct SkillInFixture
{
	Socket				m_Socket;
	SocketEncryptInputStream	m_Stream;

	SkillInFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket, 4096)
	{
	}
};

std::vector<unsigned char>	SkillBody(const Packet& packet, uchar code)
{
	SkillOutFixture f;
	f.m_Stream.setEncryptCode(code);
	packet.write(f.m_Stream);
	return SocketOutputStreamTestAccess::Bytes(f.m_Stream);
}

template <class PacketT>
void	SkillReadBody(PacketT& dst, const std::vector<unsigned char>& body, uchar code)
{
	SkillInFixture f;
	f.m_Stream.setEncryptCode(code);
	SocketInputStreamTestAccess::Preload(f.m_Stream, body.empty() ? NULL : &body[0],
					     (unsigned int)body.size());
	dst.read(f.m_Stream);
	CHECK(f.m_Stream.isEmpty());
}

// True when read() refuses `body` by running out of bytes. The bounded
// span core and the pointer/length adapter share failUnderflow(), so the
// migrated packets must still raise exactly this.
template <class PacketT>
bool	UnderflowsOn(const std::vector<unsigned char>& body, uchar code)
{
	try {
		PacketT dst;
		SkillInFixture f;
		f.m_Stream.setEncryptCode(code);
		SocketInputStreamTestAccess::Preload(f.m_Stream,
			body.empty() ? NULL : &body[0], (unsigned int)body.size());
		dst.read(f.m_Stream);
	} catch (InsufficientDataException&) {
		return true;
	} catch (Throwable&) {
		return false;
	}
	return false;
}

// The body one byte short of what write() emitted, for `code`.
std::vector<unsigned char>	TruncatedBody(const Packet& packet, uchar code)
{
	std::vector<unsigned char> body = SkillBody(packet, code);
	CHECK(body.size() > 1);
	if (body.size() > 1)
		body.pop_back();
	return body;
}

// Fixture values distinct from test_packet_goldens.cpp's, so a getter
// wired to the wrong member cannot pass by coincidence with that file.
void	FillSelf(CGSkillToSelf& p)
{
	p.setSkillType(0x81C2);
	p.setCEffectID(0x93D4);
}

void	FillObject(CGSkillToObject& p)
{
	p.setSkillType(0x82C3);
	p.setCEffectID(0x94D5);
	p.setTargetObjectID(0xA5E6F708);
}

void	FillTile(CGSkillToTile& p)
{
	p.setSkillType(0x83C4);
	p.setCEffectID(0x95D6);
	p.setX(0xB7);
	p.setY(0xC8);
}

void	FillNamed(CGSkillToNamed& p)
{
	p.setSkillType(0x84C5);
	p.setCEffectID(0x96D7);
	p.setTargetName("Nosferatu");
}

} // namespace

//----------------------------------------------------------------------
// Field-level round-trips over every encrypt code
//----------------------------------------------------------------------
TEST(CGSkillToSelf, WireMigrationPreservesEveryFieldUnderEveryCode)
{
	for (size_t i = 0; i < kSkillEncryptCodeCount; i++)
	{
		CGSkillToSelf src, dst;
		FillSelf(src);
		const std::vector<unsigned char> body =
			SkillBody(src, kSkillEncryptCodes[i]);
		CHECK_EQ(src.getPacketSize(), body.size());
		SkillReadBody(dst, body, kSkillEncryptCodes[i]);
		CHECK_EQ(src.getSkillType(), dst.getSkillType());
		CHECK_EQ(src.getCEffectID(), dst.getCEffectID());
	}
}

TEST(CGSkillToObject, WireMigrationPreservesEveryFieldUnderEveryCode)
{
	for (size_t i = 0; i < kSkillEncryptCodeCount; i++)
	{
		CGSkillToObject src, dst;
		FillObject(src);
		const std::vector<unsigned char> body =
			SkillBody(src, kSkillEncryptCodes[i]);
		CHECK_EQ(src.getPacketSize(), body.size());
		SkillReadBody(dst, body, kSkillEncryptCodes[i]);
		CHECK_EQ(src.getSkillType(), dst.getSkillType());
		CHECK_EQ(src.getCEffectID(), dst.getCEffectID());
		CHECK_EQ(src.getTargetObjectID(), dst.getTargetObjectID());
	}
}

TEST(CGSkillToTile, WireMigrationPreservesEveryFieldUnderEveryCode)
{
	for (size_t i = 0; i < kSkillEncryptCodeCount; i++)
	{
		CGSkillToTile src, dst;
		FillTile(src);
		const std::vector<unsigned char> body =
			SkillBody(src, kSkillEncryptCodes[i]);
		CHECK_EQ(src.getPacketSize(), body.size());
		SkillReadBody(dst, body, kSkillEncryptCodes[i]);
		CHECK_EQ(src.getSkillType(), dst.getSkillType());
		CHECK_EQ(src.getCEffectID(), dst.getCEffectID());
		CHECK_EQ(src.getX(), dst.getX());
		CHECK_EQ(src.getY(), dst.getY());
	}
}

// CGSkillToNamed never reaches the encrypter, so its bytes and its
// parse must be identical under every code - the same property
// EncrypterFree() asserts for it in test_packet_goldens.cpp, restated
// here at field level because this file is where its span write lives.
TEST(CGSkillToNamed, WireMigrationPreservesEveryFieldUnderEveryCode)
{
	CGSkillToNamed reference;
	FillNamed(reference);
	const std::vector<unsigned char> plain = SkillBody(reference, 0);

	for (size_t i = 0; i < kSkillEncryptCodeCount; i++)
	{
		CGSkillToNamed src, dst;
		FillNamed(src);
		const std::vector<unsigned char> body =
			SkillBody(src, kSkillEncryptCodes[i]);
		CHECK(body == plain);
		CHECK_EQ(src.getPacketSize(), body.size());
		SkillReadBody(dst, body, kSkillEncryptCodes[i]);
		CHECK_EQ(src.getSkillType(), dst.getSkillType());
		CHECK_EQ(src.getCEffectID(), dst.getCEffectID());
		CHECK(src.getTargetName() == dst.getTargetName());
	}
}

//----------------------------------------------------------------------
// Truncated bodies: the migrated reads must still underflow
//----------------------------------------------------------------------
TEST(SkillPacketFamily, TruncatedBodiesRaiseInsufficientData)
{
	CGSkillToSelf self;
	FillSelf(self);
	CHECK(UnderflowsOn<CGSkillToSelf>(TruncatedBody(self, 0), 0));

	CGSkillToObject object;
	FillObject(object);
	CHECK(UnderflowsOn<CGSkillToObject>(TruncatedBody(object, 0), 0));

	CGSkillToTile tile;
	FillTile(tile);
	CHECK(UnderflowsOn<CGSkillToTile>(TruncatedBody(tile, 0), 0));

	// Two truncations for the named packet: the first lands inside the
	// name body (the std::string read the length byte drives), the second
	// inside the scalar header itself (the readWire path).
	CGSkillToNamed named;
	FillNamed(named);
	CHECK(UnderflowsOn<CGSkillToNamed>(TruncatedBody(named, 0), 0));

	std::vector<unsigned char> headerOnly = SkillBody(named, 0);
	headerOnly.resize(3);	// SkillType, then half of CEffectID
	CHECK(UnderflowsOn<CGSkillToNamed>(headerOnly, 0));
}

//----------------------------------------------------------------------
// CGSkillToNamed's bounded name write
//----------------------------------------------------------------------

// A name at the 20-byte cap survives the round trip, and the body is
// exactly the five header bytes - two, two and the length - plus those
// twenty.
TEST(CGSkillToNamed, NameAtTheCapRoundTrips)
{
	CGSkillToNamed src, dst;
	FillNamed(src);
	src.setTargetName(std::string(20, 'z'));

	const std::vector<unsigned char> body = SkillBody(src, 0);
	CHECK_EQ((size_t)(szSkillType + szCEffectID + szBYTE + 20), body.size());
	CHECK_EQ(20, body[szSkillType + szCEffectID]);

	SkillReadBody(dst, body, 0);
	CHECK(src.getTargetName() == dst.getTargetName());
	CHECK_EQ((size_t)20, dst.getTargetName().size());
}

// write() refuses anything over the cap instead of emitting a body its
// own length byte does not describe. 276 is the case the BYTE narrowing
// used to hide: (BYTE)276 is 20, which passed the old cap check while
// write() went on to emit all 276 characters.
TEST(CGSkillToNamed, WriteRefusesNamesLongerThanTheCap)
{
	const size_t lengths[] = { 21, 255, 256, 276, 300 };

	for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++)
	{
		CGSkillToNamed packet;
		FillNamed(packet);
		packet.setTargetName(std::string(lengths[i], 'z'));

		bool bThrew = false;
		try {
			SkillOutFixture f;
			packet.write(f.m_Stream);
		} catch (InvalidProtocolException&) {
			bThrew = true;
		}
		CHECK(bThrew);
	}
}

// An empty name is refused on both sides, as it always was: write()
// cannot express a zero length byte the reader accepts.
TEST(CGSkillToNamed, EmptyNameIsRefusedOnBothSides)
{
	CGSkillToNamed packet;
	FillNamed(packet);
	packet.setTargetName("");

	bool bWriteThrew = false;
	try {
		SkillOutFixture f;
		packet.write(f.m_Stream);
	} catch (InvalidProtocolException&) {
		bWriteThrew = true;
	}
	CHECK(bWriteThrew);

	// The same shape arriving from a peer: header, then a zero length.
	std::vector<unsigned char> body;
	body.push_back(0xC5);
	body.push_back(0x84);
	body.push_back(0xD7);
	body.push_back(0x96);
	body.push_back(0x00);

	bool bReadThrew = false;
	try {
		CGSkillToNamed dst;
		SkillInFixture f;
		SocketInputStreamTestAccess::Preload(f.m_Stream, &body[0],
			(unsigned int)body.size());
		dst.read(f.m_Stream);
	} catch (InvalidProtocolException&) {
		bReadThrew = true;
	}
	CHECK(bReadThrew);
}
