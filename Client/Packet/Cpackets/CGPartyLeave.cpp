//////////////////////////////////////////////////////////////////////////////
// Filename    : CGPartyLeave.cpp 
// Written By  : 김성민
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGPartyLeave.h"

//////////////////////////////////////////////////////////////////////////////
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////////////
void CGPartyLeave::read (SocketInputStream & iStream) 
	 throw (ProtocolException , Error)
{
	__BEGIN_TRY

	BYTE name_length = 0;
	iStream.read(name_length);
	if (name_length > 0)
	{
		iStream.read(m_TargetName, name_length);
	}

	__END_CATCH
}
		    
//////////////////////////////////////////////////////////////////////////////
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////////////
void CGPartyLeave::write (SocketOutputStream & oStream) const 
     throw (ProtocolException , Error)
{
	__BEGIN_TRY

	// Cap before narrowing; the length byte must describe every byte written.
	if (m_TargetName.size() > 10)
		throw InvalidProtocolException("too large name length");

	const BYTE name_length = (BYTE)m_TargetName.size();

	oStream.write(name_length);
	if (name_length > 0)
	{
		oStream.write(std::span<const char>(m_TargetName.data(), name_length));
	}

	__END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// get debug std::string
//////////////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_OUTPUT__
std::string CGPartyLeave::toString () 
	const throw ()
{
	__BEGIN_TRY

	StringStream msg;
	msg << "CGPartyLeave(" 
		<< "TargetName:" << m_TargetName
		<< ")";
	return msg.toString();

	__END_CATCH
}
#endif