//----------------------------------------------------------------------
// test_packet_combat_family_wire.cpp
//----------------------------------------------------------------------
//
// The last four packets in the packet directories that read and wrote
// their scalars through a (char*)&field, size cast - CGAddZoneToMouse,
// CGBloodDrain, GCAttack and GCGetDamage - at field level, after their
// migration to readWire/writeWire: every field survives a write/read
// cycle under every encrypt code, and a truncated body underflows with
// the same InsufficientDataException the pointer/length read raised.
//
// The wire bytes and the field ORDER are pinned in
// test_packet_goldens.cpp, not here.
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

#include "Cpackets/CGAddZoneToMouse.h"
#include "Cpackets/CGBloodDrain.h"
#include "Gpackets/GCAttack.h"
#include "Gpackets/GCGetDamage.h"

#include <vector>

namespace {

const uchar	kCombatEncryptCodes[] = { 0, 1, 2, 3, 4, 5 };
const size_t	kCombatEncryptCodeCount =
	sizeof(kCombatEncryptCodes) / sizeof(kCombatEncryptCodes[0]);

struct CombatOutFixture
{
	Socket				m_Socket;
	SocketEncryptOutputStream	m_Stream;

	CombatOutFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket)
	{
	}
};

struct CombatInFixture
{
	Socket				m_Socket;
	SocketEncryptInputStream	m_Stream;

	CombatInFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket, 4096)
	{
	}
};

std::vector<unsigned char>	CombatBody(const Packet& packet, uchar code)
{
	CombatOutFixture f;
	f.m_Stream.setEncryptCode(code);
	packet.write(f.m_Stream);
	return SocketOutputStreamTestAccess::Bytes(f.m_Stream);
}

template <class PacketT>
void	CombatReadBody(PacketT& dst, const std::vector<unsigned char>& body, uchar code)
{
	CombatInFixture f;
	f.m_Stream.setEncryptCode(code);
	SocketInputStreamTestAccess::Preload(f.m_Stream, body.empty() ? NULL : &body[0],
					     (unsigned int)body.size());
	dst.read(f.m_Stream);
	CHECK(f.m_Stream.isEmpty());
}

