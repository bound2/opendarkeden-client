//////////////////////////////////////////////////////////////////////
//
// Filename    : StringStream.cc
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#ifdef PLATFORM_WINDOWS
#include <WTYPES.H>
#endif
#include "StringStream.h"
#include <stdio.h>

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
StringStream::StringStream () 
: m_Size(0), m_bInserted(false), m_Buffer("")
{
}

	
//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
StringStream::~StringStream () 
{
}

	
//////////////////////////////////////////////////////////////////////
// add string to stream
//////////////////////////////////////////////////////////////////////
StringStream & StringStream::operator << ( bool T ) 
{
	std::string buf( T == true ? "true" : "false" );

	m_Strings.push_back( buf );

	m_Size += buf.size();
	m_bInserted = true;

	return *this;
}

// One streamed character is one byte: no trailing NUL into toString().

StringStream & StringStream::operator << ( char T )
{
	std::string buf(1,T);

	m_Strings.push_back( buf );

	m_Size += buf.size();
	m_bInserted = true;

	return *this;
}

StringStream & StringStream::operator << ( uchar T )
{
	std::string buf(1,(char)T);

	m_Strings.push_back( buf );

	m_Size += buf.size();
	m_bInserted = true;

	return *this;
}

// Buffer sizing note for the numeric operators below: every buffer is
// sized for the widest "%"-format output of its type's full range, and
// every call is snprintf, so an undersized buffer can only truncate,
// never overrun. The old sprintf-into-char[12] family overran the stack
// for float values >= 10,000 and double values >= ~1e15 (%f of DBL_MAX
// is 316 characters) - pinned by tests/unit/test_stringstream.cpp.

StringStream & StringStream::operator << ( short T )
{
	char buf[8];
	snprintf( buf , sizeof(buf) , "%d" , T );

	std::string str(buf);

	m_Strings.push_back( str );

	m_Size += str.size();
	m_bInserted = true;

	return *this;
}

StringStream & StringStream::operator << ( ushort T )
{
	char buf[8];
	snprintf( buf , sizeof(buf) , "%d" , T );

	std::string str(buf);

	m_Strings.push_back( str );

	m_Size += str.size();
	m_bInserted = true;

	return *this;
}

StringStream & StringStream::operator << ( int T )
{
	char buf[24];
	snprintf( buf , sizeof(buf) , "%d" , T );
	
	std::string str(buf);

	m_Strings.push_back( str );

	m_Size += str.size();
	m_bInserted = true;

	return *this;
}

StringStream & StringStream::operator << ( uint T )
{
	char buf[24];
	snprintf( buf , sizeof(buf) , "%u" , T );
	
	std::string str(buf);

	m_Strings.push_back( str );

	m_Size += str.size();
	m_bInserted = true;

	return *this;
}

StringStream & StringStream::operator << ( long T )
{
	// long is 32-bit under MSVC but 64-bit on LP64 platforms - size for
	// the wider case rather than the current compiler.
	char buf[24];
	snprintf( buf , sizeof(buf) , "%ld" , T );
	
	std::string str(buf);

	m_Strings.push_back( str );

	m_Size += str.size();
	m_bInserted = true;

	return *this;
}

StringStream & StringStream::operator << ( ulong T )
{
	char buf[24];
	snprintf( buf , sizeof(buf) , "%lu" , T );
	
	std::string str(buf);

	m_Strings.push_back( str );

	m_Size += str.size();
	m_bInserted = true;

	return *this;
}

StringStream & StringStream::operator << ( ulonglong T )
{
	char buf[24];
	snprintf( buf , sizeof(buf) , "%llu" , T );

	std::string str(buf);

	m_Strings.push_back( str );

	m_Size += str.size();
	m_bInserted = true;

	return *this;
}

StringStream & StringStream::operator << ( float T )
{
	// %f of FLT_MAX is 46 characters (39 integer digits, '.', 6 decimals).
	char buf[64];
	snprintf( buf , sizeof(buf) , "%f" , T );
	
	std::string str(buf);

	m_Strings.push_back( str );

	m_Size += str.size();
	m_bInserted = true;

	return *this;
}

StringStream & StringStream::operator << ( double T )
{
	// %f of -DBL_MAX is 317 characters (sign, 309 integer digits, '.',
	// 6 decimals); sized with headroom so no representable double
	// truncates.
	char buf[352];
	snprintf( buf , sizeof(buf) , "%f" , T );
	
	std::string str(buf);

	m_Strings.push_back( str );

	m_Size += str.size();
	m_bInserted = true;

	return *this;
}

StringStream & StringStream::operator << ( const char * buf )
{
	std::string str(buf);

	m_Strings.push_back( str );

	m_Size += str.size();
	m_bInserted = true;

	return *this;
}

StringStream & StringStream::operator << ( const std::string & str )
{
	m_Strings.push_back( str );

	m_Size += str.size();
	m_bInserted = true;

	return *this;
}


//////////////////////////////////////////////////////////////////////
// make string
//////////////////////////////////////////////////////////////////////
std::string StringStream::toString () const
{
	// Once the string has been built, later calls reuse it until
	// something new is inserted.
	if ( m_bInserted ) {

		m_bInserted = false;

		// The rebuild starts from nothing.
		m_Buffer.clear();

		// Reserve the whole size up front so the appends do not copy.
		m_Buffer.reserve( m_Size );

		for ( std::list<std::string>::const_iterator itr = m_Strings.begin () ;
			  itr != m_Strings.end() ;
			  itr ++ ) {
			m_Buffer.append( *itr );
		}
	}

	return m_Buffer;
}
