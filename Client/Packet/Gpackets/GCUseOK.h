//////////////////////////////////////////////////////////////////////
// 
// Filename    : GCUseOK.h 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

#ifndef __GC_USE_OK_H__
#define __GC_USE_OK_H__

// include files
#include "Packet.h"
#include "ModifyInfo.h"
#include "PacketFactory.h"


//////////////////////////////////////////////////////////////////////
//
// class GCUseOK;
//
//////////////////////////////////////////////////////////////////////

class GCUseOK : public ModifyInfo {

public :

	// Constructor
	GCUseOK();

	// Desctructor
	~GCUseOK();
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_GC_USE_OK; }
	
	// get packet's body size
	// *OPTIMIZATION HINT*
	// const static GCUseOKPacketSize 를 정의해서 리턴하라.
	PacketSize_t getPacketSize () const { return ModifyInfo::getPacketSize(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "GCUseOK"; }
		
		// get packet's debug std::string
		std::string toString () const;
	#endif

};


//////////////////////////////////////////////////////////////////////
//
// class GCUseOKFactory;
//
// Factory for GCUseOK
//
//////////////////////////////////////////////////////////////////////

class GCUseOKFactory : public PacketFactory {

public :
	
	// create packet
	Packet * createPacket () { return new GCUseOK(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "GCUseOK"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_GC_USE_OK; }

	// get packet's max body size
	// *OPTIMIZATION HINT*
	// const static GCUseOKPacketSize 를 정의해서 리턴하라.
	// the body is one ModifyInfo; the old hardcoded 255 dropped any use
	// result with more than ~36 modify entries (server can send up to this)
	PacketSize_t getPacketMaxSize () const { return ModifyInfo::getPacketMaxSize(); }

};


//////////////////////////////////////////////////////////////////////
//
// class GCUseOKHandler;
//
//////////////////////////////////////////////////////////////////////

class GCUseOKHandler {
	
public :

	// execute packet's handler
	static void execute ( GCUseOK * pPacket , Player * player );
};

//-------------------------------------------GCUseSkillCardOK----------------

//////////////////////////////////////////////////////////////////////
//
// class GCUseSkillCardOK;
//
//////////////////////////////////////////////////////////////////////

class GCUseSkillCardOK : public Packet {

public :

	// Constructor
	GCUseSkillCardOK();

	// Desctructor
	~GCUseSkillCardOK();
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	void setCardType(BYTE CardType) noexcept {	m_CardType = CardType;  }

	BYTE getCardType() const noexcept	{	return m_CardType;	}

	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_GC_USE_SKILLCARD_OK; }
	
	// get packet's body size
	// *OPTIMIZATION HINT*
	// const static GCUseOKPacketSize 를 정의해서 리턴하라.
	PacketSize_t getPacketSize () const noexcept { return szBYTE; }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "GCUseSkillCardOK"; }
		
		// get packet's debug std::string
		std::string toString () const;
	#endif

public:
	BYTE m_CardType;

};


//////////////////////////////////////////////////////////////////////
//
// class GCUseSkillCardOKFactory;
//
// Factory for GCUseSkillCardOK
//
//////////////////////////////////////////////////////////////////////

class GCUseSkillCardOKFactory : public PacketFactory {

public :
	
	// create packet
	Packet * createPacket () { return new GCUseSkillCardOK(); }

	#ifdef __DEBUG_OUTPUT__
		// get packet name
		std::string getPacketName () const { return "GCUseSkillCardOK"; }
	#endif
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_GC_USE_SKILLCARD_OK; }

	// get packet's max body size
	// *OPTIMIZATION HINT*
	// const static GCUseOKPacketSize 를 정의해서 리턴하라.
	PacketSize_t getPacketMaxSize () const noexcept { return szBYTE; }

};


//////////////////////////////////////////////////////////////////////
//
// class GCUseSkillCardOKHandler;
//
//////////////////////////////////////////////////////////////////////

class GCUseSkillCardOKHandler {
	
public :

	// execute packet's handler
	static void execute ( GCUseSkillCardOK * pPacket , Player * player );
};


#endif
