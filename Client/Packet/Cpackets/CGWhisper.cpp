//////////////////////////////////////////////////////////////////////////////
// Filename    : CGWhisper.cpp 
// Written By  : reiot@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGWhisper.h"

void CGWhisper::read (SocketInputStream & iStream) 
	 throw (ProtocolException , Error)
{
	__BEGIN_TRY
		
	// 이름 읽기
	BYTE szName;

	iStream.read(szName);

	if (szName == 0)
		throw InvalidProtocolException("szName == 0");

	if (szName > 10)
		throw InvalidProtocolException("too large name length");

	iStream.read(m_Name , szName);
	iStream.read( m_Color );

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
		    
void CGWhisper::write (SocketOutputStream & oStream) const 
     throw (ProtocolException , Error)
{
	__BEGIN_TRY
		
	// Write the name. Both caps below run on the std::string's own size,
	// BEFORE the narrowing to the BYTE that goes on the wire: (BYTE)300
	// is 44, so a 300-byte name or message used to pass its cap and then
	// go out whole behind a length byte claiming 44, leaving the peer to
	// parse the tail as the next packet. (read() bounds the name at 10,
	// not 128; that asymmetry is upstream's and is left alone.)
	if (m_Name.size() > 128)
		throw InvalidProtocolException("too large name length");

	const BYTE szName = (BYTE)m_Name.size();

	if (szName == 0)
		throw InvalidProtocolException("szName == 0");

	oStream.write(szName);

	oStream.write(std::span<const char>(m_Name.data(), szName));

	oStream.write( m_Color );

	// Write the message.
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
std::string CGWhisper::toString () const
       throw ()
{
	__BEGIN_TRY
		
	StringStream msg;
	msg << "CGWhisper(Name :" << m_Name 
		<< ", Color : " << m_Color
		<< ", Message : " << m_Message
		<< ")" ;
	return msg.toString();

	__END_CATCH
}
#endif