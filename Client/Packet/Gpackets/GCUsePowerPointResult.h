//--------------------------------------------------------------------------------
// 
// Filename    : GCUsePowerPointResult.h 
// Written By  : bezz
// Description : 
// 
//--------------------------------------------------------------------------------

#ifndef __GC_USE_POWER_POINT_RESULT_H__
#define __GC_USE_POWER_POINT_RESULT_H__

// include files
#include "Packet.h"
#include "PacketFactory.h"


//--------------------------------------------------------------------------------
//
// class GCUsePowerPointResult;
//
//--------------------------------------------------------------------------------

class GCUsePowerPointResult : public Packet
{
public:

	enum RESULT_CODE
	{
		_NO_ERROR = 0,
		NOT_ENOUGH_POWER_POINT,		// Not enough power points.
		NOT_ENOUGH_INVENTORY_SPACE	// There is not enough room in the inventory.
	};

	enum ITEM_CODE
	{
		CANDY = 0,				// One candy
		RESURRECTION_SCROLL,	// One resurrection scroll
		ELIXIR_SCROLL,			// One elixir scroll
		MEGAPHONE,				// Thirty minutes of the megaphone
		NAMING_PEN,				// One naming pen
		SIGNPOST,				// Six hours of the notice board
		BLACK_RICE_CAKE_SOUP	// One black rice cake soup
	};

	// The last code of each list. read() refuses a byte past it, as the
	// server's copy of this packet does.
	static const BYTE kLastResultCode = NOT_ENOUGH_INVENTORY_SPACE;
	static const BYTE kLastItemCode = BLACK_RICE_CAKE_SOUP;

public:
	GCUsePowerPointResult();
	~GCUsePowerPointResult();

public :
    // Read data from the input stream (buffer) and initialise the packet.
    void read(SocketInputStream & iStream);

    // Send the packet's binary image to the output stream (buffer).
    void write(SocketOutputStream & oStream) const;


	// get packet id
	PacketID_t getPacketID() const noexcept { return PACKET_GC_USE_POWER_POINT_RESULT; }
	
	// get packet's body size
	PacketSize_t getPacketSize() const noexcept { return szBYTE + szBYTE + szDWORD; }
#ifdef __DEBUG_OUTPUT__
	// get packet name
	string getPacketName() const { return "GCUsePowerPointResult"; }
	
	// get packet's debug string
	string toString() const;
#endif
	// get / set Error Code
	BYTE getErrorCode() const { return m_ErrorCode; }
	void setErrorCode( BYTE errorcode ) { m_ErrorCode = errorcode; }

	// get / set Item Code
	BYTE getItemCode() const { return m_ItemCode; }
	void setItemCode( BYTE itemcode ) { m_ItemCode = itemcode; }

	// get / set Power Point
	uint getPowerPoint() const { return m_PowerPoint; }
	void setPowerPoint( uint powerpoint ) { m_PowerPoint = powerpoint; }

//--------------------------------------------------
// data members
//--------------------------------------------------
private :
	// Error code
	BYTE	m_ErrorCode;

	// Item code
	BYTE	m_ItemCode;

	// Power points
	uint	m_PowerPoint;
};


//--------------------------------------------------------------------------------
//
// class GCUsePowerPointResultFactory;
//
// Factory for GCUsePowerPointResult
//
//--------------------------------------------------------------------------------

class GCUsePowerPointResultFactory : public PacketFactory
{
public :
	// create packet
	Packet* createPacket() { return new GCUsePowerPointResult(); }
#ifdef __DEBUG_OUTPUT__
	// get packet name
	string getPacketName() const { return "GCUsePowerPointResult"; }
#endif
	// get packet id
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_GC_USE_POWER_POINT_RESULT; }

	// get packet's max body size
	// *OPTIMIZATION HINT*
	// Define and return const static GCUsePowerPointResultPacketMaxSize.
	PacketSize_t getPacketMaxSize() const noexcept { return szBYTE + szBYTE + szDWORD; }
};

//--------------------------------------------------------------------------------
//
// class GCUsePowerPointResultHandler;
//
//--------------------------------------------------------------------------------

class GCUsePowerPointResultHandler
{
public :
	// execute packet's handler
	static void execute(GCUsePowerPointResult* pPacket, Player* pPlayer);
};

#endif

