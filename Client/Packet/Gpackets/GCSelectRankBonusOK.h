//////////////////////////////////////////////////////////////////////
// 
// Filename    :  GCSelectRankBonusOK.h 
// Written By  :  elca@ewestsoft.com
// Description :  Å
//                
// 
//////////////////////////////////////////////////////////////////////

#ifndef __GC_SELECT_RANK_BONUS_OK_H__
#define __GC_SELECT_RANK_BONUS_OK_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class  GCSelectRankBonusOK;
//
//////////////////////////////////////////////////////////////////////

class GCSelectRankBonusOK : public Packet {

public :
	
	// constructor
	GCSelectRankBonusOK();
	
	// destructor
	~GCSelectRankBonusOK();

	
public :
	
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read(SocketInputStream & iStream);
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write(SocketOutputStream & oStream) const;


	// get packet id
	PacketID_t getPacketID() const noexcept { return PACKET_GC_SELECT_RANK_BONUS_OK; }
	
	// get packet size
	PacketSize_t getPacketSize() const noexcept { return szDWORD; }
	
#ifdef __DEBUG_OUTPUT__
	// get packet's name
	std::string getPacketName() const { return "GCSelectRankBonusOK"; }
	
	// get packet's debug std::string
	std::string toString() const;
#endif
	
	// get/set m_RankBonusType
	DWORD getRankBonusType() const noexcept { return m_RankBonusType; }
	void setRankBonusType(DWORD rankBonusType) noexcept { m_RankBonusType = rankBonusType; }

private : 

	// RankBonusType
	DWORD m_RankBonusType;
};


//////////////////////////////////////////////////////////////////////
//
// class  GCSelectRankBonusOKFactory;
//
// Factory for  GCSelectRankBonusOK
//
//////////////////////////////////////////////////////////////////////

class  GCSelectRankBonusOKFactory : public PacketFactory {

public :
	
	// constructor
	 GCSelectRankBonusOKFactory() {}
	
	// destructor
	virtual ~GCSelectRankBonusOKFactory() {}

	
public :
	
	// create packet
	Packet* createPacket() { return new GCSelectRankBonusOK(); }

	// get packet name
	std::string getPacketName() const { return "GCSelectRankBonusOK"; }
	
	// get packet id
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_GC_SELECT_RANK_BONUS_OK; }

	// get Packet Max Size
	PacketSize_t getPacketMaxSize() const noexcept { return szDWORD; }

};


//////////////////////////////////////////////////////////////////////
//
// class  GCSelectRankBonusOKHandler;
//
//////////////////////////////////////////////////////////////////////

class  GCSelectRankBonusOKHandler {

public :

	// execute packet's handler
	static void execute( GCSelectRankBonusOK* pGCSelectRankBonusOK, Player* pPlayer);

};

#endif
