//////////////////////////////////////////////////////////////////////////////
// Filename    : CGUsePowerPoint.h 
// Written By  : bezz
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __CG_USE_POWER_POINT_H__
#define __CG_USE_POWER_POINT_H__

#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////////////
// class CGUsePowerPoint;
//////////////////////////////////////////////////////////////////////////////

class CGUsePowerPoint : public Packet
{
public:
	CGUsePowerPoint ();
	~CGUsePowerPoint ();
	
public:
    void read ( SocketInputStream & iStream );
    void write ( SocketOutputStream & oStream ) const;
    PacketID_t getPacketID () const noexcept { return PACKET_CG_USE_POWER_POINT; }
	PacketSize_t getPacketSize () const noexcept { return 0; }
#ifdef __DEBUG_OUTPUT__
	string getPacketName () const { return "CGUsePowerPoint"; }
	string toString () const;
#endif
};

//////////////////////////////////////////////////////////////////////////////
// class CGUsePowerPointFactory;
//////////////////////////////////////////////////////////////////////////////
class CGUsePowerPointFactory : public PacketFactory 
{
public:
	Packet * createPacket () { return new CGUsePowerPoint(); }
	string getPacketName () const { return "CGUsePowerPoint"; }
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_CG_USE_POWER_POINT; }
	PacketSize_t getPacketMaxSize () const noexcept { return 0; }
};
//////////////////////////////////////////////////////////////////////////////
// class CGUsePowerPointHandler;
//////////////////////////////////////////////////////////////////////////////
#ifndef __GAME_CLIENT__
class CGUsePowerPointHandler 
{
public:
	static void execute ( CGUsePowerPoint * pCGUsePowerPoint , Player * pPlayer );
};

#endif
#endif

