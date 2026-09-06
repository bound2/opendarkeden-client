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
		
	BYTE szTargetName = m_TargetName.size();

	if ( szTargetName == 0 )
		throw InvalidProtocolException( "szTargetName == 0" );
	if ( szTargetName > 20 )
		throw InvalidProtocolException( "too long target name" );

	oStream.writeWire(m_SkillType);
	oStream.writeWire(m_CEffectID);
	oStream.write( szTargetName );
	// The same bytes the std::string overload emitted: every byte of the
	// name, through the bounded view.
	oStream.write( std::span<const char>(m_TargetName.data(), m_TargetName.size()) );

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