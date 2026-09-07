//////////////////////////////////////////////////////////////////////////////
// Filename    : CGAcceptUnion.cpp 
// Written By  : 
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGAcceptUnion.h"


void CGAcceptUnion::read (SocketInputStream & iStream)
{
	__BEGIN_TRY  
		
	iStream.read( m_GuildID );
  
	__END_CATCH
} 
 
void CGAcceptUnion::write (SocketOutputStream & oStream) const 
{
	__BEGIN_TRY 
		   
	oStream.write( m_GuildID );   

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
string CGAcceptUnion::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
    msg << "CGAcceptUnion("
		<< "GuildID:" << m_GuildID
		<< ")" ;
	return msg.toString();

	__END_CATCH
}
#endif