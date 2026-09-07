//////////////////////////////////////////////////////////////////////
// 
// Filename    : LCPCList.h 
// Written By  : Reiot
// Description :
// 
//////////////////////////////////////////////////////////////////////

#ifndef __LC_PC_LIST_H__
#define __LC_PC_LIST_H__

// include files
#include "Packet.h"
#include "PacketFactory.h"
#include "PCInfo.h"
#include "Packet/PCSlayerInfo.h"
#include "Packet/PCVampireInfo.h"
#include "Packet/PCOustersInfo.h"

//////////////////////////////////////////////////////////////////////
//
// class LCPCList;
//
//////////////////////////////////////////////////////////////////////

class LCPCList : public Packet {

public :

	// constructor
	// PCInfo* 배열에 각각 NULL을 지정한다.
	LCPCList ();

	// destructor
	// PCInfo* 배열에 할당된 객체를 삭제한다.
	~LCPCList ();
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_LC_PC_LIST; }
	
	// get packet's body size
	PacketSize_t getPacketSize () const;
	
	#ifdef __DEBUG_OUTPUT__
		// get packet's name
		std::string getPacketName () const { return "LCPCList"; }
		
		// get packet's debug string
		std::string toString () const;
	#endif
	
public :

	// get/set pc info
	PCInfo * getPCInfo ( Slot slot ) const 
	{ 
		if ( m_pPCInfos[ slot ] == NULL )
			throw NoSuchElementException("no such PC exist in that slot");

		return m_pPCInfos[slot];
	}

	void setPCInfo ( Slot slot , PCInfo * pPCInfo ) 
	{ 
		if ( m_pPCInfos[ slot ] != NULL )
			throw DuplicatedException("PCInfo duplicated.");
		m_pPCInfos[slot] = pPCInfo;
	}

private : 

	// 캐릭터 정보
	PCInfo * m_pPCInfos[ SLOT_MAX ];

};


//////////////////////////////////////////////////////////////////////
//
// class LCPCListFactory;
//
// Factory for LCPCList
//
//////////////////////////////////////////////////////////////////////

class LCPCListFactory : public PacketFactory {

public :
	
	// create packet
	Packet * createPacket () { return new LCPCList(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "LCPCList"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_LC_PC_LIST; }

	// get packet's max body size
	PacketSize_t getPacketMaxSize () const 
	{ 
		// 슬레이어 정보가 뱀파이어 정보보다 사이즈가 크기 때문에,
		// 이 패킷의 최대 크기는 슬레이어 3 명일 경우이다.
		return SLOT_MAX * sizeof(char) + PCSlayerInfo::getMaxSize() * SLOT_MAX; 
	}
	
};


//////////////////////////////////////////////////////////////////////
//
// class LCPCListHandler;
//
//////////////////////////////////////////////////////////////////////

class LCPCListHandler {

public :

	// execute packet's handler
	static void execute ( LCPCList * pPacket , Player * pPlayer );

};

#endif