// True when read() refuses `body` by running out of bytes.
template <class PacketT>
bool	UnderflowsOn(const std::vector<unsigned char>& body, uchar code)
{
	try {
		PacketT dst;
		CombatInFixture f;
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

// Every body from one byte short of what write() emitted down to
// empty must underflow: a staged read that consumed a partial scalar
// would surface as a parse rather than an exception.
template <class PacketT>
bool	EveryTruncationUnderflows(const Packet& packet, uchar code)
{
	std::vector<unsigned char> body = CombatBody(packet, code);
	while (!body.empty())
	{
		body.pop_back();
		if (!UnderflowsOn<PacketT>(body, code))
			return false;
	}
	return true;
}

// Fixture values: every field of each packet differs from the value
// test_packet_goldens.cpp's fixture gives that same packet, so a
// parse that silently kept a golden's byte would be caught here.
void	FillAddZoneToMouse(CGAddZoneToMouse& p)
{
	p.setObjectID(0x91A2B3C4);
	p.setZoneX(0x85);
	p.setZoneY(0x96);
}

void	FillBloodDrain(CGBloodDrain& p)
{
	p.setObjectID(0x92A3B4C5);
}

void	FillAttack(GCAttack& p)
{
	p.setObjectID(0x93A4B5C6);
	p.setX(0x87);
	p.setY(0x98);
	p.setDir(0xA9);
}

void	FillGetDamage(GCGetDamage& p)
{
	p.setObjectID(0x94A5B6C7);
	p.setDamage(0x88D9);
}

} // namespace

//----------------------------------------------------------------------
// Field-level round-trips over every encrypt code
//----------------------------------------------------------------------

// CGAddZoneToMouse is the family's one encrypter user: codes 1..5 take
// the SHUFFLE_STATEMENT_3 branch, code 0 the migrated plain one.
TEST(CGAddZoneToMouse, WireMigrationPreservesEveryFieldUnderEveryCode)
{
	for (size_t i = 0; i < kCombatEncryptCodeCount; i++)
	{
		CGAddZoneToMouse src, dst;
		FillAddZoneToMouse(src);
		const std::vector<unsigned char> body =
			CombatBody(src, kCombatEncryptCodes[i]);
		CHECK_EQ(src.getPacketSize(), body.size());
		CombatReadBody(dst, body, kCombatEncryptCodes[i]);
		CHECK_EQ(src.getObjectID(), dst.getObjectID());
		CHECK_EQ(src.getZoneX(), dst.getZoneX());
		CHECK_EQ(src.getZoneY(), dst.getZoneY());
	}
}

// The other three never reach the encrypter, so their bytes and their
// parse must be identical under every code.
TEST(CGBloodDrain, WireMigrationPreservesEveryFieldUnderEveryCode)
{
	CGBloodDrain reference;
	FillBloodDrain(reference);
	const std::vector<unsigned char> plain = CombatBody(reference, 0);
	CHECK_EQ((size_t)szObjectID, plain.size());

	for (size_t i = 0; i < kCombatEncryptCodeCount; i++)
	{
		CGBloodDrain src, dst;
		FillBloodDrain(src);
		const std::vector<unsigned char> body =
			CombatBody(src, kCombatEncryptCodes[i]);
		CHECK(body == plain);
		CHECK_EQ(src.getPacketSize(), body.size());
		CombatReadBody(dst, body, kCombatEncryptCodes[i]);
		CHECK_EQ(src.getObjectID(), dst.getObjectID());
	}
}

TEST(GCAttack, WireMigrationPreservesEveryFieldUnderEveryCode)
{
	GCAttack reference;
	FillAttack(reference);
	const std::vector<unsigned char> plain = CombatBody(reference, 0);
	CHECK_EQ((size_t)(szObjectID + szCoord + szCoord + szDir), plain.size());

	for (size_t i = 0; i < kCombatEncryptCodeCount; i++)
	{
		GCAttack src, dst;
		FillAttack(src);
		const std::vector<unsigned char> body =
			CombatBody(src, kCombatEncryptCodes[i]);
		CHECK(body == plain);
		CHECK_EQ(src.getPacketSize(), body.size());
		CombatReadBody(dst, body, kCombatEncryptCodes[i]);
		CHECK_EQ(src.getObjectID(), dst.getObjectID());
		CHECK_EQ(src.getX(), dst.getX());
		CHECK_EQ(src.getY(), dst.getY());
		CHECK_EQ(src.getDir(), dst.getDir());
	}
}

TEST(GCGetDamage, WireMigrationPreservesEveryFieldUnderEveryCode)
{
	GCGetDamage reference;
	FillGetDamage(reference);
	const std::vector<unsigned char> plain = CombatBody(reference, 0);
	CHECK_EQ((size_t)(szObjectID + szWORD), plain.size());

	for (size_t i = 0; i < kCombatEncryptCodeCount; i++)
	{
		GCGetDamage src, dst;
		FillGetDamage(src);
		const std::vector<unsigned char> body =
			CombatBody(src, kCombatEncryptCodes[i]);
		CHECK(body == plain);
		CHECK_EQ(src.getPacketSize(), body.size());
		CombatReadBody(dst, body, kCombatEncryptCodes[i]);
		CHECK_EQ(src.getObjectID(), dst.getObjectID());
		CHECK_EQ(src.getDamage(), dst.getDamage());
	}
}

//----------------------------------------------------------------------
// The full ObjectID width goes on the wire: a value with every byte
// above 0x7F and the top bit set survives the uint32_t staging in
// both directions, so the cast neither sign-extends nor narrows.
//----------------------------------------------------------------------
TEST(CombatPacketFamily, ObjectIDStagingKeepsAllThirtyTwoBits)
{
	const ObjectID_t ids[] = { 0x00000000u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu, 0xFEDCBA98u };

	for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++)
	{
		GCAttack src, dst;
		FillAttack(src);
		src.setObjectID(ids[i]);
		const std::vector<unsigned char> body = CombatBody(src, 0);
		CHECK_EQ((size_t)(szObjectID + szCoord + szCoord + szDir), body.size());
		CHECK_EQ((unsigned char)(ids[i] & 0xFF), body[0]);
		CHECK_EQ((unsigned char)((ids[i] >> 8) & 0xFF), body[1]);
		CHECK_EQ((unsigned char)((ids[i] >> 16) & 0xFF), body[2]);
		CHECK_EQ((unsigned char)((ids[i] >> 24) & 0xFF), body[3]);
		CombatReadBody(dst, body, 0);
		CHECK_EQ(ids[i], dst.getObjectID());
	}
}

//----------------------------------------------------------------------
// Truncated bodies: the migrated reads must still underflow, at every
// cut, on every code
//----------------------------------------------------------------------
TEST(CombatPacketFamily, EveryTruncatedBodyRaisesInsufficientData)
{
	for (size_t i = 0; i < kCombatEncryptCodeCount; i++)
	{
		const uchar code = kCombatEncryptCodes[i];

		CGAddZoneToMouse addZoneToMouse;
		FillAddZoneToMouse(addZoneToMouse);
		CHECK(EveryTruncationUnderflows<CGAddZoneToMouse>(addZoneToMouse, code));

		CGBloodDrain bloodDrain;
		FillBloodDrain(bloodDrain);
		CHECK(EveryTruncationUnderflows<CGBloodDrain>(bloodDrain, code));

		GCAttack attack;
		FillAttack(attack);
		CHECK(EveryTruncationUnderflows<GCAttack>(attack, code));

		GCGetDamage getDamage;
		FillGetDamage(getDamage);
		CHECK(EveryTruncationUnderflows<GCGetDamage>(getDamage, code));
	}
}
