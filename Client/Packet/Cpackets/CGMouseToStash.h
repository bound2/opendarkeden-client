////////////////////////////////////////////////////////////////////////////////
// Filename    : CGMouseToStash.h 
// Written By  : 김성민
// Description : 
////////////////////////////////////////////////////////////////////////////////

#ifndef __CG_MOUSE_TO_STASH_H__
#define __CG_MOUSE_TO_STASH_H__

// include files
#include "Packet.h"
#include "PacketFactory.h"

////////////////////////////////////////////////////////////////////////////////
//
// class CGMouseToStash
//
////////////////////////////////////////////////////////////////////////////////

class CGMouseToStash : public Packet 
{
public:
    void         read (SocketInputStream & iStream);
    void         write (SocketOutputStream & oStream) const;
	PacketID_t   getPacketID () const noexcept { return PACKET_CG_MOUSE_TO_STASH; }
	PacketSize_t getPacketSize () const noexcept { return szObjectID + szBYTE*2; }

	#ifdef __DEBUG_OUTPUT__
		std::string       getPacketName () const { return "CGMouseToStash"; }
		std::string       toString () const;
	#endif
	
public:
	ObjectID_t getObjectID(void) noexcept { return m_ObjectID; }
	void       setObjectID(ObjectID_t ObjectID) noexcept { m_ObjectID = ObjectID; }
	BYTE       getRack(void) noexcept { return m_Rack;}
	void       setRack(BYTE rack) noexcept { m_Rack = rack;}
	BYTE       getIndex(void) noexcept { return m_Index;}
	void       setIndex(BYTE index) noexcept { m_Index = index;}

private:
	ObjectID_t m_ObjectID;
	BYTE       m_Rack;
	BYTE       m_Index;

};


////////////////////////////////////////////////////////////////////////////////
//
// class CGMouseToStashFactory;
//
////////////////////////////////////////////////////////////////////////////////
class CGMouseToStashFactory : public PacketFactory 
{
public :
	Packet*      createPacket () { return new CGMouseToStash(); }

	#ifdef __DEBUG_OUTPUT__
		std::string       getPacketName () const { return "CGMouseToStash"; }
	#endif

	PacketID_t   getPacketID () const noexcept { return Packet::PACKET_CG_MOUSE_TO_STASH; }
	PacketSize_t getPacketMaxSize () const noexcept { return szObjectID+szBYTE*2; }

};

////////////////////////////////////////////////////////////////////////////////
//
// class CGMouseToStashHandler;
//
////////////////////////////////////////////////////////////////////////////////
#ifndef __GAME_CLIENT__
	class CGMouseToStashHandler 
	{
	public :
		static void execute(CGMouseToStash * pPacket, Player * player);
		static void executeSlayer(CGMouseToStash * pPacket, Player * player);
		static void executeVampire(CGMouseToStash * pPacket, Player * player);
	};
#endif

#endif
