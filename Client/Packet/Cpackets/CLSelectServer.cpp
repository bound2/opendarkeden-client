//////////////////////////////////////////////////////////////////////////////
// Filename    : CLSelectServer.cpp 
// Written By  : reiot@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CLSelectServer.h"

void CLSelectServer::read (SocketInputStream & iStream)
{
	__BEGIN_TRY

	iStream.read(m_ServerGroupID);

	__END_CATCH
}

void CLSelectServer::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY

	oStream.write(m_ServerGroupID);
//	oStream.write((BYTE)4);

	__END_CATCH
}

