//-------------------------------------------------------------------------------- // 
// Filename    : GCRankBonusInfo.cpp 
// Written By  : elca@ewestsoft.com
// Description : 
// 
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "GCRankBonusInfo.h"
#include "PacketAssert.h"

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
GCRankBonusInfo::GCRankBonusInfo ()
{
}

//--------------------------------------------------------------------------------
// destructor
//--------------------------------------------------------------------------------
GCRankBonusInfo::~GCRankBonusInfo ()
{
	m_RankBonusInfoList.clear();
}

//--------------------------------------------------------------------------------
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//--------------------------------------------------------------------------------
void GCRankBonusInfo::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY

	BYTE ListNum;
	iStream.read( ListNum );

	for ( WORD i = 0; i < ListNum; i++ )
	{
		DWORD rankBonusType;
		iStream.read( rankBonusType );
		m_RankBonusInfoList.push_back( rankBonusType );
	}

	__END_CATCH
}

//--------------------------------------------------------------------------------
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//--------------------------------------------------------------------------------
void GCRankBonusInfo::write ( SocketOutputStream & oStream ) const
{
	__BEGIN_TRY

	//--------------------------------------------------
	// write pc type
	//--------------------------------------------------
	// read() reads this count back into a single BYTE (see below), so match
	// that width here - m_RankBonusInfoList.size() is size_t (64-bit on
	// x64), which is ambiguous between SocketOutputStream::write(uint)/
	// write(ulong) (error C2668) and wasn't the wire format read() expects
	// anyway.
	oStream.write( (BYTE)m_RankBonusInfoList.size() );

	for (std::list<DWORD>::const_iterator itr = m_RankBonusInfoList.begin(); itr!= m_RankBonusInfoList.end(); itr++) 
	{
		oStream.write( (*itr) );
	}

	__END_CATCH
}


#ifdef __DEBUG_OUTPUT__
//--------------------------------------------------------------------------------
// get packet's debug std::string
//--------------------------------------------------------------------------------
std::string GCRankBonusInfo::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
	
	msg << "GCRankBonusInfo(";
	for (std::list<DWORD>::const_iterator itr = m_RankBonusInfoList.begin(); itr!= m_RankBonusInfoList.end(); itr++) 
	{
		msg << (*itr) << ",";
	}
	msg << ")";

	return msg.toString();

	__END_CATCH
}
#endif