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

#if __LINUX__
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
	void read ( std::string & str , uint len );
	void read ( DatagramPacket * & pPacket );

	void read ( char   & buf ) { read( (char*)&buf , szchar   ); }
    void read ( uchar  & buf ) { read( (char*)&buf , szuchar  ); }
    void read ( short  & buf ) { read( (char*)&buf , szshort  ); }
    void read ( ushort & buf ) { read( (char*)&buf , szushort ); }
    void read ( int    & buf ) { read( (char*)&buf , szint    ); }
    void read ( uint   & buf ) { read( (char*)&buf , szuint   ); }
    void read ( long   & buf ) {
        int32_t tmp = 0;
        read( (char*)&tmp , szlong );
        buf = static_cast<long>(tmp);
    }
    void read ( ulong  & buf ) {
        uint32_t tmp = 0;
        read( (char*)&tmp , szulong );
        buf = static_cast<ulong>(tmp);
    }

	// write DatagramPacket into datagram's internal buffer
	void write ( const char * buf , uint len );
	void write ( const std::string & buf );
	void write ( const DatagramPacket * pPacket );

	void write ( char   buf ) { write( (char*)&buf , szchar   ); }
    void write ( uchar  buf ) { write( (char*)&buf , szuchar  ); }
    void write ( short  buf ) { write( (char*)&buf , szshort  ); }
    void write ( ushort buf ) { write( (char*)&buf , szushort ); }
    void write ( int    buf ) { write( (char*)&buf , szint    ); }
    void write ( uint   buf ) { write( (char*)&buf , szuint   ); }
    void write ( long   buf ) {
        int32_t tmp = static_cast<int32_t>(buf);
        write( (char*)&tmp , szlong );
    }
    void write ( ulong  buf ) {
        uint32_t tmp = static_cast<uint32_t>(buf);
        write( (char*)&tmp , szulong );
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
