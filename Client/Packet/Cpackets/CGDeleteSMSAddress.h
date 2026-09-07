//////////////////////////////////////////////////////////////////////////////
// Filename    : CGDeleteSMSAddress.h 
// Written By  : reiot@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////

#ifndef __CG_DELETE_SMS_ADDRESS_H__
#define __CG_DELETE_SMS_ADDRESS_H__

#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////////////
// class CGDeleteSMSAddress;
//////////////////////////////////////////////////////////////////////////////

class CGDeleteSMSAddress : public Packet 
{
public:
	CGDeleteSMSAddress();
	~CGDeleteSMSAddress();

public:
    void read(SocketInputStream & iStream);
    void write(SocketOutputStream & oStream) const;
	PacketID_t getPacketID() const noexcept { return PACKET_CG_DELETE_SMS_ADDRESS; }
	PacketSize_t getPacketSize() const noexcept { return szDWORD; }
	string getPacketName() const { return "CGDeleteSMSAddress"; }
	string toString() const;
	
public:
	DWORD	getElementID() const { return m_ElementID; }
	void	setElementID( DWORD eID ) { m_ElementID = eID; }

private:
	DWORD	m_ElementID;
};

//////////////////////////////////////////////////////////////////////////////
// class CGDeleteSMSAddressFactory;
//////////////////////////////////////////////////////////////////////////////

class CGDeleteSMSAddressFactory : public PacketFactory 
{
public:
	Packet* createPacket() { return new CGDeleteSMSAddress(); }
	string getPacketName() const { return "CGDeleteSMSAddress"; }
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_CG_DELETE_SMS_ADDRESS; }
	PacketSize_t getPacketMaxSize() const noexcept { return szDWORD; }
};

//////////////////////////////////////////////////////////////////////////////
// class CGDeleteSMSAddressHandler;
//////////////////////////////////////////////////////////////////////////////

class CGDeleteSMSAddressHandler 
{
public:
	static void execute(CGDeleteSMSAddress* pPacket, Player* player);
};

#endif
