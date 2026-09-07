//////////////////////////////////////////////////////////////////////
// 
// Filename    :  GCLearnSkillOK.h 
// Written By  :  elca@ewestsoft.com
// Description :  Å

//                
// 
//////////////////////////////////////////////////////////////////////

#ifndef __GC_LEARN_SKILL_OK_H__
#define __GC_LEARN_SKILL_OK_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class  GCLearnSkillOK;
//
//////////////////////////////////////////////////////////////////////

class GCLearnSkillOK : public Packet {

public :
	
	// constructor
	GCLearnSkillOK ();
	
	// destructor
	~GCLearnSkillOK ();

	
public :
	
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_GC_LEARN_SKILL_OK; }
	
	// get packet size
	PacketSize_t getPacketSize () const noexcept { return szSkillType+szSkillDomainType; }
	
	#ifdef __DEBUG_OUTPUT__
		// get packet's name
		std::string getPacketName () const { return "GCLearnSkillOK"; }
		
		// get packet's debug std::string
		std::string toString () const;
	#endif
	
	// get/set m_SkillType
	SkillType_t getSkillType() const noexcept { return m_SkillType; }
	void setSkillType( SkillType_t SkillType ) noexcept { m_SkillType = SkillType; }

	// get/set m_SkillDomainType
	SkillDomainType_t getSkillDomainType() const noexcept { return m_DomainType;}
	void setSkillDomainType( SkillDomainType_t DomainType) noexcept { m_DomainType = DomainType;}

private : 

	// SkillType
	SkillType_t m_SkillType; 

	// DomainType
	SkillDomainType_t m_DomainType;
};


//////////////////////////////////////////////////////////////////////
//
// class  GCLearnSkillOKFactory;
//
// Factory for  GCLearnSkillOK
//
//////////////////////////////////////////////////////////////////////

class  GCLearnSkillOKFactory : public PacketFactory {

public :
	
	// constructor
	 GCLearnSkillOKFactory () {}
	
	// destructor
	virtual ~GCLearnSkillOKFactory () {}

	
public :
	
	// create packet
	Packet * createPacket () { return new GCLearnSkillOK(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "GCLearnSkillOK"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_GC_LEARN_SKILL_OK; }

	// get Packet Max Size
	PacketSize_t getPacketMaxSize() const noexcept { return szSkillType+szSkillDomainType; }

};


//////////////////////////////////////////////////////////////////////
//
// class  GCLearnSkillOKHandler;
//
//////////////////////////////////////////////////////////////////////

class  GCLearnSkillOKHandler {

public :

	// execute packet's handler
	static void execute (  GCLearnSkillOK * pGCLearnSkillOK , Player * pPlayer );

};

#endif	// __GC_LEARN_SKILL_OK_H__
