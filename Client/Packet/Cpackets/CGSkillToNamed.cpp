//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSkillToNamed.cpp 
// Written By  : elca@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGSkillToNamed.h"

#include <span>

CGSkillToNamed::CGSkillToNamed ()
     throw ()
{
	__BEGIN_TRY
	__END_CATCH
}

CGSkillToNamed::~CGSkillToNamed () 
    throw ()
{
	__BEGIN_TRY
	__END_CATCH
}

void CGSkillToNamed::read (SocketInputStream & iStream) 
	 throw (ProtocolException , Error)
{
	__BEGIN_TRY

	BYTE szTargetName;

	// SkillType_t and CEffectID_t are both WORD, so the wire scalar
	// constraint pins them at the exact 16-bit width szSkillType and
	// szCEffectID asked for.
	iStream.readWire(m_SkillType);
	iStream.readWire(m_CEffectID);
	iStream.read( szTargetName );

	if ( szTargetName == 0 )
		throw InvalidProtocolException( "szTargetName == 0" );
	if ( szTargetName > 20 )
		throw InvalidProtocolException( "too long target name length" );

	iStream.read( m_TargetName, szTargetName );

	__END_CATCH
}

void CGSkillToNamed::write (SocketOutputStream & oStream) const 
     throw (ProtocolException , Error)
{
	__BEGIN_TRY
		
	// Bound the name on the std::string's own size, BEFORE narrowing it to
	// the BYTE that goes on the wire. A 276-character name narrows to 20
	// and used to pass the cap, after which write() emitted all 276 bytes
	// behind a length byte that claimed 20 - this repo's frame-bounded
	// reader rejects such a tail, the server's legacy reader parses it as
	// the next packet. read() cannot produce such a name, but
	// setTargetName() takes any std::string. (SocketOutputStream has
	// already written the framing header when this throws, as it has for
	// every throwing write(); that residue is the same shape as before.)
	if ( m_TargetName.size() > 20 )
		throw InvalidProtocolException( "too long target name" );

	const BYTE szTargetName = (BYTE)m_TargetName.size();

	if ( szTargetName == 0 )
		throw InvalidProtocolException( "szTargetName == 0" );

	oStream.writeWire(m_SkillType);
	oStream.writeWire(m_CEffectID);
	oStream.write( szTargetName );
	// The bounded view ties the emitted bytes to the length just written,
	// which is what getPacketSize() advertised in the framing header.
	oStream.write( std::span<const char>(m_TargetName.data(), szTargetName) );

	__END_CATCH
}


#ifdef __DEBUG_OUTPUT__
std::string CGSkillToNamed::toString () const
       throw ()
{
	__BEGIN_TRY
		
	StringStream msg;
	msg << "CGSkillToNamed("
		<< "SkillType:" << (int)m_SkillType 
		<< ",CEffectID:" << (int)m_CEffectID 
		<< ",TargetName:" << m_TargetName
		<< ")" ;
	return msg.toString();

	__END_CATCH
}
#endif