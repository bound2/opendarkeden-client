//////////////////////////////////////////////////////////////////////
// 
// Filename    : CLDeletePC.cpp 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "CLDeletePC.h"


//////////////////////////////////////////////////////////////////////
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////
void CLDeletePC::read ( SocketInputStream & iStream ) 
	 throw ( ProtocolException , Error )
{
	__BEGIN_TRY
		
	BYTE szName;
	iStream.read( szName );

	if ( szName == 0 )
		throw InvalidProtocolException("szName == 0");

	if ( szName > 20 )
		throw InvalidProtocolException("too long name length");

	iStream.read( m_Name , szName );

	BYTE slot;
	iStream.read( slot );
	m_Slot = Slot(slot);

	BYTE szSSN;
	iStream.read( szSSN );

	if ( szSSN == 0 )
		throw InvalidProtocolException("szSSN == 0");

	if ( szSSN > 18 )
		throw InvalidProtocolException("too long name length");

	iStream.read( m_SSN, szSSN );

	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////
void CLDeletePC::write ( SocketOutputStream & oStream ) const 
     throw ( ProtocolException , Error )
{
	__BEGIN_TRY
		
	// Both caps run on the std::string's own size, BEFORE the narrowing
	// to the BYTE that goes on the wire: (BYTE)276 is 20, so a
	// 276-character name used to pass its cap and then go out whole
	// behind a length byte claiming 20.
	if ( m_Name.size() > 20 )
		throw InvalidProtocolException("too long name length");

	const BYTE szName = (BYTE)m_Name.size();

	if ( szName == 0 )
		throw InvalidProtocolException("szName == 0");

	oStream.write( szName );
	oStream.write( std::span<const char>(m_Name.data(), szName) );

	oStream.write( (BYTE)m_Slot );

	if ( m_SSN.size() > 18 )
		throw InvalidProtocolException("too long name length");

	const BYTE szSSN = (BYTE)m_SSN.size();

	if ( szSSN== 0 )
		throw InvalidProtocolException("szSSN == 0");

	oStream.write( szSSN );
	oStream.write( std::span<const char>(m_SSN.data(), szSSN) );

	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// get packet's debug std::string
//
//////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_OUTPUT__
	std::string CLDeletePC::toString () const
		   throw ()
	{
		__BEGIN_TRY

		StringStream msg;
		
		msg << "CLDeletePC(Name:" << m_Name
			<< ",Slot:" << (int)m_Slot
			<< ",SSN:" << m_SSN
			<< ")" ;
		
		return msg.toString();

		__END_CATCH
	}

#endif