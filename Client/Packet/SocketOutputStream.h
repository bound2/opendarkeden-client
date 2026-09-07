//////////////////////////////////////////////////////////////////////
// 
// SocketOutputStream.h 
// 
// by Reiot
// 
//////////////////////////////////////////////////////////////////////

#ifndef __SOCKET_OUTPUT_STREAM_H__
#define __SOCKET_OUTPUT_STREAM_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Socket.h"
#include "WireScalar.h"

#include <cstddef>
#include <span>

// constant definitions
const unsigned int DefaultSocketOutputBufferSize = 8192;

// forward declaration
class Packet;

//////////////////////////////////////////////////////////////////////
//
// class SocketOutputStream
//
//////////////////////////////////////////////////////////////////////

class SocketOutputStream {

//////////////////////////////////////////////////
// constructor/destructor
//////////////////////////////////////////////////
public :
	
	// constructor
	SocketOutputStream ( Socket * sock , uint BufferSize = DefaultSocketOutputBufferSize );
	
	// destructor
	virtual ~SocketOutputStream () noexcept(false);

	
//////////////////////////////////////////////////
// methods
//////////////////////////////////////////////////
public :
	
	// write data to stream (output buffer)
	// *CAUTION*
	// string 을 버퍼에 writing 할 때, 자동으로 size 를 앞에 붙일 수도 있다.
	// 그러나, string 의 크기를 BYTE/WORD 중 어느 것으로 할 건지는 의문이다.
	// 패킷의 크기는 작을 수록 좋다는 정책하에서 필요에 따라서 string size 값을
	// BYTE 또는 WORD 를 수동으로 사용하도록 한다.
	uint write ( const char * buf , uint len );
	uint write ( std::span<const char> buf );
	uint write ( std::span<const std::byte> buf );
	uint write ( const std::string & buf )
	{
		return write(std::span<const char>(buf.data(), buf.size()));
	}
	void write ( const Packet * pPacket );
	
	template <packetwire::WireScalar T>
	uint writeWire ( T value )
	{
		using Storage = packetwire::WireStorageT<T>;
		const Storage storage = static_cast<Storage>(value);
		return write(std::as_bytes(std::span(&storage, 1)));
	}

    uint write ( bool   buf ) { return write( (const char*)&buf, szbool   ); }
    uint write ( char   buf ) { return write( (const char*)&buf, szchar   ); }
    uint write ( uchar  buf ) { return writeWire(buf); }
    uint write ( short  buf ) { return writeWire(buf); }
    uint write ( ushort buf ) { return writeWire(buf); }
    uint write ( int    buf ) { return writeWire(buf); }
    uint write ( uint   buf ) { return writeWire(buf); }
    uint write ( long   buf ) {
        int32_t tmp = static_cast<int32_t>(buf);
		return writeWire(tmp);
    }
    uint write ( ulong  buf ) {
        uint32_t tmp = static_cast<uint32_t>(buf);
		return writeWire(tmp);
    }

	// flush stream (output buffer) to socket
	uint flush ();

	// resize buffer 
	void resize ( int size );

	// get buffer length
	int capacity () const noexcept { return m_BufferLen; }
 
    // get data length in buffer
    uint length () const noexcept;
    uint size () const noexcept { return length(); }
 
    // check if buffer is empty
    bool isEmpty () const noexcept { return m_Head == m_Tail; }

    // get debug string
    std::string toString () const
    {
        StringStream msg;
        msg << "SocketOutputStream(m_BufferLen:"<<m_BufferLen<<",m_Head:"<<m_Head<<",m_Tail:"<<m_Tail
<<")";
        return msg.toString();
    }
	// 놓迦뺏룐관埼죗
	void InitSeq(){ m_Sequence =0;}
//////////////////////////////////////////////////
// attributes
//////////////////////////////////////////////////
private :

	// Test seam (tests/support/packet_stream_access.h): unit tests copy
	// the ring out without a socket to flush to - flush() is the only
	// production reader. Access only, no layout or behavior change;
	// declared unconditionally so the class definition stays identical
	// in every translation unit (the same seam SocketInputStream has).
	friend class SocketOutputStreamTestAccess;

	// socket
	Socket * m_Socket;

	// output buffer
	char * m_Buffer;
	
	// buffer length
	uint m_BufferLen;
	
	// buffer head/tail
	uint m_Head;
	uint m_Tail;
	// 룐관埼죗
	BYTE m_Sequence;
public :

	// There is no transport encryption on this stream -- see the same note in
	// SocketInputStream.h. Everything this client sends, the login password
	// included, goes out in cleartext.
	void setKey(WORD /*EncryptKey*/, BYTE* /*HashTable*/) noexcept {}
};

#endif
