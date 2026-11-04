//----------------------------------------------------------------------
// test_packet_budgets.cpp
//----------------------------------------------------------------------
//
// A packet written at the most the server emits fits its factory's
// budget exactly (docs/protocol/wire-layout.md). The budget is the
// receive gate (ClientPlayer::processCommand, "too large packet SIZE"):
// one below the server's maximum refuses a legal packet, one above it is
// only a looser gate. Each case builds the packet at the server's caps
// (its kMaxCount, its name lengths, MAX_ITEM_OPTION_NUM) through this
// client's own write(), which takes the server's fields in the server's
// order, and checks the bytes and getPacketSize() against
// getPacketMaxSize().
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"
#include "Exception.h"
#include "Packet.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketEncryptOutputStream.h"
#include "EffectInfo.h"
#include "GuildMemberInfo.h"
#include "Gpackets/GCAddMonster.h"
#include "Gpackets/GCCreateItem.h"
#include "Gpackets/GCGuildMemberList.h"
#include "Gpackets/GCNPCAskDynamic.h"
#include "Gpackets/GCReconnect.h"
#include "Gpackets/GCRemoveEffect.h"
#include "Gpackets/GCShopBuyFail.h"
#include "Gpackets/GCUnionOfferList.h"
#include "Gpackets/GCWarScheduleList.h"
#include <string>

namespace {

// A stream over a never-used socket at encryption code 0, so its length
// is the body's (tests/support/packet_stream_access.h).
struct BodyOut
{
	Socket				m_Socket;
	SocketEncryptOutputStream	m_Stream;

	BodyOut()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket)
	{
		m_Stream.setEncryptCode(0);
	}
};

// The body `packet` writes, in bytes, and its own size, against the budget.
template <class FactoryT, class PacketT>
void	CheckWritesExactlyTheBudget(PacketT& packet)
{
	BodyOut out;
	packet.write(out.m_Stream);
	FactoryT factory;
	CHECK_EQ((long long)out.m_Stream.length(), (long long)factory.getPacketMaxSize());
	CHECK_EQ((long long)packet.getPacketSize(), (long long)factory.getPacketMaxSize());
}

} // namespace

TEST(PacketBudgets, AShopBuyFailIsItsNineBytes)
{
	GCShopBuyFail packet;
	packet.setObjectID(7);
	packet.setCode(1);
	packet.setAmount(3);
	CheckWritesExactlyTheBudget<GCShopBuyFailFactory>(packet);
}

TEST(PacketBudgets, AReconnectWithTheLongestNameAndAddressFits)
{
	GCReconnect packet;
	packet.setName(std::string(20, 'n'));
	packet.setServerIP(std::string(15, '1'));
	packet.setKey(9);
	CheckWritesExactlyTheBudget<GCReconnectFactory>(packet);
}

TEST(PacketBudgets, ACreatedItemWithTheMostOptionsFits)
{
	GCCreateItem packet;
	for (uint i = 0; i < MAX_ITEM_OPTION_NUM; i++)
		packet.addOptionType((OptionType_t)(i + 1));
	CheckWritesExactlyTheBudget<GCCreateItemFactory>(packet);
}

TEST(PacketBudgets, ARemoveEffectWithAFullListFits)
{
	GCRemoveEffect packet;
	packet.setObjectID(7);
	for (int i = 0; i < 255; i++)
		packet.addEffectList((EffectID_t)i);
	CheckWritesExactlyTheBudget<GCRemoveEffectFactory>(packet);
}

TEST(PacketBudgets, AMonsterWithTheLongestNameAndAFullEffectListFits)
{
	GCAddMonster packet;
	packet.setMonsterName(std::string(32, 'm'));
	EffectInfo* pEffects = new EffectInfo;
	for (int i = 0; i < 255; i++)
		pEffects->addListElement((EffectID_t)i, (WORD)i);
	packet.setEffectInfo(pEffects);		// the packet owns it
	CheckWritesExactlyTheBudget<GCAddMonsterFactory>(packet);
}

TEST(PacketBudgets, AnNPCQuestionWithTheLongestSubjectAndFifteenAnswersFits)
{
	GCNPCAskDynamic packet;
	packet.setSubject(std::string(1024, 's'));
	for (int i = 0; i < 15; i++)
		packet.addContent(std::string(1024, 'c'));
	CheckWritesExactlyTheBudget<GCNPCAskDynamicFactory>(packet);
}

TEST(PacketBudgets, AGuildMemberListOfTheMostMembersFits)
{
	GCGuildMemberList packet;
	packet.setType(0);
	for (int i = 0; i < 220; i++)
	{
		GuildMemberInfo* pMember = new GuildMemberInfo;
		pMember->setName(std::string(20, 'g'));
		pMember->setRank(1);
		pMember->setLogOn(true);
		pMember->setServerID(2);
		packet.addGuildMemberInfo(pMember);		// the packet owns it
	}
	CheckWritesExactlyTheBudget<GCGuildMemberListFactory>(packet);
}

TEST(PacketBudgets, AUnionOfferListOfTheMostOffersFits)
{
	GCUnionOfferList packet;
	for (int i = 0; i < 20; i++)
	{
		SingleGuildUnionOffer* pOffer = new SingleGuildUnionOffer;
		pOffer->setGuildID(1);
		pOffer->setGuildType(0);
		pOffer->setGuildName(std::string(30, 'u'));
		pOffer->setGuildMaster(std::string(20, 'm'));
		pOffer->setDate(5);
		packet.addUnionOfferList(pOffer);		// the packet owns it
	}
	CheckWritesExactlyTheBudget<GCUnionOfferListFactory>(packet);
}

TEST(PacketBudgets, AWarScheduleOfTheMostGuildWarsFits)
{
	GCWarScheduleList packet;
	for (int i = 0; i < 20; i++)
	{
		WarScheduleInfo* pWar = new WarScheduleInfo;
		pWar->warType = 0;		// a guild war carries the six guilds
		pWar->year = 2026;
		pWar->month = 11;
		pWar->day = 4;
		pWar->hour = 18;
		for (int g = 0; g < 5; g++)
		{
			pWar->challengerGuildID[g] = (GuildID_t)(g + 1);
			pWar->challengerGuildName[g] = std::string(16, 'c');
		}
		pWar->reinforceGuildID = 9;
		pWar->reinforceGuildName = std::string(16, 'r');
		packet.addWarScheduleInfo(pWar);		// the packet owns it
	}
	CheckWritesExactlyTheBudget<GCWarScheduleListFactory>(packet);
}
