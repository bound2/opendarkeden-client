//////////////////////////////////////////////////////////////////////
// 
// Filename    :  GCLearnSkillFailed.h 
// Written By  :  elca@ewestsoft.com
// Description :  Å
//                
// 
//////////////////////////////////////////////////////////////////////

#ifndef __GC_LEARN_SKILL_FAILED_H__
#define __GC_LEARN_SKILL_FAILED_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class  GCLearnSkillFailed;
//
//////////////////////////////////////////////////////////////////////

class GCLearnSkillFailed : public Packet 
{

public: 

	GCLearnSkillFailed();
	virtual ~GCLearnSkillFailed();

	
public:
	
	// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
	void read ( SocketInputStream & iStream );
			
	// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
	void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_GC_LEARN_SKILL_FAILED; }
	
	// get packet size
	PacketSize_t getPacketSize () const noexcept { return szSkillType+szBYTE; }
	
	#ifdef __DEBUG_OUTPUT__
		// get packet's name
		std::string getPacketName () const { return "GCLearnSkillFailed"; }
		
		// get packet's debug std::string
		std::string toString () const;
	#endif
	
	// get/set skill type
	SkillType_t getSkillType(void) const noexcept { return m_SkillType; }
	void setSkillType( SkillType_t SkillType ) noexcept { m_SkillType = SkillType; }

	// get/set description
	BYTE getDesc(void) const noexcept { return m_Desc;}
	void setDesc(BYTE desc) noexcept { m_Desc = desc;}

private:

	SkillType_t m_SkillType;
	BYTE        m_Desc;

};


//////////////////////////////////////////////////////////////////////
//
// class  GCLearnSkillFailedFactory;
//
// Factory for  GCLearnSkillFailed
//
//////////////////////////////////////////////////////////////////////

class  GCLearnSkillFailedFactory : public PacketFactory {

public :
	
	// constructor
	 GCLearnSkillFailedFactory () {}
	
	// destructor
	virtual ~GCLearnSkillFailedFactory () {}

	
public :
	
	// create packet
	Packet * createPacket () { return new GCLearnSkillFailed(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "GCLearnSkillFailed"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_GC_LEARN_SKILL_FAILED; }

	// get Packet Max Size
	PacketSize_t getPacketMaxSize() const noexcept { return szSkillType+szBYTE; }

};


//////////////////////////////////////////////////////////////////////
//
// class  GCLearnSkillFailedHandler;
//
//////////////////////////////////////////////////////////////////////

class  GCLearnSkillFailedHandler {

public :

	// execute packet's handler
	static void execute (  GCLearnSkillFailed * pGCLearnSkillFailed , Player * pPlayer );

};

#endif	// __GC_LEARN_SKILL_FAILED_H__
