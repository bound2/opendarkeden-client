//////////////////////////////////////////////////////////////////////////////
// Filename    : CGRequestPowerPoint.cpp 
// Written By  :
// Description :
// 서버에 원하는 사람의 IP 요청
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGRequestPowerPoint.h"

//////////////////////////////////////////////////////////////////////////////
// class CGRequestPowerPoint member methods
//////////////////////////////////////////////////////////////////////////////

CGRequestPowerPoint::CGRequestPowerPoint () 
     throw ()
{
	__BEGIN_TRY
	__END_CATCH
}
	
CGRequestPowerPoint::~CGRequestPowerPoint () 
    throw ()
{
	__BEGIN_TRY
	__END_CATCH
}

void CGRequestPowerPoint::read ( SocketInputStream & iStream ) 
	 throw ( ProtocolException , Error )
{
	__BEGIN_TRY

	BYTE szCellNum;
	iStream.read( szCellNum );

	if ( szCellNum == 0 )
		throw InvalidProtocolException( "szCellNum == 0" );

	if ( szCellNum > 12 )
		throw InvalidProtocolException( "szCellNum > 12" );

	iStream.read( m_CellNum, szCellNum );

	__END_CATCH
}
		    
void CGRequestPowerPoint::write ( SocketOutputStream & oStream ) 
     const throw ( ProtocolException , Error )
{
	__BEGIN_TRY
		
	// The cap runs on the std::string's own size, BEFORE the narrowing
	// to the BYTE that goes on the wire: (BYTE)268 is 12, so a
	// 268-character number used to pass this test and then go out whole
	// behind a length byte claiming 12.
	if ( m_CellNum.size() > 12 )
		throw InvalidProtocolException( "szCellNum > 12" );

	const BYTE szCellNum = (BYTE)m_CellNum.size();

	if ( szCellNum == 0 )
		throw InvalidProtocolException( "szCellNum == 0" );

	oStream.write( szCellNum );
	oStream.write( std::span<const char>(m_CellNum.data(), szCellNum) );

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
string CGRequestPowerPoint::toString () 
	const throw ()
{
	__BEGIN_TRY

	StringStream msg;
	msg << "CGRequestPowerPoint( "
		<< ",CellNum: " << m_CellNum
		<< ")";
	return msg.toString();

	__END_CATCH
}
#endif