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
	throw ()
: m_Size(0), m_bInserted(false), m_Buffer("")
{
}

	
//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
StringStream::~StringStream () 
    throw ()
{
}

	
//////////////////////////////////////////////////////////////////////
// add string to stream
//////////////////////////////////////////////////////////////////////
StringStream & StringStream::operator << ( bool T ) 
	throw ()
{
	std::string buf( T == true ? "true" : "false" );

	m_Strings.push_back( buf );

	m_Size += buf.size();
	m_bInserted = true;

	return *this;
}

// One streamed character is one byte. Both overloads below used to build
// a std::string(2,'\0') and write the character into the first byte only,
// so the trailing NUL survived into toString()'s result - which cut every
// text a consumer read through c_str() off at the first streamed
// character. Throwable::toString() streams a '\n' between the message and
// the stack trace, so SendBugReport("%s", t.toString().c_str()) was
// handed a bug report cut off at its first line (SendBugReport then cuts
// at 100 bytes of its own, so the server still sees at most that much),
// and __assert__ streams eos first, so assertion_failed.log received a
// bare newline per failed Assert. Pinned by
// tests/unit/test_stringstream_chars.cpp.

StringStream & StringStream::operator << ( char T )
	throw ()
{
	std::string buf(1,T);

	m_Strings.push_back( buf );

	m_Size += buf.size();
	m_bInserted = true;

	return *this;
}

StringStream & StringStream::operator << ( uchar T )
	throw ()
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
	throw ()
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
	throw ()
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
	throw ()
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
	throw ()
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
	throw ()
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
	throw ()
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
	throw ()
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
	throw ()
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
	throw ()
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
	throw ()
{
	std::string str(buf);

	m_Strings.push_back( str );

	m_Size += str.size();
	m_bInserted = true;

	return *this;
}

StringStream & StringStream::operator << ( const std::string & str )
	throw ()
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
	throw ()
{
	// Once the string has been built, later calls reuse it until
	// something new is inserted.
	if ( m_bInserted ) {

		m_bInserted = false;

		// The rebuild starts from nothing. It used to append the whole
		// list to the previous result, so a second toString() after a
		// further insertion returned the old text followed by all of it
		// again - latent only because every caller in the tree calls it
		// once, or twice with nothing inserted between.
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
