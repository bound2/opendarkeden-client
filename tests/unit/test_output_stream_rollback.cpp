//----------------------------------------------------------------------
// test_output_stream_rollback.cpp
//----------------------------------------------------------------------
//
// SocketOutputStream::write(const Packet*) frames a packet: it puts the
// packet id, getPacketSize() and the sequence byte into the ring, bumps
// the sequence counter, and only then asks the packet to write its body.
//
// The body write can throw. Every bounded write() in the tree does -
// CGSkillToNamed::write() refuses a target name over its 20-byte cap
// (595e994), and the stream's own span core throws on an oversized
// span - and the header is already in the ring when it happens. Nothing
// used to roll it back, so the ring kept a header announcing a body that
// never followed and the peer parsed the NEXT packet's bytes as that
// body; the sequence counter had also advanced for a packet that never
// went out. That defect is what this file pins.
//
// What is asserted is the observable contract at the ring, not a crash:
//
//   - a failed write leaves Bytes() exactly as it found them, whether
//     the ring was empty, already held a framed packet, or had to be
//     resized by the doomed body itself;
//   - the exception still reaches the caller, with its own type;
//   - the sequence byte the NEXT packet gets is the one it would have
//     had if the failed write had never been attempted;
//   - the non-throwing path is byte for byte what it always was: id and
//     size in host (little-endian) order at their declared widths, then
//     the sequence byte, then the body. tests/golden/*.hex owns the body
//     bytes; this pins the frame around them.
//
// SocketEncryptOutputStream inherits write(const Packet*) unchanged - it
// overrides only the writeEncrypt scalar family - so the same cases run
// over it, which is the proof that the one fix covers both streams. The
// real packet used as the well-behaved witness, CGSkillToSelf, is only
// ever framed on an encrypt stream: its write() Asserts on the
// dynamic_cast to SocketEncryptOutputStream, so a plain stream is not a
// shape any real packet is written to. The plain-stream cases therefore
// use the local packet below, which writes through the base interface.
//
// That local packet is never registered with a factory, so the factory
// tables, tests/wire-layout.txt and the wire goldens are all untouched
// by it, and the number of body bytes it emits before throwing can be
// dialled to reach the resize path.
//
// Compiled with the packetwire defines (tests/CMakeLists.txt).
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "Exception.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketEncryptOutputStream.h"
#include "Packet.h"

#include "Cpackets/CGSkillToSelf.h"

#include <string>
#include <vector>

namespace {

//----------------------------------------------------------------------
// Streams over a never-used socket (see packet_stream_access.h). Both
// take an explicit ring size so a test can force write() to resize.
//----------------------------------------------------------------------
struct PlainOutFixture
{
	Socket			m_Socket;
	SocketOutputStream	m_Stream;

	PlainOutFixture(uint BufferLen = DefaultSocketOutputBufferSize)
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket, BufferLen)
	{
	}
};

struct EncryptOutFixture
{
	Socket				m_Socket;
	SocketEncryptOutputStream	m_Stream;

	EncryptOutFixture(uint BufferLen = DefaultSocketEncryptOutputBufferSize)
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket, BufferLen)
	{
	}
};

//----------------------------------------------------------------------
//
// class TestBodyPacket
//
// A packet whose body write emits m_Prefix bytes and then either returns
// or refuses. getPacketSize() is whatever the constructor was told,
// exactly as a real packet's would be: the header goes out before the
// body decides whether it can honour it.
//
//----------------------------------------------------------------------
class TestBodyPacket : public Packet {

public :

	TestBodyPacket ( uint Prefix , PacketSize_t Claimed , bool bThrow )
	: m_Prefix(Prefix), m_Claimed(Claimed), m_bThrow(bThrow) {}

	// No packet ever reads one of these; the frame is the whole point.
	void read ( SocketInputStream & /*iStream*/ ) { throw UnsupportedError(); }

