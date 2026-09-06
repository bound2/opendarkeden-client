//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGPortCheck.cpp 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "CGPortCheck.h"


//////////////////////////////////////////////////////////////////////
// Datagram 객체로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////
void CGPortCheck::read ( Datagram & iDatagram ) 
	 throw ( ProtocolException , Error )
{
	__BEGIN_TRY

	//--------------------------------------------------
	// read creature's name
	//--------------------------------------------------
	BYTE szPCName;

	iDatagram.read( szPCName );

	if ( szPCName == 0 )
		throw InvalidProtocolException("szPCName == 0");

	if ( szPCName > 20 )
		throw InvalidProtocolException("too long name length");

	iDatagram.read( m_PCName , szPCName );

	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////
// Datagram 객체로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////
void CGPortCheck::write ( Datagram & oDatagram ) const 
     throw ( ProtocolException , Error )
{
	__BEGIN_TRY

	//--------------------------------------------------
	// write PC name
	//--------------------------------------------------
	// The cap runs on the std::string's own size, BEFORE the narrowing
	// to the BYTE that goes on the wire: (BYTE)276 is 20, so a
	// 276-character name used to pass this test and then go out whole
	// behind a length byte claiming 20. (Datagram has no span overload,
	// so the bounded write is the pointer/length one.)
	if ( m_PCName.size() > 20 )
		throw InvalidProtocolException("too long name length");

	const BYTE szPCName = (BYTE)m_PCName.size();

	if ( szPCName == 0 )
		throw InvalidProtocolException("szPCName == 0");

	oDatagram.write( szPCName );

	oDatagram.write( m_PCName.data(), szPCName );

	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_OUTPUT__
std::string CGPortCheck::toString () const
    throw ()
{
	StringStream msg;

	msg << "CGPortCheck("
		<< ",PCName:" << m_PCName 
		<< ")";

	return msg.toString();
}

#endif