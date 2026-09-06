//----------------------------------------------------------------------
// test_output_stream_flush.cpp
//----------------------------------------------------------------------
//
// SocketOutputStream::flush() hands the output ring to the socket. The
// socket does not have to take all of it: send() returns the number of
// bytes it accepted, which may be fewer than it was offered, and
// SocketAPI::send_ex turns a would-block into a NonBlockingIOException
// only when the underlying send() returned SOCKET_ERROR - that is, when
// nothing at all went out on that call. So the count of bytes actually
// sent is always known: the loops add every returned count into m_Head,
// and the call that throws transferred nothing.
//
// flush() nevertheless caught that exception, swallowed it, and then ran
// `m_Head = m_Tail = 0;` unconditionally, which threw away every byte the
// socket had not taken. The peer got a frame cut mid-packet and the next
// flush started a fresh packet inside it. That defect is what this file
// pins.
//
// What is asserted is the observable contract at the ring and at the
// peer, not a crash:
//
//   - after a partial send the ring still holds exactly the bytes the
//     socket did not take, and length()/isEmpty() agree;
//   - the next flush() sends that remainder first, so the peer's byte
//     stream, concatenated across flushes, is the queued one with
//     nothing dropped and nothing reordered - including over a real
//     framed packet, which is the symptom the defect had;
//   - the wrapped ring (head past tail, live bytes running over the end
//     of the buffer) behaves the same in both of its segments, whether
//     the first segment is cut short or goes out whole and the second is
//     cut short;
//   - a would-block before any byte goes out leaves the ring exactly as
//     it found it;
//   - a fully drained flush still normalises head and tail back to zero,
//     which is the one thing the old unconditional reset got right.
//
// The seam is the socket. Socket::send forwards to SocketImpl::send,
// which is virtual for this (see the note in SocketImpl.h); the impl
// below is scripted with one cap per call, so a partial send is an
// input the test can state rather than a congested peer it would have to
// arrange. It also records everything handed to it, which is what makes
// "the peer sees the queued bytes, contiguous" assertable at all.
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

#include "Cpackets/CGSkillToSelf.h"

#include <vector>

namespace {

typedef std::vector<unsigned char> ByteVec;

//----------------------------------------------------------------------
//
// class ScriptedSendSocketImpl
//
// A SocketImpl whose send() takes only as much as its script allows and
// keeps a copy of everything it took, in order.
//
//----------------------------------------------------------------------
class ScriptedSendSocketImpl : public SocketImpl {

public :

	ScriptedSendSocketImpl () throw () : m_nNextCall(0) {}

	// One cap per send() call, in order. A call past the end of the
	// script takes everything it is offered; a cap of 0 is the
	// would-block, which is the only way a real send() reports failure
	// with nothing sent.
	void	setScript ( const std::vector<uint> & Script ) throw ()
	{
		m_Script = Script;
		m_nNextCall = 0;
	}

	uint	send ( const void * buf , uint len , uint /*flags*/ )
		throw ( IOException , Error ) override
	{
		const uint nCap = ( m_nNextCall < m_Script.size() )
				? m_Script[m_nNextCall] : len;
		m_nNextCall++;

		if ( nCap == 0 )
			throw NonBlockingIOException("scripted would-block");

		const uint nSent = ( nCap < len ) ? nCap : len;
		const unsigned char * pBytes = (const unsigned char *)buf;
		m_Peer.insert( m_Peer.end() , pBytes , pBytes + nSent );
		return nSent;
	}

	// what the peer has received across every flush so far
	const ByteVec &	getPeer () const throw () { return m_Peer; }

	// how many times flush() reached the socket
	size_t		getCallCount () const throw () { return m_nNextCall; }

private :

	// per-call caps, consumed in order
	std::vector<uint>	m_Script;

	// how far into the script the next call is
	size_t			m_nNextCall;

	// every byte the socket accepted, in the order it accepted it
	ByteVec			m_Peer;
};

//----------------------------------------------------------------------
// Streams over a scripted socket. Socket takes ownership of the impl and
// deletes it, so m_pImpl is only valid for the fixture's lifetime; see
// packet_stream_access.h for why Winsock has to be up first.
//----------------------------------------------------------------------
struct FlushFixture
{
	ScriptedSendSocketImpl *	m_pImpl;
	Socket				m_Socket;
	SocketOutputStream		m_Stream;