	void write ( SocketOutputStream & oStream ) const
	{
		if ( m_Prefix > 0 )
		{
			const std::string prefix( m_Prefix , 'P' );
			oStream.write( prefix.data() , (uint)prefix.size() );
		}
		if ( m_bThrow )
			throw InvalidProtocolException("test packet refuses to write its body");
	}

	// A real id, so the framed bytes look like the real thing. Nothing
	// dispatches on it here.
	PacketID_t   getPacketID () const { return PACKET_CG_SKILL_TO_SELF; }
	PacketSize_t getPacketSize () const { return m_Claimed; }

	// The same single condition Packet.h declares these under, so the two
	// cannot be emitted twice when both macros happen to be set.
	#if !defined(__GAME_CLIENT__) || defined(__DEBUG_OUTPUT__)
		std::string getPacketName () const { return "TestBodyPacket"; }
		std::string toString () const { return "TestBodyPacket"; }
	#endif

private :

	// how many body bytes reach the ring before the packet returns
	uint		m_Prefix;

	// what the header claims the body will be
	PacketSize_t	m_Claimed;

	// whether the body refuses after those bytes
	bool		m_bThrow;
};

//----------------------------------------------------------------------
// Fixture values distinct from the other wire tests', so a header field
// read out of the wrong member cannot pass by coincidence.
//----------------------------------------------------------------------
void	FillSelf(CGSkillToSelf& p)
{
	p.setSkillType(0x71B2);
	p.setCEffectID(0x83C4);
}

// The packet's own body, with no frame around it. CGSkillToSelf needs an
// encrypt stream even at code 0 (see the file header).
std::vector<unsigned char>	Body(const Packet& packet, uchar code)
{
	EncryptOutFixture f;
	f.m_Stream.setEncryptCode(code);
	packet.write(f.m_Stream);
	return SocketOutputStreamTestAccess::Bytes(f.m_Stream);
}

// Writes `packet` through the framing entry point and reports whether it
// refused with InvalidProtocolException. The bytes are left in the
// stream for the caller to inspect.
bool	FramesAndThrows(SocketOutputStream& stream, const Packet& packet)
{
	try {
		stream.write( &packet );
	} catch ( InvalidProtocolException & ) {
		return true;
	} catch ( Throwable & ) {
		return false;
	}
	return false;
}

} // namespace

//----------------------------------------------------------------------
// A failed body write leaves nothing behind
//----------------------------------------------------------------------

// Nothing at all was in the ring, so nothing may be in it afterwards -
// not the id, not the size, not the sequence byte. Both the case where
// the body writes no bytes before refusing and the case where it writes
// some, which is the shape a bounded field write has.
TEST(SocketOutputStream, FailedBodyWriteLeavesTheRingEmpty)
{
	const uint prefixes[] = { 0, 1, 5, 64 };

	for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++)
	{
		PlainOutFixture f;
		TestBodyPacket packet(prefixes[i], 32, true);

		CHECK(FramesAndThrows(f.m_Stream, packet));
		CHECK_EQ((size_t)0, SocketOutputStreamTestAccess::Bytes(f.m_Stream).size());
		CHECK_EQ((long long)0, (long long)f.m_Stream.length());
		CHECK(f.m_Stream.isEmpty());
	}
}

// A packet that was framed successfully before the failure must survive
// it byte for byte: the rollback may only drop what the failed write
// itself put in.
TEST(SocketOutputStream, FailedWriteLeavesAnEarlierPacketIntact)
{
	PlainOutFixture f;

	TestBodyPacket good(6, 6, false);
	f.m_Stream.write( &good );

	const std::vector<unsigned char> before =
		SocketOutputStreamTestAccess::Bytes(f.m_Stream);
	CHECK_EQ((long long)(szPacketHeader + 6), (long long)before.size());

	TestBodyPacket bad(7, 32, true);
	CHECK(FramesAndThrows(f.m_Stream, bad));

	CHECK(SocketOutputStreamTestAccess::Bytes(f.m_Stream) == before);
	CHECK_EQ((long long)before.size(), (long long)f.m_Stream.length());
}

