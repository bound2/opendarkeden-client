//////////////////////////////////////////////////////////////////////////////
// Filename    : CGModifyGuildMemberIntro.cpp 
// Written By  : 
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGModifyGuildMemberIntro.h"


void CGModifyGuildMemberIntro::read (SocketInputStream & iStream) 
	 throw (ProtocolException , Error)
{
	__BEGIN_TRY

	BYTE szGuildMemberIntro;

	iStream.read( m_GuildID );
	iStream.read( szGuildMemberIntro );

	if ( szGuildMemberIntro > 255 )
		throw InvalidProtocolException( "too long szGuildMemberIntro length" );

	if ( szGuildMemberIntro > 0 )
		iStream.read( m_GuildMemberIntro, szGuildMemberIntro );
	else
		m_GuildMemberIntro = "";

	__END_CATCH
}

void CGModifyGuildMemberIntro::write (SocketOutputStream & oStream) const 
     throw (ProtocolException , Error)
{
	__BEGIN_TRY

	// The cap runs on the std::string's own size, BEFORE the narrowing
	// to the BYTE that goes on the wire. This one never fired: no BYTE
	// exceeds 255, so every intro passed and one over 255 went out whole
	// behind a length byte holding its low eight bits.
	if ( m_GuildMemberIntro.size() > 255 )
		throw InvalidProtocolException( "too long szGuildMemberIntro length" );

	const BYTE szGuildMemberIntro = (BYTE)m_GuildMemberIntro.size();

	oStream.write( m_GuildID );
	oStream.write( szGuildMemberIntro );

	if ( szGuildMemberIntro > 0 )
		oStream.write( std::span<const char>(m_GuildMemberIntro.data(), szGuildMemberIntro) );

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
std::string CGModifyGuildMemberIntro::toString () const
       throw ()
{
	__BEGIN_TRY
		
	StringStream msg;
    msg << "CGModifyGuildMemberIntro("
		<< "GuildID:" << (int)m_GuildID
		<< "GuildMemberIntro:" << m_GuildMemberIntro
		<< ")";
	return msg.toString();

	__END_CATCH
}
#endif