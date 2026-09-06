//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSay.cpp 
// Written By  : reiot@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGSay.h"

#ifdef __GAME_SERVER__
	#include "GamePlayer.h"
#endif

void CGSay::read (SocketInputStream & iStream) 
	 throw (ProtocolException , Error)
{
	__BEGIN_TRY

	iStream.read( m_Color );
		
	BYTE szMessage;

	iStream.read(szMessage);

	if (szMessage == 0)
		throw InvalidProtocolException("szMessage == 0");

	if (szMessage > MAX_MESSAGE_SIZE)
		throw InvalidProtocolException("too large message length");

	iStream.read(m_Message , szMessage);

	__END_CATCH
}

void CGSay::write (SocketOutputStream & oStream) const 
     throw (ProtocolException , Error)
{
	__BEGIN_TRY
		
	oStream.write( m_Color );

	// Bound the message on the std::string's own size, BEFORE narrowing
	// it to the BYTE that goes on the wire. (BYTE)300 is 44, so a
	// 300-byte chat line - 100 Korean characters, which is exactly what
	// the chat box's 100-CHARACTER limit allows - used to narrow below
	// this cap, pass it, and then go out as 300 bytes behind a length
	// byte claiming 44. The peer parses the tail as the next packet.
	if (m_Message.size() > MAX_MESSAGE_SIZE)
		throw InvalidProtocolException("too large message length");

	const BYTE szMessage = (BYTE)m_Message.size();

	if (szMessage == 0)
		throw InvalidProtocolException("szMessage == 0");

	oStream.write(szMessage);

	// The bounded view ties the emitted bytes to the length just
	// written, which is what getPacketSize() advertised in the header.
	oStream.write(std::span<const char>(m_Message.data(), szMessage));

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
std::string CGSay::toString () const
       throw ()
{
	__BEGIN_TRY
		
	StringStream msg;
	msg << "CGSay(Color:" << m_Color << ", Message:" << m_Message << ")" ;
	return msg.toString();

	__END_CATCH
}
#endif