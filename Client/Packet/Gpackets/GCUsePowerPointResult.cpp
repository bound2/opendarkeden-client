//-------------------------------------------------------------------------------- // 
// Filename    : GCUsePowerPointResult.cpp 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "GCUsePowerPointResult.h"


//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
GCUsePowerPointResult::GCUsePowerPointResult ()
	: m_ErrorCode(0), m_ItemCode(0), m_PowerPoint(0)
{
}

//--------------------------------------------------------------------------------
// destructor
//--------------------------------------------------------------------------------
GCUsePowerPointResult::~GCUsePowerPointResult ()
{
}

//--------------------------------------------------------------------------------
// Read data from the input stream (buffer) and initialise the packet.
//--------------------------------------------------------------------------------
void GCUsePowerPointResult::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY

	// Error code
	iStream.read( m_ErrorCode );
	if ( m_ErrorCode > kLastResultCode )
		throw InvalidProtocolException("power point result code out of range");

	// Item Code
	iStream.read( m_ItemCode );
	if ( m_ItemCode > kLastItemCode )
		throw InvalidProtocolException("power point item code out of range");

	// Power Point
	iStream.read( m_PowerPoint );

	__END_CATCH
}

		    
//--------------------------------------------------------------------------------
// Send the packet's binary image to the output stream (buffer).
//--------------------------------------------------------------------------------
void GCUsePowerPointResult::write ( SocketOutputStream & oStream ) const
{
	__BEGIN_TRY

	// Error code
	oStream.write( m_ErrorCode );

	// Item Code
	oStream.write( m_ItemCode );

	// Power Point
	oStream.write( m_PowerPoint );

	__END_CATCH
}


#ifdef __DEBUG_OUTPUT__
//--------------------------------------------------------------------------------
// get packet's debug string
//--------------------------------------------------------------------------------
string GCUsePowerPointResult::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
	
	msg << "GCUsePowerPointResult("
		<< "ErrorCode:" << (int)m_ErrorCode
		<< ",ItemCode:" << (int)m_ItemCode
		<< ",PowerPoint:" << (int)m_PowerPoint
		<< ")";

	return msg.toString();

	__END_CATCH
}

#endif