// The doomed body is large enough to make write() resize the ring, which
// reallocates it and moves the retained bytes to offset zero. The
// rollback therefore cannot simply restore the old tail index; it has to
// restore the amount of data the ring held, from wherever the head now
// is.
TEST(SocketOutputStream, FailedWriteRollsBackAcrossARingResize)
{
	PlainOutFixture f(64);

	TestBodyPacket good(6, 6, false);
	f.m_Stream.write( &good );

	const std::vector<unsigned char> before =
		SocketOutputStreamTestAccess::Bytes(f.m_Stream);
	const int capacityBefore = f.m_Stream.capacity();

	TestBodyPacket bad(4096, 4096, true);
	CHECK(FramesAndThrows(f.m_Stream, bad));

	// The resize really happened, or this test proves nothing.
	CHECK(f.m_Stream.capacity() > capacityBefore);
	CHECK(SocketOutputStreamTestAccess::Bytes(f.m_Stream) == before);
	CHECK_EQ((long long)before.size(), (long long)f.m_Stream.length());
}

// The head is off zero and the live bytes wrap around the end of the
// ring, which is the state a partial flush() leaves behind. This is the
// case that separates "restore the saved length from the head" from
// "restore the saved tail index": with the ring wrapped and the doomed
// body forcing a resize, the retained bytes move to offset zero and the
// old tail index (0, at the wrap) names the wrong place. Without the
// resize the two spellings agree, and that variant is here so the
// wrapped state itself is shown to survive a plain rollback too.
TEST(SocketOutputStream, FailedWriteRollsBackWhenTheRingHasWrapped)
{
	for (int iResize = 0; iResize < 2; iResize++)
	{
		PlainOutFixture f(64);

		// 27 bytes in, 20 consumed: head 20, 7 live.
		TestBodyPacket first(20, 20, false);
		f.m_Stream.write( &first );
		SocketOutputStreamTestAccess::Consume(f.m_Stream, 20);

		// 37 more: tail runs to 64 and wraps to 0, head stays at 20.
		TestBodyPacket second(30, 30, false);
		f.m_Stream.write( &second );

		const std::vector<unsigned char> before =
			SocketOutputStreamTestAccess::Bytes(f.m_Stream);
		CHECK_EQ(44, before.size());
		CHECK_EQ(44u, f.m_Stream.length());
		CHECK_EQ(false, f.m_Stream.isEmpty());
		const int capacityBefore = f.m_Stream.capacity();

		TestBodyPacket bad(iResize ? 4096 : 3, 32, true);
		CHECK(FramesAndThrows(f.m_Stream, bad));

		if (iResize)
			CHECK(f.m_Stream.capacity() > capacityBefore);
		else
			CHECK_EQ(capacityBefore, f.m_Stream.capacity());

		CHECK(SocketOutputStreamTestAccess::Bytes(f.m_Stream) == before);
		CHECK_EQ(44u, f.m_Stream.length());

		// And the ring still frames the next packet correctly from there.
		TestBodyPacket next(5, 5, false);
		f.m_Stream.write( &next );
		CHECK_EQ(44u + 7u + 5u, f.m_Stream.length());
	}
}

//----------------------------------------------------------------------
// The sequence counter
//----------------------------------------------------------------------

// The sequence byte the peer sees must count packets that were actually
// framed. A failed write on a fresh stream may not consume sequence 0.
TEST(SocketOutputStream, FailedWriteDoesNotConsumeTheFirstSequenceNumber)
{
	TestBodyPacket good(6, 6, false);

	PlainOutFixture reference;
	reference.m_Stream.write( &good );
	const std::vector<unsigned char> expected =
		SocketOutputStreamTestAccess::Bytes(reference.m_Stream);

	PlainOutFixture f;
	TestBodyPacket bad(3, 16, true);
	CHECK(FramesAndThrows(f.m_Stream, bad));
	f.m_Stream.write( &good );

	CHECK(SocketOutputStreamTestAccess::Bytes(f.m_Stream) == expected);
}

