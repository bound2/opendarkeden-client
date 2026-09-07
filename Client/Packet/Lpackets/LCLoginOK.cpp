//////////////////////////////////////////////////////////////////////
// 
// Filename    : LCLoginOK.cpp
// Written By  : Reiot
// Description : 
// 
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "LCLoginOK.h"

//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
void LCLoginOK::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY

	// 서버 그룹 이름을 읽는다.
	iStream.read( m_isAdult );
	iStream.read( m_bFamily );
	iStream.read( m_Stat );
	iStream.read( m_LastDays );

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
void LCLoginOK::write ( SocketOutputStream & oStream ) const
{
	__BEGIN_TRY

	// 그룹 이름을 쓴다.
	oStream.write( m_isAdult );
	oStream.write( m_bFamily );
	oStream.write( m_Stat );
	
	oStream.write( m_LastDays );

	__END_CATCH
}

PacketSize_t LCLoginOK::getPacketSize() const 
{ 
	return szBYTE + szBYTE + szBYTE + szWORD; 
}