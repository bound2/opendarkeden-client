//////////////////////////////////////////////////////////////////////////////
// Filename    : CGGlobalChat.cpp 
// Written By  : reiot@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGGlobalChat.h"

void CGGlobalChat::read (SocketInputStream & iStream) 
	 throw (ProtocolException , Error)
{
	__BEGIN_TRY
	
	iStream.read( m_Color );
		
	BYTE szMessage;
	iStream.read(szMessage);
	if (szMessage == 0) throw InvalidProtocolException("szMessage == 0");
	if (szMessage > 128) throw InvalidProtocolException("too large message length");
	iStream.read(m_Message , szMessage);

	__END_CATCH
}

void CGGlobalChat::write (SocketOutputStream & oStream) const 
     throw (ProtocolException , Error)
{
	__BEGIN_TRY

	oStream.write( m_Color );

	// Cap the std::string's own size, before narrowing to the length byte.
	if (m_Message.size() > 128) throw InvalidProtocolException("too large message length");

	const BYTE szMessage = (BYTE)m_Message.size();

	if (szMessage == 0) throw InvalidProtocolException("szMessage == 0");

	oStream.write(szMessage);

	oStream.write(std::span<const char>(m_Message.data(), szMessage));

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
std::string CGGlobalChat::toString () const
       throw ()
{
	__BEGIN_TRY
		
	StringStream msg;
	msg << "CGGlobalChat(Color:" << m_Color << ",Message:" << m_Message << ")" ;
	return msg.toString();

	__END_CATCH
}
#endif