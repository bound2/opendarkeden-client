//////////////////////////////////////////////////////////////////////
// 
// Filename    : PacketFactoryManager.h 
// Written By  : reiot@ewestsoft.com
// Description :
// 
//////////////////////////////////////////////////////////////////////

#ifndef __PACKET_FACTORY_MANAGER_H__
#define __PACKET_FACTORY_MANAGER_H__

// include files
#include "Packet.h"
#include "PacketFactory.h"
#include <string>
#include <vector>

// A factory and its packet's name from one token, for init(): the name
// is what the game server link reads (GameServerLink.h), and a factory
// states its own only under __DEBUG_OUTPUT__.
#define DE_PACKET_FACTORY(Name) new Name##Factory(), #Name

//////////////////////////////////////////////////////////////////////
//
// class PacketFactoryManager
//
//////////////////////////////////////////////////////////////////////

class PacketFactoryManager {

public :

	// constructor
	PacketFactoryManager ();

	// destructor
	~PacketFactoryManager ();

	// 패킷팩토리매니저를 초기화한다.
	// 게임서버객체의 init()에서 호출된다.
	void init ();

	// 팩토리 객체를 특정 인덱스에 추가한다.
	// The name is the packet class's, which getPacketName answers in
	// every build (DE_PACKET_FACTORY gives both from one token).
	void addFactory (PacketFactory* pFactory, const char* packetName);
	// The same with the name the factory states under __DEBUG_OUTPUT__,
	// and no name in every other build.
	void addFactory (PacketFactory* pFactory);
	// Ownership transfers only on successful registration; rejected factories
	// remain the caller's responsibility.

	// 패킷아이디로 패킷객체를 생성한다.
	Packet* createPacket (PacketID_t packetID);

	// whether a factory is registered for the id (false past the table)
	bool hasFactory (PacketID_t packetID) const noexcept;

	// the name a registered factory was added with;
	// InvalidProtocolException for an id with no factory
	std::string getPacketName (PacketID_t packetID) const;

#ifdef __DEBUG_OUTPUT__
		// get debug std::string
	std::string toString () const;
#endif


	// 특정 패킷의 최대 크기를 리턴한다.
	PacketSize_t getPacketMaxSize (PacketID_t packetID);



private :
	
	// 패킷팩토리의 배열
	PacketFactory ** m_Factories;

	// 패킷팩토리배열의 크기
	ushort m_Size;

	// the packet name each registered factory was added with, by id
	std::vector<std::string> m_Names;

};

extern PacketFactoryManager* g_pPacketFactoryManager;

#endif
