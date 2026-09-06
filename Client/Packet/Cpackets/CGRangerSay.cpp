//////////////////////////////////////////////////////////////////////////////
// Filename    : CGWhisper.cpp 
// Written By  : reiot@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGRangerSay.h"

void CGRangerSay::read (SocketInputStream & iStream) 
	 throw (ProtocolException , Error)
{
	__BEGIN_TRY
		
	// 메세지 읽기
	BYTE szMessage;

	iStream.read(szMessage);

	if (szMessage == 0)
		throw InvalidProtocolException("szMessage == 0");

	if (szMessage > 128)
		throw InvalidProtocolException("too large message length");

	iStream.read(m_Message , szMessage);

	__END_CATCH
}
		    
void CGRangerSay::write (SocketOutputStream & oStream) const 
     throw (ProtocolException , Error)
{
	__BEGIN_TRY
		
	// Write the message. The cap runs on the std::string's own size,
	// BEFORE the narrowing: (BYTE)300 is 44, so a 300-byte line used to
	// pass this test and then go out whole behind a length byte
	// claiming 44.
	if (m_Message.size() > 128)
		throw InvalidProtocolException("too large message length");

	const BYTE szMessage = (BYTE)m_Message.size();

	if (szMessage == 0)
		throw InvalidProtocolException("szMessage == 0");

	oStream.write(szMessage);

	oStream.write(std::span<const char>(m_Message.data(), szMessage));

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
string CGRangerSay::toString () const
       throw ()
{
	__BEGIN_TRY
		
	StringStream msg;
	msg << "( CGRangerSay : " << m_Message
		<< ")" ;
	return msg.toString();

	__END_CATCH
}
#endif