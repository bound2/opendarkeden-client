//////////////////////////////////////////////////////////////////////
// 
// Filename    :  GCDownSkillOK.h 
// Written By  :  elca@ewestsoft.com
// Description :  Å
//                
// 
//////////////////////////////////////////////////////////////////////

#ifndef __GC_DOWN_SKILL_OK_H__
#define __GC_DOWN_SKILL_OK_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class  GCDownSkillOK;
//
//////////////////////////////////////////////////////////////////////

class GCDownSkillOK : public Packet {

public :
	
	// constructor
	GCDownSkillOK();
	
	// destructor
	~GCDownSkillOK();

	
public :
	
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read(SocketInputStream & iStream);
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write(SocketOutputStream & oStream) const;


	// get packet id
	PacketID_t getPacketID() const noexcept { return PACKET_GC_DOWN_SKILL_OK; }
	
	// get packet size
	PacketSize_t getPacketSize() const noexcept { return szSkillType; }

#ifdef __DEBUG_OUTPUT__
	// get packet's name
	std::string getPacketName() const { return "GCDownSkillOK"; }
	
	// get packet's debug string
	std::string toString() const;
#endif
	
	// get/set m_SkillType
	SkillType_t getSkillType() const noexcept { return m_SkillType; }
	void setSkillType(SkillType_t SkillType) noexcept { m_SkillType = SkillType; }

private : 

	// SkillType
	SkillType_t m_SkillType; 

};


//////////////////////////////////////////////////////////////////////
//
// class  GCDownSkillOKFactory;
//
// Factory for  GCDownSkillOK
//
//////////////////////////////////////////////////////////////////////

class  GCDownSkillOKFactory : public PacketFactory {

public :
	
	// constructor
	 GCDownSkillOKFactory() {}
	
	// destructor
	virtual ~GCDownSkillOKFactory() {}

	
public :
	
	// create packet
	Packet* createPacket() { return new GCDownSkillOK(); }

#ifdef __DEBUG_OUTPUT__
	// get packet name
	std::string getPacketName() const { return "GCDownSkillOK"; }
#endif
	
	// get packet id
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_GC_DOWN_SKILL_OK; }

	// get Packet Max Size
	PacketSize_t getPacketMaxSize() const noexcept { return szSkillType; }

};


//////////////////////////////////////////////////////////////////////
//
// class  GCDownSkillOKHandler;
//
//////////////////////////////////////////////////////////////////////

class  GCDownSkillOKHandler {

public :

	// execute packet's handler
	static void execute( GCDownSkillOK* pGCDownSkillOK, Player* pPlayer);

};

#endif	// __GC_DOWN_SKILL_OK_H__
