//////////////////////////////////////////////////////////////////////////////
// Filename    : CGRequestStoreInfo.cpp 
// Written By  : 
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGRequestStoreInfo.h"

void CGRequestStoreInfo::read (SocketInputStream & iStream)
{
	__BEGIN_TRY

	iStream.read(m_OwnerObjectID);

	__END_CATCH
}

void CGRequestStoreInfo::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY

	oStream.write(m_OwnerObjectID);

	__END_CATCH
}

string CGRequestStoreInfo::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
    msg << "CGRequestStoreInfo("
		<< ")" ;
	return msg.toString();

	__END_CATCH
}
