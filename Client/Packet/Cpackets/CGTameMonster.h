//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGTameMonster 
// Written By  : elca@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __CG_TAME_MONSTER_H__
#define __CG_TAME_MONSTER_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class CGTameMonster;
//
//////////////////////////////////////////////////////////////////////

class CGTameMonster : public Packet {

public:
	CGTameMonster();
	~CGTameMonster();
	
public:
    void read(SocketInputStream & iStream);
    void write(SocketOutputStream & oStream) const;
	PacketID_t getPacketID() const noexcept { return PACKET_CG_TAME_MONSTER; }
	PacketSize_t getPacketSize() const noexcept { return szObjectID; }

#ifdef __DEBUG_OUTPUT__
	std::string getPacketName() const { return "CGTameMonster"; }
	std::string toString() const;
#endif
	
public:
	// get/set ObjectID
	ObjectID_t getObjectID() const noexcept { return m_ObjectID; }
	void setObjectID(ObjectID_t ObjectID) noexcept { m_ObjectID = ObjectID; }
	
private :
	
	ObjectID_t m_ObjectID;  // ObjectID
};

//////////////////////////////////////////////////////////////////////
//
// class CGTameMonsterFactory;
//
// Factory for CGTameMonster
//
//////////////////////////////////////////////////////////////////////

class CGTameMonsterFactory : public PacketFactory {

public:
	CGTameMonsterFactory() {}
	virtual ~CGTameMonsterFactory() {}

public:
	Packet* createPacket() { return new CGTameMonster(); }
	std::string getPacketName() const { return "CGTameMonster"; }
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_CG_TAME_MONSTER; }
	PacketSize_t getPacketMaxSize() const noexcept { return szObjectID; }
};

#ifndef __GAME_CLIENT__
//////////////////////////////////////////////////////////////////////
//
// class CGTameMonsterHandler;
//
//////////////////////////////////////////////////////////////////////

class CGTameMonsterHandler {
	
public:
	static void execute(CGTameMonster* pCGTameMonster, Player* player);
};

#endif

#endif