	FlushFixture ( uint BufferLen = DefaultSocketOutputBufferSize )
	: m_pImpl((EnsureSocketsInitialised(), new ScriptedSendSocketImpl())),
	  m_Socket(m_pImpl),
	  m_Stream(&m_Socket, BufferLen)
	{
	}
};

struct EncryptFlushFixture
{
	ScriptedSendSocketImpl *	m_pImpl;
	Socket				m_Socket;
	SocketEncryptOutputStream	m_Stream;

	EncryptFlushFixture ( uint BufferLen = DefaultSocketEncryptOutputBufferSize )
	: m_pImpl((EnsureSocketsInitialised(), new ScriptedSendSocketImpl())),
	  m_Socket(m_pImpl),
	  m_Stream(&m_Socket, BufferLen)
	{
	}
};

//----------------------------------------------------------------------
// A run of distinct byte values, so a dropped, duplicated or reordered
// stretch cannot pass as the right one.
//----------------------------------------------------------------------
ByteVec	Pattern ( uint len , unsigned char first )
{
	ByteVec out;
	for ( uint i = 0 ; i < len ; i++ )
		out.push_back( (unsigned char)( first + i ) );
	return out;
}

ByteVec	Slice ( const ByteVec & in , size_t from , size_t to )
{
	return ByteVec( in.begin() + from , in.begin() + to );
}

ByteVec	Join ( const ByteVec & a , const ByteVec & b )
{
	ByteVec out(a);
	out.insert( out.end() , b.begin() , b.end() );
	return out;
}

void	Queue ( SocketOutputStream & stream , const ByteVec & bytes )
{
	stream.write( (const char*)bytes.data() , (uint)bytes.size() );
}

void	Script ( FlushFixture & f , uint a )
{
	std::vector<uint> script;
	script.push_back(a);
	f.m_pImpl->setScript(script);
}

void	Script ( FlushFixture & f , uint a , uint b )
{
	std::vector<uint> script;
	script.push_back(a);
	script.push_back(b);
	f.m_pImpl->setScript(script);
}

void	Script ( FlushFixture & f , uint a , uint b , uint c )
{
	std::vector<uint> script;
	script.push_back(a);
	script.push_back(b);
	script.push_back(c);
	f.m_pImpl->setScript(script);
}

//----------------------------------------------------------------------
// Leaves the ring wrapped in a 64-byte buffer: head at 30, tail at 16,
// 50 live bytes running over the end of the buffer and round to the
// front. Consume() moves the head exactly as a successful send does
// (packet_stream_access.h), which is the only way to reach that state
// without a peer. Returns the live bytes, in order.
//----------------------------------------------------------------------
ByteVec	FillWrapped ( SocketOutputStream & stream )
{
	const ByteVec first = Pattern(40, 0x10);
	Queue( stream , first );
	SocketOutputStreamTestAccess::Consume( stream , 30 );

	const ByteVec second = Pattern(40, 0x40);
	Queue( stream , second );

	return Join( Slice( first , 30 , 40 ) , second );
}

} // namespace

//----------------------------------------------------------------------
// A partial send keeps its remainder
//----------------------------------------------------------------------

// The socket takes 8 of 20 bytes and then refuses. The 12 it did not
// take are the next flush's job, not litter to be dropped.
TEST(SocketOutputStream, PartialFlushKeepsTheUnsentRemainder)
{
	FlushFixture f(64);

	const ByteVec queued = Pattern(20, 0x40);
	Queue( f.m_Stream , queued );

	Script( f , 8 , 0 );
	CHECK_EQ(8, f.m_Stream.flush());

	CHECK(f.m_pImpl->getPeer() == Slice(queued, 0, 8));
	CHECK_EQ(12, f.m_Stream.length());
	CHECK_EQ(false, f.m_Stream.isEmpty());
	CHECK(SocketOutputStreamTestAccess::Bytes(f.m_Stream) == Slice(queued, 8, 20));

	// The head has to name the first byte the socket did not take, or
	// the next flush would send bytes the peer already has.
	CHECK_EQ(8, SocketOutputStreamTestAccess::Head(f.m_Stream));
	CHECK_EQ(20, SocketOutputStreamTestAccess::Tail(f.m_Stream));

	// And the next flush sends that remainder first, so what the peer
	// received across the two flushes is the queued run, contiguous.
	CHECK_EQ(12, f.m_Stream.flush());
	CHECK(f.m_pImpl->getPeer() == queued);
	CHECK(f.m_Stream.isEmpty());
}

