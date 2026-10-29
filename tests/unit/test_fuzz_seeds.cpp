//----------------------------------------------------------------------
// test_fuzz_seeds.cpp
//----------------------------------------------------------------------
//
// The handler fuzz targets' own seeds (tests/fuzz/seeds/<target>/*.hex,
// docs/protocol/fuzzing.md): streams of frames a server could send that
// open a window and then free the object it draws, the routes the
// recording presentation hosts model (tests/fuzz/recording_hosts.h). The
// seed corpus made from tests/golden holds no packet that reaches one.
//
// Each seed is the encrypt code byte 0 and the frames, each body framed
// by its length (see Seed): one the packet class has no setters for is
// written from its fields and read into the packet, so a seed holds only
// what the reader accepts; one with setters is what the packet writes.
//
// Set UPDATE_FUZZ_SEEDS=1 to (re)write the seeds instead of comparing
// them; such a run fails, as a golden-recording run does. A seed is not
// a wire pin: one that no longer matches follows a change the goldens
// already pin, and is written again.
//
//----------------------------------------------------------------------
#include "test_framework.h"
#include "packet_stream_access.h"
#include "CrtCompat.h"
#include "ItemClassDef.h"
#include "MShop.h"
#include "PCSlayerInfo2.h"
#include "PCVampireInfo2.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "StoreInfo.h"
#include "Gpackets/GCAddNPC.h"
#include "Gpackets/GCDeleteObject.h"
#include "Gpackets/GCMorph1.h"
#include "Gpackets/GCMyStoreInfo.h"
#include "Gpackets/GCPetStashList.h"
#include "Gpackets/GCShopList.h"
#include "Gpackets/GCStashList.h"
#include "Gpackets/GCUpdateInfo.h"
#include <cstdio>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

// The world the handler targets build (tests/fuzz/handler_world.h): a
// zone 32 cells a side, the player in its middle.
const ObjectID_t kNPC = 5000;
const Coord_t kNear = 17;

// A body as a server writes its fields.
struct Body
{
	Socket socket{(EnsureSocketsInitialised(), new SocketImpl())};
	SocketOutputStream out{&socket};

	std::vector<unsigned char> Bytes() const { return SocketOutputStreamTestAccess::Bytes(out); }

	// Read into the packet, which must take every byte.
	template<class PacketType> void Into(PacketType& packet)
	{
		const std::vector<unsigned char> bytes = Bytes();
		Socket in((EnsureSocketsInitialised(), new SocketImpl()));
		SocketInputStream stream(&in, 8192);
		CHECK(SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size())));
		packet.read(stream);
		CHECK(stream.isEmpty());
	}
};

// The frames of one seed: the id, the body's size and the sequence, then
// the body. A body built from its fields is framed as written, since a
// packet the client only reads need not write what it reads (GCUpdateInfo
// does not); one built with setters, as the packet writes it. The size is
// the body's own length, not getPacketSize(), which GCMorph1's gets 1
// byte under what it writes, so SocketOutputStream::write(const Packet*)
// would frame it wrong.
struct Seed
{
	Socket socket{(EnsureSocketsInitialised(), new SocketImpl())};
	SocketOutputStream out{&socket};
	BYTE sequence = 0;

	Seed& Then(const Packet& packet)
	{
		Body body;
		packet.write(body.out);
		return Frame(packet.getPacketID(), body.Bytes());
	}
	Seed& Frame(PacketID_t id, const std::vector<unsigned char>& bytes)
	{
		out.write(id);
		out.write(static_cast<PacketSize_t>(bytes.size()));
		out.write(sequence++);
		out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<unsigned int>(bytes.size()));
		return *this;
	}
	std::vector<unsigned char> Bytes() const
	{
		std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(out);
		bytes.insert(bytes.begin(), 0);		// the encrypt code
		return bytes;
	}
};

void AddNPC(Seed& seed)
{
	GCAddNPC packet;
	packet.setObjectID(kNPC);
	packet.setNPCID(1);
	packet.setName("Seed");
	packet.setSpriteType(9);
	packet.setX(kNear);
	packet.setY(kNear);
	packet.setDir(2);
	seed.Then(packet);
}

