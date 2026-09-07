//////////////////////////////////////////////////////////////////////////////
// Filename    : CGModifyGuildMember.cpp 
// Written By  : 
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGModifyGuildMember.h"


void CGModifyGuildMember::read (SocketInputStream & iStream)
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
	iStream.read( m_GuildMemberRank );

	__END_CATCH
}

void CGModifyGuildMember::write (SocketOutputStream & oStream) const
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
	oStream.write( m_GuildMemberRank );

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__

std::string CGModifyGuildMember::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
    msg << "CGModifyGuildMember("
		<< "GuildID:" << (int)m_GuildID
		<< "Name:" << m_Name
		<< "GuildMemberRank:" << m_GuildMemberRank
		<< ")";
	return msg.toString();

	__END_CATCH
}
#endif