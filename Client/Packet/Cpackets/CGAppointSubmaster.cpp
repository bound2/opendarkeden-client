//////////////////////////////////////////////////////////////////////////////
// Filename    : CGAppointSubmaster.cpp 
// Written By  : 
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGAppointSubmaster.h"


void CGAppointSubmaster::read (SocketInputStream & iStream)
{
	__BEGIN_TRY

	BYTE szName;

	iStream.read( m_GuildID );
	iStream.read( szName );

	if ( szName == 0 )
		throw InvalidProtocolException( "szName == 0" );
	if ( szName > 20 )
		throw InvalidProtocolException( "too long szName length" );

	iStream.read( m_Name, szName );

	__END_CATCH
}

void CGAppointSubmaster::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY

	// Cap the std::string's own size, before narrowing to the length byte.
	if ( m_Name.size() > 20 )
		throw InvalidProtocolException( "too long szName length" );

	const BYTE szName = (BYTE)m_Name.size();

	if ( szName == 0 )
		throw InvalidProtocolException( "szName == 0" );

	oStream.write( m_GuildID );
	oStream.write( szName );
	oStream.write( std::span<const char>(m_Name.data(), szName) );

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
string CGAppointSubmaster::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
    msg << "CGAppointSubmaster("
		<< "GuildID:" << (int)m_GuildID
		<< "Name:" << m_Name
		<< ")";
	return msg.toString();

	__END_CATCH
}
#endif