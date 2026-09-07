//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSkillToInventory.h 
// Written By  : elca@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////

#ifndef __CG_SKILL_TO_INVENTORY_H__
#define __CG_SKILL_TO_INVENTORY_H__

#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////////////
// class CGSkillToInventory;
//////////////////////////////////////////////////////////////////////////////

class CGSkillToInventory : public Packet 
{
public:

	CGSkillToInventory ();
	~CGSkillToInventory ();

    void read(SocketInputStream & iStream);
    void write(SocketOutputStream & oStream) const;
	PacketID_t getPacketID() const noexcept { return PACKET_CG_SKILL_TO_INVENTORY; }
	//modify by viva 
	//PacketSize_t getPacketSize() const throw() { return szSkillType + szObjectID + szObjectID  + szCoordInven*4; }
	PacketSize_t getPacketSize() const noexcept { return szSkillType + szObjectID  + szCoordInven*4; }
	//end

#ifdef __DEBUG_OUTPUT__
	std::string getPacketName() const { return "CGSkillToInventory"; }
	std::string toString() const;
#endif

public:
	SkillType_t getSkillType() const noexcept  { return m_SkillType; }
	void setSkillType(SkillType_t SkillType) noexcept { m_SkillType = SkillType; }

	ObjectID_t getObjectID() const noexcept { return m_ObjectID; }
	void setObjectID(ObjectID_t ObjectID) noexcept { m_ObjectID = ObjectID; }

	ObjectID_t getInventoryItemObjectID() noexcept { return m_InventoryItemObjectID; }
	void setInventoryItemObjectID(ObjectID_t InventoryItemObjectID) noexcept { m_InventoryItemObjectID = InventoryItemObjectID; }

	CoordInven_t getX() const noexcept { return m_X; }
	void setX(Coord_t X) noexcept { m_X = X; }

	CoordInven_t getY() const noexcept { return m_Y; }
	void setY(Coord_t Y) noexcept { m_Y = Y; }

	CoordInven_t getTargetX() const noexcept { return m_TargetX; }
	void setTargetX(Coord_t TargetX) noexcept { m_TargetX = TargetX; }

	CoordInven_t getTargetY() const noexcept { return m_TargetY; }
	void setTargetY(Coord_t TargetY) noexcept { m_TargetY = TargetY; }

private :
	SkillType_t  m_SkillType;	// SkillType
	ObjectID_t   m_ObjectID;	// ObjectID
	// 보조 인벤토리 아이템의 오브젝트 아이디. 0이면 메인 인벤토리에서 사용

	ObjectID_t	 m_InventoryItemObjectID;
	CoordInven_t m_X;			// Coord X
	CoordInven_t m_Y;			// Coord Y
	CoordInven_t m_TargetX;		// Target X
	CoordInven_t m_TargetY;		// Target Y
};

//////////////////////////////////////////////////////////////////////////////
// class CGSkillToInventoryFactory;
//////////////////////////////////////////////////////////////////////////////
class CGSkillToInventoryFactory : public PacketFactory 
{
public:
	Packet* createPacket() { return new CGSkillToInventory(); }

	#ifdef __DEBUG_OUTPUT__
		std::string getPacketName() const { return "CGSkillToInventory"; }
	#endif

	PacketID_t getPacketID() const noexcept { return Packet::PACKET_CG_SKILL_TO_INVENTORY; }
	//modify by viva
	//PacketSize_t getPacketMaxSize() const throw() { return szSkillType + szObjectID + szObjectID + szCoordInven*4; }
	PacketSize_t getPacketMaxSize() const noexcept { return szSkillType + szObjectID + szCoordInven*4; }
	//end
};

//////////////////////////////////////////////////////////////////////////////
// class CGSkillToInventoryHandler;
//////////////////////////////////////////////////////////////////////////////
#ifndef __GAME_CLIENT__
	class CGSkillToInventoryHandler 
	{
	public:
		static void execute(CGSkillToInventory* pCGSkillToInventory, Player* pPlayer);
	};
#endif

#endif
