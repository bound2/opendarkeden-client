//////////////////////////////////////////////////////////////////////
// 
// Filename    : GCSkillToObjectOK2.h 
// Written By  : elca@ewestsoft.com
// Description : 기술이 성공했을때 보내는 패킷을 위한 클래스 정의
// 				기술에 당한 분들이 받으실 패킷
// 
//////////////////////////////////////////////////////////////////////

#ifndef __GC_SKILL_TO_OBJECT_OK_2_H__
#define __GC_SKILL_TO_OBJECT_OK_2_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "ModifyInfo.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class GCSkillToObjectOK2;
//
// 게임서버에서 클라이언트로 자신의 기술이 성공을 알려주기 위한 클래스
//
//////////////////////////////////////////////////////////////////////

class GCSkillToObjectOK2 : public ModifyInfo {

public :
	
	// constructor
	GCSkillToObjectOK2();
	
	// destructor
	~GCSkillToObjectOK2();

	
public :
	
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read(SocketInputStream & iStream);
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write(SocketOutputStream & oStream) const;


	// get packet id
	PacketID_t getPacketID() const noexcept { return PACKET_GC_SKILL_TO_OBJECT_OK_2; }
	
	// get packet's body size
	// 최적화시, 미리 계산된 정수를 사용한다.
	PacketSize_t getPacketSize() const { return szObjectID+ szSkillType + szDuration + szBYTE + ModifyInfo::getPacketSize(); }
#ifdef __DEBUG_OUTPUT__
	// get packet's name
	std::string getPacketName() const { return "GCSkillToObjectOK2"; }
	
	// get packet's debug string
	std::string toString() const;
#endif

	// get / set ObjectID
	ObjectID_t getObjectID() const noexcept { return m_ObjectID; }
	void setObjectID(ObjectID_t ObjectID) noexcept { m_ObjectID = ObjectID; }

	// get / set SkillType

	SkillType_t getSkillType() const noexcept { return m_SkillType; }
	void setSkillType(SkillType_t SkillType) noexcept { m_SkillType = SkillType; }

	// get / set Duration
	Duration_t getDuration() const noexcept { return m_Duration; }
	void setDuration(Duration_t Duration) noexcept { m_Duration = Duration; }
	
	// get / set CEffectID 
//	CEffectID_t getCEffectID() const throw() { return m_CEffectID; }
//	void setCEffectID(CEffectID_t e) throw() { m_CEffectID = e; }

	BYTE getGrade() const noexcept { return m_Grade; }
	void setGrade( BYTE grade ) noexcept { m_Grade = grade; }

private :
	
	// ObjectID
	ObjectID_t m_ObjectID;

	// SkillType
	SkillType_t m_SkillType;

	// Duration
	Duration_t m_Duration;

	// CEffectID
//	CEffectID_t m_CEffectID;

	// grade
	BYTE m_Grade;
};


//////////////////////////////////////////////////////////////////////
//
// class GCSkillToObjectOK2Factory;
//
// Factory for GCSkillToObjectOK2
//
//////////////////////////////////////////////////////////////////////

class GCSkillToObjectOK2Factory : public PacketFactory {

public :
	
	// constructor
	GCSkillToObjectOK2Factory() {}
	
	// destructor
	virtual ~GCSkillToObjectOK2Factory() {}

	
public :
	
	// create packet
	Packet* createPacket() { return new GCSkillToObjectOK2(); }

#ifdef __DEBUG_OUTPUT__
	// get packet name
	std::string getPacketName() const { return "GCSkillToObjectOK2"; }
#endif
	
	// get packet id
	PacketID_t getPacketID() const noexcept { return Packet::PACKET_GC_SKILL_TO_OBJECT_OK_2; }

	// get Packet Max Size
	PacketSize_t getPacketMaxSize() const { return szObjectID+ szSkillType + szDuration + szBYTE + ModifyInfo::getPacketMaxSize(); }

};


//////////////////////////////////////////////////////////////////////
//
// class GCSkillToObjectOK2Handler;
//
//////////////////////////////////////////////////////////////////////

class GCSkillToObjectOK2Handler {

public :

	// execute packet's handler
	static void execute(GCSkillToObjectOK2* pGCSkillToObjectOK2, Player* pPlayer);

};

#endif
