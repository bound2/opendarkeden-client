//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSilverCoating.h 
// Written By  : 김성민
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __CG_SILVER_COATING_H__
#define __CG_SILVER_COATING_H__

#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////////////
// class CGSilverCoating
//////////////////////////////////////////////////////////////////////////////

class CGSilverCoating : public Packet 
{
public:
	void read ( SocketInputStream & iStream );
	void write ( SocketOutputStream & oStream ) const;

	PacketID_t   getPacketID () const noexcept   { return PACKET_CG_SILVER_COATING; }
	PacketSize_t getPacketSize () const noexcept { return szObjectID; }

	#ifdef __DEBUG_OUTPUT__
		std::string       getPacketName () const { return "CGSilverCoating"; }
		std::string       toString () const;
	#endif
	
public:
	ObjectID_t getObjectID() noexcept { return m_ObjectID; }
	void setObjectID( ObjectID_t ObjectID ) noexcept { m_ObjectID = ObjectID; }

private:
	ObjectID_t m_ObjectID; // Item Object ID

};


//////////////////////////////////////////////////////////////////////////////
// class CGSilverCoatingFactory;
//////////////////////////////////////////////////////////////////////////////
class CGSilverCoatingFactory : public PacketFactory 
{
public:
	Packet * createPacket () { return new CGSilverCoating(); }

	#ifdef __DEBUG_OUTPUT__
		std::string getPacketName () const { return "CGSilverCoating"; }
	#endif

	PacketID_t getPacketID () const noexcept { return Packet::PACKET_CG_SILVER_COATING; }
	PacketSize_t getPacketMaxSize () const noexcept { return szObjectID; }
};

//////////////////////////////////////////////////////////////////////////////
// class CGSilverCoatingHandler;
//////////////////////////////////////////////////////////////////////////////
#ifndef __GAME_CLIENT__
	class CGSilverCoatingHandler 
	{
	public:
		static void execute ( CGSilverCoating * pPacket , Player * player );
	};
#endif

#endif
