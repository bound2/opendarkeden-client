//////////////////////////////////////////////////////////////////////////////
// Filename    : GCNPCAsk.h 
// Written By  : excel96
// Description : 
//////////////////////////////////////////////////////////////////////////////

#ifndef __GC_NPC_ASK_H__
#define __GC_NPC_ASK_H__

#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////////////
// class GCNPCAsk;
// NPC 의 대사를 주변의 PC 들에게 전송한다.
//////////////////////////////////////////////////////////////////////////////

class GCNPCAsk : public Packet 
{
public:
	GCNPCAsk();
	virtual ~GCNPCAsk();

public:
	void read ( SocketInputStream & iStream );
	void write ( SocketOutputStream & oStream ) const;
	PacketID_t getPacketID () const noexcept { return PACKET_GC_NPC_ASK; }
	PacketSize_t getPacketSize () const noexcept { return szObjectID + szScriptID + szNPCID; }

	#ifdef __DEBUG_OUTPUT__
		std::string getPacketName () const { return "GCNPCAsk"; }
		std::string toString () const;
	#endif


public:
	ObjectID_t getObjectID(void) const noexcept { return m_ObjectID; }
	void setObjectID(ObjectID_t creatureID) noexcept { m_ObjectID = creatureID; }

	ScriptID_t getScriptID(void) const noexcept { return m_ScriptID; }
	void setScriptID(ScriptID_t id) noexcept { m_ScriptID = id; }

	NPCID_t		getNPCID(void) const noexcept { return m_NpcID; }
	void		setNPCID(NPCID_t id) noexcept { m_NpcID = id;}

private:
	ObjectID_t m_ObjectID; // NPC's object id
	ScriptID_t m_ScriptID; // script id
	NPCID_t	   m_NpcID;
	
};

//////////////////////////////////////////////////////////////////////////////
// class GCNPCAskFactory;
//////////////////////////////////////////////////////////////////////////////

class GCNPCAskFactory : public PacketFactory 
{
public:
	Packet * createPacket () { return new GCNPCAsk(); }

	#ifdef __DEBUG_OUTPUT__
		std::string getPacketName () const { return "GCNPCAsk"; }
	#endif

	PacketID_t getPacketID () const noexcept { return Packet::PACKET_GC_NPC_ASK; }
	PacketSize_t getPacketMaxSize () const noexcept { return szObjectID + szScriptID + szNPCID; }
};

//////////////////////////////////////////////////////////////////////////////
// class GCNPCAskHandler;
//////////////////////////////////////////////////////////////////////////////

class GCNPCAskHandler 
{
public:
	static void execute ( GCNPCAsk * pPacket , Player * pPlayer );
};

#endif