// The same property when the socket reports the short count instead of
// throwing: send() returning fewer bytes than it was offered is an
// ordinary return, and the loop calls it again until one call blocks.
TEST(SocketOutputStream, ShortSendsLoopAndTheirRemainderSurvives)
{
	FlushFixture f(64);

	const ByteVec queued = Pattern(20, 0x40);
	Queue( f.m_Stream , queued );

	Script( f , 3 , 5 , 0 );
	CHECK_EQ(8, f.m_Stream.flush());
	CHECK_EQ((long long)3, (long long)f.m_pImpl->getCallCount());

	CHECK(f.m_pImpl->getPeer() == Slice(queued, 0, 8));
	CHECK_EQ(12, f.m_Stream.length());
	CHECK(SocketOutputStreamTestAccess::Bytes(f.m_Stream) == Slice(queued, 8, 20));

	CHECK_EQ(12, f.m_Stream.flush());
	CHECK(f.m_pImpl->getPeer() == queued);
}

// A would-block on the very first call sent nothing, so the ring must be
// exactly as it was - which for a wrapped ring is a state the old
// unconditional reset could not even represent.
TEST(SocketOutputStream, AWouldBlockBeforeAnyByteLeavesTheRingUntouched)
{
	FlushFixture f(64);

	const ByteVec live = FillWrapped( f.m_Stream );
	CHECK_EQ(50, f.m_Stream.length());

	Script( f , 0 );
	CHECK_EQ(0, f.m_Stream.flush());

	CHECK_EQ((long long)1, (long long)f.m_pImpl->getCallCount());
	CHECK_EQ((long long)0, (long long)f.m_pImpl->getPeer().size());
	CHECK_EQ(50, f.m_Stream.length());
	CHECK(SocketOutputStreamTestAccess::Bytes(f.m_Stream) == live);
	CHECK_EQ(30, SocketOutputStreamTestAccess::Head(f.m_Stream));
	CHECK_EQ(16, SocketOutputStreamTestAccess::Tail(f.m_Stream));
}

//----------------------------------------------------------------------
// The wrapped ring, in both of its segments
//----------------------------------------------------------------------

// The live bytes run over the end of the buffer, so flush() sends them
// as two segments. Here the FIRST segment is cut short: 20 of its 34
// bytes go out and the ring stays wrapped around the remaining 30.
TEST(SocketOutputStream, PartialFlushOfAWrappedRingKeepsTheFirstSegment)
{
	FlushFixture f(64);

	const ByteVec live = FillWrapped( f.m_Stream );

	Script( f , 20 , 0 );
	CHECK_EQ(20, f.m_Stream.flush());

	CHECK(f.m_pImpl->getPeer() == Slice(live, 0, 20));
	CHECK_EQ(30, f.m_Stream.length());
	CHECK(SocketOutputStreamTestAccess::Bytes(f.m_Stream) == Slice(live, 20, 50));
	CHECK_EQ(50, SocketOutputStreamTestAccess::Head(f.m_Stream));
	CHECK_EQ(16, SocketOutputStreamTestAccess::Tail(f.m_Stream));

	CHECK_EQ(30, f.m_Stream.flush());
	CHECK(f.m_pImpl->getPeer() == live);
	CHECK(f.m_Stream.isEmpty());
}

// And here the first segment goes out WHOLE - which is where flush()
// resets the head to zero on its own - and the second is cut short. The
// ring is left unwrapped, head off zero, holding the last 10 bytes.
TEST(SocketOutputStream, PartialFlushOfAWrappedRingKeepsTheSecondSegment)
{
	FlushFixture f(64);

	const ByteVec live = FillWrapped( f.m_Stream );

	Script( f , 34 , 6 , 0 );
	CHECK_EQ(40, f.m_Stream.flush());

	CHECK(f.m_pImpl->getPeer() == Slice(live, 0, 40));
	CHECK_EQ(10, f.m_Stream.length());
	CHECK(SocketOutputStreamTestAccess::Bytes(f.m_Stream) == Slice(live, 40, 50));
	CHECK_EQ(6, SocketOutputStreamTestAccess::Head(f.m_Stream));
	CHECK_EQ(16, SocketOutputStreamTestAccess::Tail(f.m_Stream));

	CHECK_EQ(10, f.m_Stream.flush());
	CHECK(f.m_pImpl->getPeer() == live);
	CHECK(f.m_Stream.isEmpty());
}

//----------------------------------------------------------------------
// The drained path is unchanged
//----------------------------------------------------------------------