// A normal rack of one sword.
void ShopList(Seed& seed)
{
	Body body;
	body.out.write(kNPC);
	body.out.write(static_cast<ShopVersion_t>(1));
	body.out.write(static_cast<ShopRackType_t>(SHOP_RACK_NORMAL));
	body.out.write(static_cast<BYTE>(1));
	body.out.write(static_cast<BYTE>(0));								// its slot
	body.out.write(static_cast<decltype(SHOPLISTITEM::objectID)>(6001));
	body.out.write(static_cast<decltype(SHOPLISTITEM::itemClass)>(ITEM_CLASS_SWORD));
	body.out.write(static_cast<decltype(SHOPLISTITEM::itemType)>(1));
	body.out.write(static_cast<BYTE>(0));								// no options
	body.out.write(static_cast<decltype(SHOPLISTITEM::durability)>(50));
	body.out.write(static_cast<decltype(SHOPLISTITEM::silver)>(0));
	body.out.write(static_cast<decltype(SHOPLISTITEM::grade)>(0));
	body.out.write(static_cast<decltype(SHOPLISTITEM::enchantLevel)>(0));
	body.out.write(static_cast<MarketCond_t>(100));
	body.out.write(static_cast<MarketCond_t>(100));
	body.out.write(static_cast<BYTE>(MShop::SHOP_NORMAL));
	GCShopList packet;
	body.Into(packet);
	seed.Frame(packet.getPacketID(), body.Bytes());
}

void DeleteNPC(Seed& seed)
{
	GCDeleteObject packet;
	packet.setObjectID(kNPC);
	seed.Then(packet);
}

// One rack, one sword, and the money.
void StashList(Seed& seed)
{
	Body body;
	body.out.write(static_cast<BYTE>(1));
	body.out.write(static_cast<BYTE>(1));
	body.out.write(static_cast<BYTE>(0));								// rack
	body.out.write(static_cast<BYTE>(0));								// slot
	body.out.write(static_cast<ObjectID_t>(6002));
	body.out.write(static_cast<BYTE>(ITEM_CLASS_SWORD));
	body.out.write(static_cast<ItemType_t>(1));
	body.out.write(static_cast<BYTE>(0));								// no options
	body.out.write(static_cast<Durability_t>(40));
	body.out.write(static_cast<ItemNum_t>(1));
	body.out.write(static_cast<Silver_t>(0));
	body.out.write(static_cast<Grade_t>(0));
	body.out.write(static_cast<EnchantLevel_t>(0));
	body.out.write(static_cast<BYTE>(0));								// no pockets
	body.out.write(static_cast<Gold_t>(100));
	GCStashList packet;
	body.Into(packet);
	seed.Frame(packet.getPacketID(), body.Bytes());
}

// Every slot empty.
void PetStashList(Seed& seed)
{
	Body body;
	for (int slot = 0; slot < MAX_PET_STASH; slot++)
		body.out.write(static_cast<BYTE>(0));
	GCPetStashList packet;
	body.Into(packet);
	seed.Frame(packet.getPacketID(), body.Bytes());
}

// The player's own shop of one sword; openUI asks for its window, which
// opens while the shop is shut.
void MyStoreInfo(Seed& seed, BYTE openUI, bool open)
{
	Body body;
	body.out.write(openUI);
	body.out.write(static_cast<BYTE>(open));
	const std::string sign = "Seed";
	body.out.write(static_cast<BYTE>(sign.size()));
	body.out.write(sign.data(), static_cast<unsigned int>(sign.size()));
	body.out.write(static_cast<BYTE>(1));
	StoreItemInfo info;
	info.setItemExist(1);
	info.setObjectID(6003);
	info.setItemClass(ITEM_CLASS_SWORD);
	info.setItemType(1);
	info.setDurability(300);
	info.setItemNum(1);
	info.setPrice(900);
	info.write(body.out);
	GCMyStoreInfo packet;
	body.Into(packet);
	seed.Frame(packet.getPacketID(), body.Bytes());
}

// A slayer entering the zone the world is in, as the world's own player.
void UpdateInfo(Seed& seed)
{
	Body body;
	body.out.write('S');
	// Value-initialised, as GCUpdateInfo::read makes one: the fields the
	// constructor leaves alone are 0, so a seed is the same on every run.
	std::unique_ptr<PCSlayerInfo2> pc(new PCSlayerInfo2());
	pc->setObjectID(1000);
	pc->setName("Seed");
	pc->setSex(MALE);
	pc->setHairStyle(HAIR_STYLE1);
	pc->write(body.out);
	InventoryInfo().write(body.out);
	GearInfo().write(body.out);
	ExtraInfo().write(body.out);
	EffectInfo().write(body.out);
	body.out.write(false);												// no motorcycle
	body.out.write(static_cast<ZoneID_t>(61));
	body.out.write(static_cast<Coord_t>(16));
	body.out.write(static_cast<Coord_t>(16));
	GameTime time;
	time.setYear(2026);
	time.setMonth(10);
	time.setDay(29);
	time.setHour(12);
	time.setMinute(0);
	time.setSecond(0);
	time.write(body.out);
	body.out.write(static_cast<BYTE>(WEATHER_CLEAR));
	body.out.write(static_cast<WeatherLevel_t>(0));
	body.out.write(static_cast<DarkLevel_t>(0));
	body.out.write(static_cast<LightLevel_t>(7));
	body.out.write(static_cast<BYTE>(0));								// NPC types
	body.out.write(static_cast<BYTE>(0));								// monster types
	body.out.write(static_cast<BYTE>(0));								// NPCs
	body.out.write(static_cast<BYTE>(0));								// server status
	body.out.write(static_cast<BYTE>(0));
	body.out.write(static_cast<DWORD>(0));								// SMS charge
	NicknameInfo nick;
	nick.setNicknameType(NicknameInfo::NICK_NONE);
	nick.write(body.out);
	body.out.write(static_cast<BYTE>(1));								// non-PK
	body.out.write(static_cast<uint>(0));								// union
	body.out.write(static_cast<BYTE>(0));
	// No Blood Bible signs (BloodBibleSignInfo's write needs a list size
	// its constructor leaves unset).
	body.out.write(static_cast<uint>(0));
	body.out.write(static_cast<BYTE>(0));
	body.out.write(static_cast<uint>(0));								// power rank
	GCUpdateInfo packet;
	body.Into(packet);
	seed.Frame(packet.getPacketID(), body.Bytes());
}

