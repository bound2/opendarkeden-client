//----------------------------------------------------------------------
// 
// Filename    : CGConnect.cpp 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "CGConnect.h"

#include <span>

//----------------------------------------------------------------------
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//----------------------------------------------------------------------
void CGConnect::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY
		
	//--------------------------------------------------
	// read authentication key
	//--------------------------------------------------
	iStream.read( m_Key );

	//--------------------------------------------------
	// read PC type
	//--------------------------------------------------
	BYTE pcType;
	iStream.read( pcType );
	m_PCType = PCType(pcType);

	//--------------------------------------------------
	// read PC name
	//--------------------------------------------------
	BYTE szPCName;
	iStream.read( szPCName );

	if ( szPCName == 0 )
		throw InvalidProtocolException("szPCName == 0");

	if ( szPCName > 20 )
		throw InvalidProtocolException("too long pc name length");

	iStream.read( m_PCName , szPCName );

	// The whole array, its extent from its declaration.
	iStream.read( std::as_writable_bytes( std::span( m_MacAddress ) ) );

	__END_CATCH
}

		    
//----------------------------------------------------------------------
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//----------------------------------------------------------------------
void CGConnect::write ( SocketOutputStream & oStream ) const
{
	__BEGIN_TRY
		
	//--------------------------------------------------
	// write authentication key
	//--------------------------------------------------
	oStream.write( m_Key );

	//--------------------------------------------------
	// write PC type
	//--------------------------------------------------
	oStream.write( (BYTE)m_PCType );

	//--------------------------------------------------
	// write PC name
	//--------------------------------------------------
	// Cap the std::string's own size, before narrowing to the length byte.
	if ( m_PCName.size() > 20 )
		throw InvalidProtocolException("too long pc name length");

	const BYTE szPCName = (BYTE)m_PCName.size();

	if ( szPCName == 0 )
		throw InvalidProtocolException("szPCName == 0");

	oStream.write( szPCName );

	oStream.write( std::span<const char>(m_PCName.data(), szPCName) );

	oStream.write( std::as_bytes( std::span( m_MacAddress ) ) );

	__END_CATCH
}

//----------------------------------------------------------------------
// get packet's debug std::string
//----------------------------------------------------------------------
#ifdef __DEBUG_OUTPUT__
	std::string CGConnect::toString () const
	{
		StringStream msg;
		
		msg << "CGConnect("
			<< "KEY:" << m_Key 
			<< ",PCType:" << PCType2String[m_PCType] 
			<< ",PCName:" << m_PCName 
			<< ")" ;
		
		return msg.toString();
	}
#endif
