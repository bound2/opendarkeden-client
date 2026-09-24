//-------------------------------------------------------------------------------- // 
// Filename    : GCRequestPowerPointResult.cpp 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "GCRequestPowerPointResult.h"


//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
GCRequestPowerPointResult::GCRequestPowerPointResult ()
	: m_ErrorCode(0), m_SumPowerPoint(0), m_RequestPowerPoint(0)
{
}

//--------------------------------------------------------------------------------
// destructor
//--------------------------------------------------------------------------------
GCRequestPowerPointResult::~GCRequestPowerPointResult ()
{
}

//--------------------------------------------------------------------------------
// Read data from the input stream (buffer) and initialise the packet.
//--------------------------------------------------------------------------------
void GCRequestPowerPointResult::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY

	// Error code
	iStream.read( m_ErrorCode );
	if ( m_ErrorCode > kLastResultCode )
		throw InvalidProtocolException("power point result code out of range");

	// PowerZzang points accumulated so far
	iStream.read( m_SumPowerPoint );

	// PowerZzang points fetched by the request
	iStream.read( m_RequestPowerPoint );
	__END_CATCH
}

		    
//--------------------------------------------------------------------------------
// Send the packet's binary image to the output stream (buffer).
//--------------------------------------------------------------------------------
void GCRequestPowerPointResult::write ( SocketOutputStream & oStream ) const
{
	__BEGIN_TRY

	// Error code
	oStream.write( m_ErrorCode );

	// PowerZzang points accumulated so far
	oStream.write( m_SumPowerPoint );

	// PowerZzang points fetched by the request
	oStream.write( m_RequestPowerPoint );
	__END_CATCH
}

//--------------------------------------------------------------------------------
// get packet's debug string
//--------------------------------------------------------------------------------
#ifdef __DEBUG_OUTPUT__
string GCRequestPowerPointResult::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
	
	msg << "GCRequestPowerPointResult("
		<< "ErrorCode:" << (int)m_ErrorCode
		<< ",SumPowerPoint:" << m_SumPowerPoint
		<< ",RequestPowerPoint:" << m_RequestPowerPoint
		<< ")";


	return msg.toString();

	__END_CATCH
}

#endif