//////////////////////////////////////////////////////////////////////////////
// Filename    : CGStoreSign.cpp 
// Written By  : 
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGStoreSign.h"

void CGStoreSign::read (SocketInputStream & iStream)
{
	__BEGIN_TRY

	BYTE size;
	iStream.read(size);
	iStream.read(m_Sign, size);
		
	__END_CATCH
}

void CGStoreSign::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY

	// Cap before narrowing; the length byte must describe every byte written.
	if (m_Sign.size() > 80)
		throw InvalidProtocolException("too large sign length");

	const BYTE size = (BYTE)m_Sign.size();

	oStream.write(size);
	oStream.write(std::span<const char>(m_Sign.data(), size));

	__END_CATCH
}

string CGStoreSign::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
    msg << "CGStoreSign("
		<< ")" ;
	return msg.toString();

	__END_CATCH
}
