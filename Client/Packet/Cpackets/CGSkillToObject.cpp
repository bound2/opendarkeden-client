//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSkillToObject.cpp 
// Written By  : elca@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGSkillToObject.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"
#include "PacketAssert.h"

#include <cstdint>

// The staged read below narrows ObjectID_t through an exact-width type.
// Pin the two widths together so a change to ObjectID_t is a compile
// error here rather than a silent change of how many bytes go on the wire.
static_assert(sizeof(ObjectID_t) == sizeof(std::uint32_t),
	"CGSkillToObject stages its target ObjectID as a 32-bit wire scalar");


CGSkillToObject::CGSkillToObject ()
     throw ()
{
	__BEGIN_TRY
	m_CEffectID = 0;
	__END_CATCH
}

CGSkillToObject::~CGSkillToObject () 
    throw ()
{
	__BEGIN_TRY
	__END_CATCH
}

void CGSkillToObject::read (SocketInputStream & iStream) 
	 throw (ProtocolException , Error)
{
	__BEGIN_TRY
		
#ifdef __USE_ENCRYPTER__
	SocketEncryptInputStream* pEIStream = dynamic_cast<SocketEncryptInputStream*>(&iStream);
    Assert(pEIStream!=NULL);

	if (pEIStream->getEncryptCode()!=0)
	{
		SHUFFLE_STATEMENT_3(pEIStream->getEncryptCode(),
							pEIStream->readEncrypt(m_SkillType),
							pEIStream->readEncrypt(m_CEffectID),
							pEIStream->readEncrypt(m_TargetObjectID));
	}
	else 
#endif
	{
		// SkillType_t and CEffectID_t are both WORD, so the wire scalar
		// constraint pins them at their exact 16-bit width.
		iStream.readWire(m_SkillType);
		iStream.readWire(m_CEffectID);

		// ObjectID_t is DWORD, which on this toolchain is unsigned long -
		// four bytes wide, but not one of the exact-width types the
		// constraint accepts. Stage it in the exact-width equivalent the
		// static_assert above ties to it, exactly as CGAttack does.
		std::uint32_t targetObjectID = 0;
		iStream.readWire(targetObjectID);
		m_TargetObjectID = static_cast<ObjectID_t>(targetObjectID);
	}

	__END_CATCH
}

void CGSkillToObject::write (SocketOutputStream & oStream) const 
     throw (ProtocolException , Error)
{
	__BEGIN_TRY
		
#ifdef __USE_ENCRYPTER__
	SocketEncryptOutputStream* pEOStream = dynamic_cast<SocketEncryptOutputStream*>(&oStream);
    Assert(pEOStream!=NULL);

	if (pEOStream->getEncryptCode()!=0)
	{
		SHUFFLE_STATEMENT_3(pEOStream->getEncryptCode(),
							pEOStream->writeEncrypt(m_SkillType),
							pEOStream->writeEncrypt(m_CEffectID),
							pEOStream->writeEncrypt(m_TargetObjectID));
	}
	else
#endif
	{
		oStream.writeWire(m_SkillType);
		oStream.writeWire(m_CEffectID);
		oStream.writeWire(static_cast<std::uint32_t>(m_TargetObjectID));
	}

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
std::string CGSkillToObject::toString () const
       throw ()
{
	__BEGIN_TRY
		
	StringStream msg;
    msg << "CGSkillToObject("
		<< "SkillType:"      << (int)m_SkillType 
		<< ",CEffectID:"     << (int)m_CEffectID 
		<< ",TargetSelfID :" << (int)m_TargetObjectID 
		<< ")" ;
	return msg.toString();

	__END_CATCH
}
#endif