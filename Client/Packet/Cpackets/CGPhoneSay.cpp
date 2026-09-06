//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGPhoneSay.cpp 
// Written By  : elca@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "CGPhoneSay.h"


//////////////////////////////////////////////////////////////////////
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////
void CGPhoneSay::read ( SocketInputStream & iStream ) 
	 throw ( ProtocolException , Error )
{
	__BEGIN_TRY
		
	BYTE szMessage;

	iStream.read( m_SlotID );

	iStream.read( szMessage );

	if ( szMessage == 0 )
		throw InvalidProtocolException("szMessage == 0");

	if ( szMessage > 128 )
		throw InvalidProtocolException("too large message length");

	iStream.read( m_Message , szMessage );

	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////
void CGPhoneSay::write ( SocketOutputStream & oStream ) const 
     throw ( ProtocolException , Error )
{
	__BEGIN_TRY
	
	oStream.write( m_SlotID );
		
	// The cap runs on the std::string's own size, BEFORE the narrowing:
	// (BYTE)300 is 44, so a 300-byte message used to pass this test and
	// then go out whole behind a length byte claiming 44.
	if ( m_Message.size() > 128 )
		throw InvalidProtocolException("too large message length");

	const BYTE szMessage = (BYTE)m_Message.size();

	if ( szMessage == 0 )
		throw InvalidProtocolException("szMessage == 0");

	oStream.write( szMessage );

	oStream.write( std::span<const char>(m_Message.data(), szMessage) );

	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
// get packet's debug std::string
//////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_OUTPUT__
	std::string CGPhoneSay::toString () const
		   throw ()
	{
		__BEGIN_TRY
			
		StringStream msg;
		
		msg << "CGPhoneSay( SlotID :" << (int)m_SlotID
			<< ",Message:" << m_Message
			<< ")" ;
		
		return msg.toString();

		__END_CATCH
	}
#endif