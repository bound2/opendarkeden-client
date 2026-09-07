//////////////////////////////////////////////////////////////////////
// 
// Filename    : GCKnocksTargetBackOK5.h 
// Written By  : elca@ewestsoft.com
// Description : 기술 사용자는 볼 수 있고, 피해자는 볼 수 없는 사람에게 보내는 패킷
// 
//////////////////////////////////////////////////////////////////////

#ifndef __GC_KNOCKS_TARGET_BACK_OK_5_H__
#define __GC_KNOCKS_TARGET_BACK_OK_5_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////
//
// class GCKnocksTargetBackOK5;
//
// 게임서버에서 클라이언트로 자신의 기술이 성공을 알려주기 위한 클래스
//
//////////////////////////////////////////////////////////////////////

class GCKnocksTargetBackOK5 : public Packet {

public :
	
	// constructor
	GCKnocksTargetBackOK5 ();
	
	// destructor
	~GCKnocksTargetBackOK5 ();

	
public :
	
	
    // 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
    void read ( SocketInputStream & iStream );
		    
    // 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
    void write ( SocketOutputStream & oStream ) const;


	// get packet id
	PacketID_t getPacketID () const noexcept { return PACKET_GC_KNOCKS_TARGET_BACK_OK_5; }
	
	// get packet's body size
	// 최적화시, 미리 계산된 정수를 사용한다.
	PacketSize_t getPacketSize () const noexcept { return szObjectID + szObjectID + szbool + szSkillType + szCoord*2 + szDir; }

	#ifdef __DEBUG_OUTPUT__
		// get packet's name
		std::string getPacketName () const { return "GCKnocksTargetBackOK5"; }
		
		// get packet's debug std::string
		std::string toString () const;
	#endif

	// get / set ObjectID
	ObjectID_t getObjectID() const noexcept { return m_ObjectID; }
	void setObjectID( ObjectID_t ObjectID ) noexcept { m_ObjectID = ObjectID; }

	// get / set ObjectID
	ObjectID_t getTargetObjectID() const noexcept { return m_TargetObjectID; }
	void setTargetObjectID( ObjectID_t TargetObjectID ) noexcept { m_TargetObjectID = TargetObjectID; }
//	Coord_t getTargetX() const throw() { return m_X;}
//	Coord_t getTargetY() const throw() { return m_Y;}
//	void setTargetXY( Coord_t X, Coord_t Y) throw() { m_X = X; m_Y = Y;}

	// get / set success
	bool getSkillSuccess() const noexcept { return m_bSuccess;}
	void setSkillSuccess( bool bSuccess) noexcept { m_bSuccess = bSuccess;}

    void setXYDir( Coord_t x, Coord_t y, Coord_t dir)
    {
        m_X = x; m_Y = y; m_dir = dir;
    }
    Coord_t getX() const noexcept { return m_X;}
    Coord_t getY() const noexcept { return m_Y;}
    Dir_t getDir() const noexcept { return m_dir;}

	SkillType_t getSkillType() const noexcept { return m_SkillType;}
	void setSkillType( SkillType_t s) noexcept { m_SkillType = s;}

private :
	
	// ObjectID
	ObjectID_t m_ObjectID;
//	Coord_t m_X, m_Y;

	// TargetObjectID
	ObjectID_t m_TargetObjectID;

	// bool
	bool m_bSuccess;

    Coord_t m_X, m_Y;   // 이동된 좌표.
    Dir_t m_dir;        // 밀려난 방향.
    SkillType_t m_SkillType;

};


//////////////////////////////////////////////////////////////////////
//
// class GCKnocksTargetBackOK5Factory;
//
// Factory for GCKnocksTargetBackOK5
//
//////////////////////////////////////////////////////////////////////

class GCKnocksTargetBackOK5Factory : public PacketFactory {

public :
	
	// constructor
	GCKnocksTargetBackOK5Factory () {}
	
	// destructor
	virtual ~GCKnocksTargetBackOK5Factory () {}

	
public :
	
	// create packet
	Packet * createPacket () { return new GCKnocksTargetBackOK5(); }

	// get packet name
	std::string getPacketName () const { return "GCKnocksTargetBackOK5"; }
	
	// get packet id
	PacketID_t getPacketID () const noexcept { return Packet::PACKET_GC_KNOCKS_TARGET_BACK_OK_5; }

	// get Packet Max Size
	PacketSize_t getPacketMaxSize () const noexcept { return szObjectID + szObjectID+ szbool + szSkillType + szCoord*2 + szDir; }

};


//////////////////////////////////////////////////////////////////////
//
// class GCKnocksTargetBackOK5Handler;
//
//////////////////////////////////////////////////////////////////////

class GCKnocksTargetBackOK5Handler {

public :

	// execute packet's handler
	static void execute ( GCKnocksTargetBackOK5 * pGCKnocksTargetBackOK5 , Player * pPlayer );

};

#endif
