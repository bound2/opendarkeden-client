//////////////////////////////////////////////////////////////////////
// 
// Filename    :  GCSkillFailed2.h 
// Written By  :  elca@ewestsoft.com
// Description :  Å

//                
// 
//////////////////////////////////////////////////////////////////////

#ifndef __GC_SKILL_FAILED_2_H__
#define __GC_SKILL_FAILED_2_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class  GCSkillFailed2;
//
//////////////////////////////////////////////////////////////////////

class GCSkillFailed2 : public Packet {

public :
	
	// constructor
	GCSkillFailed2 ();
	
	// destructor
	~GCSkillFailed2 ();

	
public :
	
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_GC_SKILL_FAILED_2; }
	
	// get packet size
	PacketSize_t getPacketSize () const noexcept { return szObjectID + szObjectID + szSkillType + szBYTE; }
	
	#ifdef __DEBUG_OUTPUT__
		// get packet's name
		std::string getPacketName () const { return "GCSkillFailed2"; }
		
		// get packet's debug std::string
		std::string toString () const;
	#endif
	
	// get/set ObjectID
	ObjectID_t getObjectID() const noexcept { return m_ObjectID; }
	void setObjectID( ObjectID_t ObjectID ) noexcept { m_ObjectID = ObjectID; }

	// get/set TargetObjectID
	ObjectID_t getTargetObjectID() const noexcept { return m_TargetObjectID; }
	void setTargetObjectID( ObjectID_t TargetObjectID ) noexcept { m_TargetObjectID = TargetObjectID; }

	// get/set SkillType
	SkillType_t getSkillType() const noexcept { return m_SkillType; }
	void setSkillType( SkillType_t SkillType ) noexcept { m_SkillType = SkillType; }
	
	BYTE		getGrade()	const noexcept { return m_Grade; }
	void		setGrade(BYTE grade) noexcept { m_Grade = grade; }

private : 

	// ObjectID
	ObjectID_t m_ObjectID;

	// TaragetObjectID
	ObjectID_t m_TargetObjectID;

	// SkillType
	SkillType_t m_SkillType;

	BYTE		m_Grade;

};


//////////////////////////////////////////////////////////////////////
//
// class  GCSkillFailed2Factory;
//
// Factory for  GCSkillFailed2
//
//////////////////////////////////////////////////////////////////////

class  GCSkillFailed2Factory : public PacketFactory {

public :
	
	// constructor
	 GCSkillFailed2Factory () {}
	
	// destructor
	virtual ~GCSkillFailed2Factory () {}

	
public :
	
	// create packet
	Packet * createPacket () { return new GCSkillFailed2(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "GCSkillFailed2"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_GC_SKILL_FAILED_2; }

	// get Packet Max Size
	PacketSize_t getPacketMaxSize() const noexcept { return  szObjectID + szObjectID + szSkillType + szBYTE; }

};


//////////////////////////////////////////////////////////////////////
//
// class  GCSkillFailed2Handler;
//
//////////////////////////////////////////////////////////////////////

class  GCSkillFailed2Handler {

public :

	// execute packet's handler
	static void execute (  GCSkillFailed2 * pGCSkillFailed2 , Player * pPlayer );

};

#endif	// __GC_SKILL_FAILED_2_H__
