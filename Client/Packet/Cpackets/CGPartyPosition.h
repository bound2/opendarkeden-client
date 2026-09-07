//////////////////////////////////////////////////////////////////////
// 
// Filename    : CGPartyPosition 
// Written By  :
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __CG_PARTY_POSITION_H__
#define __CG_PARTY_POSITION_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"


//////////////////////////////////////////////////////////////////////
//
// class CGPartyPosition;
//
//////////////////////////////////////////////////////////////////////

class CGPartyPosition : public Packet {

public:
	
	// constructor
	CGPartyPosition();
	
	// destructor
	~CGPartyPosition();

	
public:
	
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read(SocketInputStream & iStream);
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write(SocketOutputStream & oStream) const;


	// get packet id
	PacketID_t getPacketID() const noexcept { return PACKET_CG_PARTY_POSITION; }
	
	// get packet's body size
	PacketSize_t getPacketSize() const noexcept { return szZoneID + szZoneCoord * 2 + szHP *2; }

#ifdef __DEBUG_OUTPUT__
	// get packet name
	string getPacketName() const { return "CGPartyPosition"; }
	
	// get packet's debug string
	string toString() const;
#endif
public:
	void	setZoneID(ZoneID_t zoneID) { m_ZoneID = zoneID; }
	ZoneID_t	getZoneID() const { return m_ZoneID; }

	void	setXY(ZoneCoord_t X, ZoneCoord_t Y) { m_X = X; m_Y = Y; }
	void	setHP(HP_t MaxHP, HP_t HP) { m_MaxHP = MaxHP; m_HP = HP;}
	ZoneCoord_t	getX() const { return m_X; }
	ZoneCoord_t	getY() const { return m_Y; }
	HP_t		getMaxHP() const { return m_MaxHP; }
	HP_t		getHP() const { return m_HP; }
	
private :
	ZoneID_t	m_ZoneID;
	ZoneCoord_t	m_X, m_Y;
	HP_t		m_MaxHP;
	HP_t		m_HP;
};


//////////////////////////////////////////////////////////////////////
//
// class CGPartyPositionFactory;
//
// Factory for CGPartyPosition
//
//////////////////////////////////////////////////////////////////////

class CGPartyPositionFactory : public PacketFactory {

public:
	
	// constructor
	CGPartyPositionFactory() {}
	
	// destructor
	virtual ~CGPartyPositionFactory() {}

	
public:
	
	// create packet
	Packet* createPacket() { return new CGPartyPosition(); }

	// get packet name
	string getPacketName() const { return "CGPartyPosition"; }
	
	// get packet id
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_CG_PARTY_POSITION; }

	// get Packet Max Size
	PacketSize_t getPacketMaxSize() const noexcept { return szZoneID + szZoneCoord*2 + szHP*2; }

};

//////////////////////////////////////////////////////////////////////
//
// class CGPartyPositionHandler;
//
//////////////////////////////////////////////////////////////////////

class CGPartyPositionHandler {
	
public:

	// execute packet's handler
	static void execute(CGPartyPosition* pCGPartyPosition, Player* player);
};

#endif