// The same property in the middle of a session: good, failed, good must
// be byte-identical to good, good.
TEST(SocketOutputStream, AFailedWriteIsInvisibleToTheNextPacket)
{
	TestBodyPacket good(6, 6, false);

	PlainOutFixture reference;
	reference.m_Stream.write( &good );
	reference.m_Stream.write( &good );
	const std::vector<unsigned char> expected =
		SocketOutputStreamTestAccess::Bytes(reference.m_Stream);

	PlainOutFixture f;
	f.m_Stream.write( &good );
	TestBodyPacket bad(11, 48, true);
	CHECK(FramesAndThrows(f.m_Stream, bad));
	f.m_Stream.write( &good );

	CHECK(SocketOutputStreamTestAccess::Bytes(f.m_Stream) == expected);
}

//----------------------------------------------------------------------
// The non-throwing path is unchanged
//----------------------------------------------------------------------

// The frame around a body: the id and the size in host order at their
// declared widths, then the sequence byte, then the body the packet's
// own write() produces. The body bytes themselves are pinned by
// tests/golden/*.hex; nothing here may move them.
TEST(SocketOutputStream, FramingIsUnchangedOnTheNonThrowingPath)
{
	CGSkillToSelf packet;
	FillSelf(packet);

	const PacketID_t   id   = packet.getPacketID();
	const PacketSize_t size = packet.getPacketSize();

	std::vector<unsigned char> expected;
	for (uint i = 0; i < szPacketID; i++)
		expected.push_back((unsigned char)((id >> (8 * i)) & 0xFF));
	for (uint i = 0; i < szPacketSize; i++)
		expected.push_back((unsigned char)((size >> (8 * i)) & 0xFF));
	expected.push_back(0x00);	// first sequence byte on a fresh stream

	const std::vector<unsigned char> body = Body(packet, 0);
	CHECK_EQ((long long)size, (long long)body.size());
	expected.insert(expected.end(), body.begin(), body.end());

	EncryptOutFixture f;
	f.m_Stream.setEncryptCode(0);
	f.m_Stream.write( &packet );
	CHECK(SocketOutputStreamTestAccess::Bytes(f.m_Stream) == expected);
	CHECK_EQ((long long)(szPacketHeader + size), (long long)f.m_Stream.length());

	// The second packet on the same stream differs only in its sequence
	// byte, which is the counter the rollback has to leave alone.
	f.m_Stream.write( &packet );
	const std::vector<unsigned char> both =
		SocketOutputStreamTestAccess::Bytes(f.m_Stream);
	CHECK_EQ((long long)(2 * (szPacketHeader + size)), (long long)both.size());
	CHECK_EQ((long long)1, (long long)both[expected.size() + szPacketID + szPacketSize]);
}

//----------------------------------------------------------------------
// The encrypt stream inherits the same framing
//----------------------------------------------------------------------

// SocketEncryptOutputStream overrides only the writeEncrypt scalar
// family, so write(const Packet*) - and therefore the rollback - is the
// base's. Restated here over a real packet at a real encrypt code, so a
// future override cannot quietly lose it.
TEST(SocketEncryptOutputStream, FailedWriteRollsTheFrameBack)
{
	CGSkillToSelf good;
	FillSelf(good);

	EncryptOutFixture reference;
	reference.m_Stream.setEncryptCode(3);
	reference.m_Stream.write( &good );
	reference.m_Stream.write( &good );
	const std::vector<unsigned char> expected =
		SocketOutputStreamTestAccess::Bytes(reference.m_Stream);

	EncryptOutFixture f;
	f.m_Stream.setEncryptCode(3);
	f.m_Stream.write( &good );

	const std::vector<unsigned char> afterFirst =
		SocketOutputStreamTestAccess::Bytes(f.m_Stream);
	CHECK(afterFirst.size() > 0);

	TestBodyPacket bad(9, 24, true);
	CHECK(FramesAndThrows(f.m_Stream, bad));
	CHECK(SocketOutputStreamTestAccess::Bytes(f.m_Stream) == afterFirst);

	f.m_Stream.write( &good );
	CHECK(SocketOutputStreamTestAccess::Bytes(f.m_Stream) == expected);
}