// The player turned into a vampire (an ousters' or a vampire's run turns
// the other way or not at all).
void Morph(Seed& seed)
{
	PCVampireInfo2* pInfo = new PCVampireInfo2();		// value-initialised, as above
	pInfo->setObjectID(1000);
	pInfo->setName("Seed");
	pInfo->setSex(MALE);
	GCMorph1 packet;
	packet.setPCInfo2(pInfo);
	packet.setGearInfo(new GearInfo);
	packet.setInventoryInfo(new InventoryInfo);
	packet.setExtraInfo(new ExtraInfo);
	seed.Then(packet);
}

std::string ToHex(const std::vector<unsigned char>& bytes)
{
	static const char* digits = "0123456789abcdef";
	std::string hex;
	for (unsigned char byte : bytes)
	{
		hex.push_back(digits[byte >> 4]);
		hex.push_back(digits[byte & 0x0F]);
	}
	return hex;
}

// Compare with tests/fuzz/seeds/client_stream_handlers/<name>.hex, or
// write it under UPDATE_FUZZ_SEEDS=1.
void ExpectSeed(const std::string& name, const Seed& seed)
{
	const std::string path = std::string(FUZZ_SEED_DIR) + "/client_stream_handlers/" + name + ".hex";
	const std::string actual = ToHex(seed.Bytes());
	const std::optional<std::string> update = Basic::GetEnvironment("UPDATE_FUZZ_SEEDS");
	if (update.has_value() && *update == "1")
	{
		std::ofstream file(path.c_str(), std::ios::trunc | std::ios::binary);
		CHECK(file.good());
		file << actual << "\n";
		std::printf("  wrote seed %s\n", path.c_str());
		::testfw::RecordCheck();
		::testfw::RecordFailure(__FILE__, __LINE__, "seed written (UPDATE_FUZZ_SEEDS=1) - re-run without it");
		return;
	}
	std::ifstream file(path.c_str(), std::ios::binary);
	std::string expected;
	std::getline(file, expected);
	if (expected != actual)
		std::fprintf(stderr, "  seed %s differs from what the packet classes write:\n"
			"  re-write it with UPDATE_FUZZ_SEEDS=1\n", path.c_str());
	CHECK(expected == actual);
}

} // namespace

// The shop window up on an NPC's shop, then the NPC deleted with it.
TEST(FuzzSeeds, AnNPCsShopThenTheNPCDeleted)
{
	Seed seed;
	AddNPC(seed);
	ShopList(seed);
	DeleteNPC(seed);
	ExpectSeed("shop-then-npc-deleted", seed);
}

// The stash and the pet storage replace each other's g_pStorage.
TEST(FuzzSeeds, TheStashAndThePetStorageEitherWayRound)
{
	Seed stashFirst;
	StashList(stashFirst);
	PetStashList(stashFirst);
	ExpectSeed("stash-then-pet-stash", stashFirst);
	Seed petsFirst;
	PetStashList(petsFirst);
	StashList(petsFirst);
	ExpectSeed("pet-stash-then-stash", petsFirst);
}

// GCUpdateInfo drops g_pStorage, and a race change does too.
TEST(FuzzSeeds, TheStorageDroppedByAnUpdateOrARaceChange)
{
	Seed update;
	PetStashList(update);
	UpdateInfo(update);
	ExpectSeed("pet-stash-then-update-info", update);
	Seed morph;
	StashList(morph);
	Morph(morph);
	ExpectSeed("stash-then-morph", morph);
}

// The player's own shop sent twice, its window opened by the first.
TEST(FuzzSeeds, ThePlayersShopTwice)
{
	Seed seed;
	MyStoreInfo(seed, 1, false);
	MyStoreInfo(seed, 0, true);
	ExpectSeed("my-store-twice", seed);
}