// A flush the socket takes whole still empties the ring and normalises
// both indices to zero, so a long session cannot walk the live run into
// a wrap it never needed. Both the ordinary and the wrapped shape.
TEST(SocketOutputStream, AFullFlushEmptiesAndNormalisesTheRing)
{
	{
		FlushFixture f(64);

		const ByteVec queued = Pattern(20, 0x40);
		Queue( f.m_Stream , queued );

		CHECK_EQ(20, f.m_Stream.flush());
		CHECK(f.m_pImpl->getPeer() == queued);
		CHECK(f.m_Stream.isEmpty());
		CHECK_EQ(0, f.m_Stream.length());
		CHECK_EQ(0, SocketOutputStreamTestAccess::Head(f.m_Stream));
		CHECK_EQ(0, SocketOutputStreamTestAccess::Tail(f.m_Stream));
	}

	{
		FlushFixture f(64);

		const ByteVec live = FillWrapped( f.m_Stream );

		CHECK_EQ(50, f.m_Stream.flush());
		CHECK(f.m_pImpl->getPeer() == live);
		CHECK(f.m_Stream.isEmpty());
		CHECK_EQ(0, SocketOutputStreamTestAccess::Head(f.m_Stream));
		CHECK_EQ(0, SocketOutputStreamTestAccess::Tail(f.m_Stream));
	}
}

// Nothing queued, nothing sent - and the socket is not touched at all.
TEST(SocketOutputStream, FlushingAnEmptyRingSendsNothing)
{
	FlushFixture f(64);

	CHECK_EQ(0, f.m_Stream.flush());
	CHECK_EQ((long long)0, (long long)f.m_pImpl->getCallCount());
	CHECK_EQ((long long)0, (long long)f.m_pImpl->getPeer().size());
	CHECK(f.m_Stream.isEmpty());
}

//----------------------------------------------------------------------
// What is written after a partial flush queues behind the remainder
//----------------------------------------------------------------------

// The head is off zero when the next packet is written, which is the
// state write() has to extend correctly for the remainder to keep its
// place in front of it.
TEST(SocketOutputStream, WritesAfterAPartialFlushQueueBehindTheRemainder)
{
	FlushFixture f(64);

	const ByteVec queued = Pattern(20, 0x40);
	Queue( f.m_Stream , queued );

	Script( f , 8 , 0 );
	CHECK_EQ(8, f.m_Stream.flush());

	const ByteVec more = Pattern(10, 0x80);
	Queue( f.m_Stream , more );
	CHECK_EQ(22, f.m_Stream.length());
	CHECK(SocketOutputStreamTestAccess::Bytes(f.m_Stream)
	      == Join(Slice(queued, 8, 20), more));

	CHECK_EQ(22, f.m_Stream.flush());
	CHECK(f.m_pImpl->getPeer() == Join(queued, more));
	CHECK(f.m_Stream.isEmpty());
}

//----------------------------------------------------------------------
// The symptom: a real frame, cut mid-packet
//----------------------------------------------------------------------

// Two real framed packets, with the socket taking 9 bytes and refusing
// the rest - a cut inside the first packet's body (the frame header is
// seven bytes: id, size, sequence; the body starts at offset 7). What
// the peer ends up with must be the two frames byte for byte, which is
// exactly what the old reset made impossible.
TEST(SocketEncryptOutputStream, APartialFlushDeliversTheFrameContiguously)
{
	CGSkillToSelf packet;
	packet.setSkillType(0x71B2);
	packet.setCEffectID(0x83C4);

	// The frame the peer is owed, from a stream that is never flushed.
	EncryptFlushFixture reference;
	reference.m_Stream.setEncryptCode(3);
	reference.m_Stream.write( &packet );
	reference.m_Stream.write( &packet );
	const ByteVec expected =
		SocketOutputStreamTestAccess::Bytes(reference.m_Stream);
	// Two 11-byte frames; the cut at 9 lands two bytes into the first body.
	CHECK_EQ((size_t)22, expected.size());

	EncryptFlushFixture f;
	f.m_Stream.setEncryptCode(3);
	f.m_Stream.write( &packet );
	f.m_Stream.write( &packet );

	std::vector<uint> script;
	script.push_back(9);
	script.push_back(0);
	f.m_pImpl->setScript(script);

	CHECK_EQ(9, f.m_Stream.flush());
	CHECK(f.m_pImpl->getPeer() == Slice(expected, 0, 9));
	CHECK_EQ((long long)(expected.size() - 9), (long long)f.m_Stream.length());

	CHECK_EQ((long long)(expected.size() - 9), (long long)f.m_Stream.flush());
	CHECK(f.m_pImpl->getPeer() == expected);
	CHECK(f.m_Stream.isEmpty());
}
