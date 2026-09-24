//--------------------------------------------------------------------------------
// 
// Filename    : GCRequestPowerPointResult.h 
// Written By  : bezz
// Description : 
// 
//--------------------------------------------------------------------------------

#ifndef __GC_REQUEST_POWER_POINT_RESULT_H__
#define __GC_REQUEST_POWER_POINT_RESULT_H__

// include files
#include "Packet.h"
#include "PacketFactory.h"


//--------------------------------------------------------------------------------
//
// class GCRequestPowerPointResult;
//
//--------------------------------------------------------------------------------

class GCRequestPowerPointResult : public Packet
{
public:
	enum RESULT_CODE
	{
		_NO_ERROR = 0,
		SERVER_ERROR,		// The PowerZzang server is alive but is not working properly at the moment
		PROCESS_ERROR,		// Server processing error (e.g. a DB error)
		NO_MEMBER,			// The user is not a PowerZzang member
		NO_POINT,			// No PowerZzang points accumulated
		NO_MATCHING,		// No matching information: the game was not matched on the
							// PowerZzang home page, so the player is shown the sentence
							// that leads to matching there.
		CONNECT_ERROR,		// The connection to the PowerZzang server is failing.
	};

	// The last result code. read() refuses a byte past it, as the server's
	// copy of this packet does.
	static const BYTE kLastResultCode = CONNECT_ERROR;
public:
	GCRequestPowerPointResult();
	~GCRequestPowerPointResult();

public :
    // Read data from the input stream (buffer) and initialise the packet.
    void read(SocketInputStream & iStream);
		    
    // Send the packet's binary image to the output stream (buffer).
    void write(SocketOutputStream & oStream) const;


	// get packet id
	PacketID_t getPacketID() const noexcept { return PACKET_GC_REQUEST_POWER_POINT_RESULT; }
	
	// get packet's body size
	PacketSize_t getPacketSize() const noexcept { return szBYTE + szint + szint; }

#ifdef __DEBUG_OUTPUT__
	// get packet name
	string getPacketName() const { return "GCRequestPowerPointResult"; }
	
	// get packet's debug string
	string toString() const;
#endif
	// get / set Error Code
	BYTE getErrorCode() const { return m_ErrorCode; }
	void setErrorCode( BYTE errorcode ) { m_ErrorCode = errorcode; }

	// get / set SumPowerPoint
	int getSumPowerPoint() const { return m_SumPowerPoint; }
	void setSumPowerPoint( int powerpoint ) { m_SumPowerPoint = powerpoint; }

	// get / set RequestPowerPoint
	int getRequestPowerPoint() const { return m_RequestPowerPoint; }
	void setRequestPowerPoint( int powerpoint ) { m_RequestPowerPoint = powerpoint; }

//--------------------------------------------------
// data members
//--------------------------------------------------
private :
	// Error code
	BYTE	m_ErrorCode;

	// PowerZzang points accumulated so far
	int		m_SumPowerPoint;

	// PowerZzang points fetched by the request
	int		m_RequestPowerPoint;
};


//--------------------------------------------------------------------------------
//
// class GCRequestPowerPointResultFactory;
//
// Factory for GCRequestPowerPointResult
//
//--------------------------------------------------------------------------------

class GCRequestPowerPointResultFactory : public PacketFactory
{
public :
	// create packet
	Packet* createPacket() { return new GCRequestPowerPointResult(); }

	// get packet name
#ifdef __DEBUG_OUTPUT__
	string getPacketName() const { return "GCRequestPowerPointResult"; }
#endif	
	// get packet id
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_GC_REQUEST_POWER_POINT_RESULT; }


	// get packet's max body size
	// *OPTIMIZATION HINT*
	// Define and return const static GCRequestPowerPointResultPacketMaxSize.
	PacketSize_t getPacketMaxSize() const noexcept { return szBYTE + szint + szint; }
};

//--------------------------------------------------------------------------------
//
// class GCRequestPowerPointResultHandler;
//
//--------------------------------------------------------------------------------

class GCRequestPowerPointResultHandler
{
public :
	// execute packet's handler
	static void execute(GCRequestPowerPointResult* pPacket, Player* pPlayer);
};

#endif

