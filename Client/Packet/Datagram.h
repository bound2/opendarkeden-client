//////////////////////////////////////////////////////////////////////
//
// Filename    : Datagram.h
// Written By  : reiot@ewestsoft.com
// Description : 
//
//////////////////////////////////////////////////////////////////////

#ifndef __DATAGRAM_H__
#define __DATAGRAM_H__

#include "Types.h"
#include "Exception.h"
#include "SocketAPI.h"
#include "WireScalar.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#if defined(PLATFORM_POSIX)
	#include <sys/socket.h>
	#include <arpa/inet.h>
	#include <netinet/in.h>
#elif __WINDOWS__
	#include <winsock.h>
#endif

// forward declaration
class DatagramPacket;

//////////////////////////////////////////////////////////////////////
//
// class Datagram;
//
// UDP 소켓으로부터 입력받거나 출력할 데이타의 집합이다.
// 각 Datagram은 보낼 곳 또는 보낸 곳의 주소를 가지고 있다.
//
//////////////////////////////////////////////////////////////////////

class Datagram {

public :

	// constructor
	Datagram ();

	// destructor
	~Datagram ();

	// read DatagramPacket from datagram's internal buffer
	void read ( char * buf , uint len );
	void read ( std::span<char> buf );
	void read ( std::span<std::byte> buf );
	void read ( std::string & str , uint len );
	void read ( DatagramPacket * & pPacket );

	// Typed scalar read: the same WireScalar set the socket streams
	// accept, at the same widths, so a datagram body and a stream body
	// are written by one rule. The scalar overloads below route through
	// it; `char` alone stays a one-byte span, as it is not a fixed-width
	// integer type.
	template <packetwire::WritableWireScalar T>
	void readWire ( T & value )
	{
		using Storage = packetwire::WireStorageT<T>;
		Storage storage = 0;
		read(std::as_writable_bytes(std::span(&storage, 1)));
		if constexpr (std::is_enum_v<std::remove_cv_t<T>>)
			value = static_cast<T>(storage);
		else
			value = storage;
	}

	void read ( char   & buf ) { read( std::span<char>(&buf, 1) ); }
	void read ( uchar  & buf ) { readWire(buf); }
	void read ( short  & buf ) { readWire(buf); }
	void read ( ushort & buf ) { readWire(buf); }
	void read ( int    & buf ) { readWire(buf); }
	void read ( uint   & buf ) { readWire(buf); }
	void read ( long   & buf ) {
		int32_t tmp = 0;
		readWire(tmp);
		buf = static_cast<long>(tmp);
	}
	void read ( ulong  & buf ) {
		uint32_t tmp = 0;
		readWire(tmp);
		buf = static_cast<ulong>(tmp);
	}

	// write DatagramPacket into datagram's internal buffer
	void write ( const char * buf , uint len );
	void write ( std::span<const char> buf );
	void write ( std::span<const std::byte> buf );
	void write ( const std::string & buf );
	void write ( const DatagramPacket * pPacket );

	template <packetwire::WireScalar T>
	void writeWire ( T value )
	{
		using Storage = packetwire::WireStorageT<T>;
		const Storage storage = static_cast<Storage>(value);
		write(std::as_bytes(std::span(&storage, 1)));
	}

	void write ( char   buf ) { write( std::span<const char>(&buf, 1) ); }
	void write ( uchar  buf ) { writeWire(buf); }
	void write ( short  buf ) { writeWire(buf); }
	void write ( ushort buf ) { writeWire(buf); }
	void write ( int    buf ) { writeWire(buf); }
	void write ( uint   buf ) { writeWire(buf); }
	void write ( long   buf ) {
		int32_t tmp = static_cast<int32_t>(buf);
		writeWire(tmp);
	}
	void write ( ulong  buf ) {
		uint32_t tmp = static_cast<uint32_t>(buf);
		writeWire(tmp);
	}

	// get data
	char * getData () noexcept { return m_Data; }

	// set data
	void setData ( char * data , uint len );
	void setData ( uint len ); 
	
	// get length
	uint getLength () const noexcept { return m_Length; }

	// get address
	SOCKADDR * getAddress () noexcept { return (SOCKADDR*)&m_SockAddr; }

	// set address
	void setAddress ( SOCKADDR_IN * pSockAddr );

	// get host
	std::string getHost () const { return std::string( inet_ntoa( m_SockAddr.sin_addr ) ); }

	// set host
	void setHost ( const std::string & host ) noexcept { m_SockAddr.sin_addr.s_addr = inet_addr( host.c_str() ); }

	// get port 
	uint getPort () const noexcept { return ntohs( m_SockAddr.sin_port ); }

	// set port
	void setPort ( uint port ) noexcept { m_SockAddr.sin_port = htons(port); }

	std::string toString () const;

private :

	// buffer length
	uint m_Length;

	// reading/writing offset
	uint m_InputOffset;
	uint m_OutputOffset;

	// internal buffer
	char * m_Data;

	// socket address
	SOCKADDR_IN m_SockAddr;

};

#endif
