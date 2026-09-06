//////////////////////////////////////////////////////////////////////////////
// Filename    : CGModifyNickname.cpp 
// Written By  : elca@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGModifyNickname.h"

CGModifyNickname::CGModifyNickname () 
     throw ()
{
	__BEGIN_TRY
	__END_CATCH
}

CGModifyNickname::~CGModifyNickname () 
    throw ()
{
	__BEGIN_TRY
	__END_CATCH
}

void CGModifyNickname::read (SocketInputStream & iStream) 
	 throw (ProtocolException , Error)
{
	__BEGIN_TRY

	iStream.read( m_NicknameID );

	BYTE szSTR;
	iStream.read( szSTR );
	iStream.read( m_Nickname, szSTR );

	__END_CATCH
}

void CGModifyNickname::write (SocketOutputStream & oStream) const 
     throw (ProtocolException , Error)
{
	__BEGIN_TRY

	oStream.write( m_NicknameID );

	// Cap before narrowing; the length byte must describe every byte written.
	if ( m_Nickname.size() > MAX_NICKNAME_SIZE )
		throw InvalidProtocolException("too large nickname length");

	const BYTE szSTR = (BYTE)m_Nickname.size();

	oStream.write( szSTR );
	oStream.write( std::span<const char>( m_Nickname.data(), szSTR ) );

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
string CGModifyNickname::toString () 
	const throw ()
{
	__BEGIN_TRY
		
	StringStream msg;
	msg << "CGModifyNickname("
		<< ")";
	return msg.toString();

	__END_CATCH
}
#endif