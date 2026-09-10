//////////////////////////////////////////////////////////////////////
// 
// Filename    : SocketInputStream.h 
// Written by  : reiot@ewestsoft.com
// Description :
// 
//////////////////////////////////////////////////////////////////////
//
// *Reiot's Notes*
//
// 시스템에서 가장 빈번하게 사용되는 클래스중의 하나이다.
// 속도에 무지막지한 영향을 미치므로, 만일 좀더 속도를 보강하고
// 싶다면, exception을 빼고 re-write 하라. 
//
// 현재 nonblocking 이 굉장히-억수로-졸라 많이 발생한다고 했을때,
// 이것이 NonBlockingIOException으로 wrapping될때 overhead가 발생할
// 확률이 높다고 추측된다.
//
//////////////////////////////////////////////////////////////////////

#ifndef __SOCKET_INPUT_STREAM_H__
#define __SOCKET_INPUT_STREAM_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Socket.h"
#include "WireScalar.h"

#include <cstddef>
#include <span>

// constant definitions
const uint DefaultSocketInputBufferSize = 8192;

// forward declaration
class Packet;

//////////////////////////////////////////////////////////////////////
//
// class SocketInputStream
//
//////////////////////////////////////////////////////////////////////

class SocketInputStream {

//////////////////////////////////////////////////
// constructor/destructor
//////////////////////////////////////////////////
public :
	
	// constructor
	SocketInputStream ( Socket * sock , uint BufferSize = DefaultSocketInputBufferSize );
	
	// destructor
	virtual ~SocketInputStream () noexcept(false);

	
//////////////////////////////////////////////////
// methods
//////////////////////////////////////////////////
public :
	
	// read data from stream (input buffer)
	uint read ( char * buf , uint len );
	uint read ( std::span<char> buf );
	uint read ( std::span<char> buf , std::size_t len );
	uint read ( std::span<std::byte> buf );
	uint read ( std::string & str , uint len );
	void read ( Packet * p );

	template <packetwire::WritableWireScalar T>
	uint readWire ( T & value )
	{
		using Storage = packetwire::WireStorageT<T>;
		Storage storage = 0;
		const uint count = read(std::as_writable_bytes(std::span(&storage, 1)));
		if constexpr (std::is_enum_v<std::remove_cv_t<T>>)
			value = static_cast<T>(storage);
		else
			value = storage;
		return count;
	}

	uint read ( bool   & buf ) { return read( (char*)&buf, szbool   ); }
	uint read ( char   & buf ) { return read( std::span<char>( &buf, 1 ) ); }
	uint read ( uchar  & buf ) { return readWire(buf); }
	uint read ( short  & buf ) { return readWire(buf); }
	uint read ( ushort & buf ) { return readWire(buf); }
	uint read ( int    & buf ) { return readWire(buf); }
	uint read ( uint   & buf ) { return readWire(buf); }
	uint read ( long   & buf ) {
		int32_t tmp = 0;
		uint ret = readWire(tmp);
		buf = static_cast<long>(tmp);
		return ret;
	}
	uint read ( ulong  & buf ) {
		uint32_t tmp = 0;
		uint ret = readWire(tmp);
		buf = static_cast<ulong>(tmp);
		return ret;
	}

	// peek data from stream (input buffer)
	bool peek ( char * buf , uint len );
	
	// skip data from stream (input buffer)
	void skip ( uint len );
	
	// fill stream (input buffer) from socket
	uint fill ();
	uint fill_RAW ();

	// resize buffer
	void resize ( int size );
	
	// get buffer length
	uint capacity () const noexcept { return m_BufferLen; }
	
	// get data length in buffer
	uint length () const;
	uint size () const { return length(); }

	// check if buffer is empty
	bool isEmpty () const { return length() == 0; }

	// get debug string
	std::string toString () const;


//////////////////////////////////////////////////
// attributes
//////////////////////////////////////////////////
private :

	// A decoder may catch a stream exception itself. Remember the failure until
	// the outer framed read finishes so partially decoded packets cannot pass.
	[[noreturn]] void failRead(const char* message);
	[[noreturn]] void failUnderflow(uint missing);
	void finishFrame() noexcept;

	// Test seam: lets tests/unit preload the ring buffer with hostile
	// bytes without a connected socket (fill() is the only production
	// writer, and it needs a live peer). Friendship changes access only -
	// no layout, no behavior - and is declared unconditionally so the
	// class definition stays identical in every translation unit.
	friend class SocketInputStreamTestAccess;

	// socket
	Socket * m_pSocket;
	
	// buffer
	char * m_Buffer;
	
	// buffer length
	uint m_BufferLen;
	
	// buffer head/tail
	uint m_Head;
	uint m_Tail;

	// While read(Packet*) is parsing a frame, every body read is limited to
	// the size declared in that frame's header. This prevents a malformed
	// body from consuming bytes that belong to the following packet.
	bool m_bFrameBounded;
	bool m_bFrameReadFailed;
	uint m_FrameRemaining;
public :

	// There is no transport encryption on this stream. The EncryptData that
	// used to live here returned its key before reaching its own XOR loop, so
	// every call in fill() was a no-op and the socket has always carried
	// cleartext -- including the account password that CLLogin::write sends.
	// Both the dead function and its call sites are gone so the absence is
	// visible rather than implied. setKey survives only because Player::setKey
	// still calls it after CGConnectSetKey; adding real encryption is a
	// protocol change that has to be agreed with the server repository first.
	void setKey(WORD /*EncryptKey*/, BYTE* /*HashTable*/) noexcept {}
};

#endif
