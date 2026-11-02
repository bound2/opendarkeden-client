//----------------------------------------------------------------------
// test_game_server_link.cpp
//----------------------------------------------------------------------
//
// The game connection admits only what a game server sends
// (docs/protocol/server-link.md): PacketValidator::init folds
// GameServerLink::SentByAGameServer over the factory table, so in
// CPS_NORMAL, the status of a session in the game, the validator admits
// every registered GC packet and refuses every other registered id (the
// CG packets the client only writes, the login server's LC replies, a
// peer's CR and RC packets, the updater's UC and CU) and every id with
// no factory, which the PIST_ANY set this replaced admitted as far as
// the size gate (getPacketMaxSize throws for it). The server's
// GameClientLink.h and game_client_link_tests are the mirror for what
// a client sends.
//
// The goldens and the handler fuzz seeds are checked against the same
// set, so nothing the game targets replay is refused at the gate, and
// the production loop is driven once with a client-to-server packet to
// show the refusal comes before the body is read.
//
// The names the fold reads are checked too: every registered factory
// carries a name with a link in every build, and a validator
// initialised before the table is an error.
//
// Compiled with WIRE_GOLDEN_DIR and FUZZ_SEED_DIR (tests/CMakeLists.txt).
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"		// EnsureSocketsInitialised, Preload

#include "ClientPlayer.h"
#include "Exception.h"
#include "GameServerLink.h"
#include "Packet.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "PlayerStatus.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

namespace {

// The two tables GameInit.cpp builds, installed as the globals the
// validator and the receive loop read, and put back after.
struct Tables
{
	Tables()
	: previousFactories(g_pPacketFactoryManager), previousValidator(g_pPacketValidator)
	{
		factories.init();
		g_pPacketFactoryManager = &factories;
		validator.init(factories);
		g_pPacketValidator = &validator;
	}

	~Tables()
	{
		g_pPacketFactoryManager = previousFactories;
		g_pPacketValidator = previousValidator;
	}

	PacketFactoryManager	factories;
	PacketValidator		validator;
	PacketFactoryManager*	previousFactories;
	PacketValidator*	previousValidator;
};

bool	AdmittedInGame(Tables& tables, PacketID_t id)
{
	return tables.validator.isValidPacketID(CPS_NORMAL, id);
}

// The registered factory named `name`, or PACKET_MAX when there is none.
PacketID_t	RegisteredIdOf(const PacketFactoryManager& factories, const std::string& name)
{
	for (PacketID_t id = 0; id < Packet::PACKET_MAX; id++)
		if (factories.hasFactory(id) && factories.getPacketName(id) == name)
			return id;
	return Packet::PACKET_MAX;
}

// A .hex file's bytes; anything but a hex digit is skipped.
std::vector<unsigned char>	BytesOfHexFile(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	const std::string hex((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	std::vector<unsigned char> bytes;
	unsigned int value = 0;
	int digits = 0;
	for (char c : hex)
	{
		int digit;
		if (c >= '0' && c <= '9')
			digit = c - '0';
		else if (c >= 'a' && c <= 'f')
			digit = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			digit = c - 'A' + 10;
		else
			continue;
		value = (value << 4) | (unsigned int)digit;
		if (++digits == 2)
		{
			bytes.push_back((unsigned char)value);
			value = 0;
			digits = 0;
		}
	}
	return bytes;
}

// A ClientPlayer over a socket that is never connected, whose ring the
// test fills itself, as test_player_base.cpp does for the base class.
class InGamePlayer : public ClientPlayer
{
public:
	InGamePlayer()
	: ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl())))
	{
	}

	~InGamePlayer() noexcept(false)
	{
		// The base destructor asserts that the session ended.
		setPlayerStatus(CPS_END_SESSION);
	}

	SocketInputStream&	inputStream()
	{
		return *m_pInputStream;
	}
};

void	AppendFrame(std::vector<unsigned char>& wire, PacketID_t id,
		    const std::vector<unsigned char>& body)
{
	const std::size_t at = wire.size();
	wire.resize(at + szPacketHeader);
	const PacketSize_t size = (PacketSize_t)body.size();
	const SequenceSize_t sequence = 0;
	std::memcpy(&wire[at], &id, szPacketID);
	std::memcpy(&wire[at + szPacketID], &size, szPacketSize);
	std::memcpy(&wire[at + szPacketID + szPacketSize], &sequence, szSequenceSize);
	wire.insert(wire.end(), body.begin(), body.end());
}

} // namespace

//----------------------------------------------------------------------
// The rule, folded over the table
//----------------------------------------------------------------------

TEST(GameServerLink, TheLinkRuleIsThePacketNamesPrefix)
{
	CHECK(GameServerLink::SentByAGameServer("GCSay"));
	CHECK(GameServerLink::SentByAGameServer("GC"));
	CHECK(!GameServerLink::SentByAGameServer("CGSay"));
	CHECK(!GameServerLink::SentByAGameServer("LCLoginOK"));
	CHECK(!GameServerLink::SentByAGameServer("GGServerChat"));
	CHECK(!GameServerLink::SentByAGameServer("GLKickVerify"));
	CHECK(!GameServerLink::SentByAGameServer("gcSay"));
	CHECK(!GameServerLink::SentByAGameServer("G"));
	CHECK(!GameServerLink::SentByAGameServer(""));
}

TEST(GameServerLink, InGameTheValidatorAdmitsEveryRegisteredGCPacketAndNothingElse)
{
	Tables tables;

	int admitted = 0;
	int refusedRegistered = 0;
	int mismatches = 0;
	for (PacketID_t id = 0; id < Packet::PACKET_MAX; id++)
	{
		const bool registered = tables.factories.hasFactory(id);
		const bool expected = registered
			&& GameServerLink::SentByAGameServer(tables.factories.getPacketName(id));
		const bool actual = AdmittedInGame(tables, id);
		if (actual != expected)
		{
			std::printf("  id %u (%s): admitted %d, expected %d\n", (unsigned)id,
				registered ? tables.factories.getPacketName(id).c_str() : "no factory",
				(int)actual, (int)expected);
			mismatches++;
		}
		if (actual)
			admitted++;
		else if (registered)
			refusedRegistered++;
	}
	CHECK_EQ(0, mismatches);

	// 246 GC factories and 41 others when this was written; the bounds let a
	// registration or two come or go without pinning the table.
	CHECK(admitted >= 200);
	CHECK(refusedRegistered >= 20);
}

TEST(GameServerLink, TheLinksAClientWritesOrReadsElsewhereAreRefusedInGame)
{
	Tables tables;

	// What a game server sends.
	CHECK(AdmittedInGame(tables, Packet::PACKET_GC_SAY));
	CHECK(AdmittedInGame(tables, Packet::PACKET_GC_UPDATE_INFO));
	CHECK(AdmittedInGame(tables, Packet::PACKET_GC_SET_POSITION));
	CHECK(AdmittedInGame(tables, Packet::PACKET_GC_MY_STORE_INFO));
	// Bound to a handler that does nothing on purpose
	// (PacketHandlerRegistry.cpp): admitted here and swallowed there, as
	// before.
	CHECK(AdmittedInGame(tables, Packet::PACKET_GC_EXCHANGE_LIST));

	// What this client only writes, the login server's replies, a peer's
	// packets and the updater's.
	CHECK(!AdmittedInGame(tables, Packet::PACKET_CG_SMS_SEND));
	CHECK(!AdmittedInGame(tables, Packet::PACKET_CG_EXCHANGE_LIST));
	CHECK(!AdmittedInGame(tables, Packet::PACKET_LC_LOGIN_OK));
	CHECK(!AdmittedInGame(tables, Packet::PACKET_CR_REQUEST));
	CHECK(!AdmittedInGame(tables, Packet::PACKET_RC_SAY));
	CHECK(!AdmittedInGame(tables, Packet::PACKET_UC_REQUEST_LOGIN_MODE));
	CHECK(!AdmittedInGame(tables, Packet::PACKET_CU_REQUEST_LOGIN_MODE));

	// An id with no factory, which PIST_ANY admitted as far as createPacket.
	PacketID_t unregistered = Packet::PACKET_MAX;
	for (PacketID_t id = 0; id < Packet::PACKET_MAX; id++)
	{
		if (!tables.factories.hasFactory(id))
		{
			unregistered = id;
			break;
		}
	}
	CHECK(unregistered != Packet::PACKET_MAX);
	CHECK(!AdmittedInGame(tables, unregistered));
}

//----------------------------------------------------------------------
// The names the fold reads
//----------------------------------------------------------------------

TEST(GameServerLink, EveryRegisteredFactoryCarriesItsNameInEveryBuild)
{
	Tables tables;

	// The links PacketFactoryManager::init registers.
	static const char* const links[] = { "GC", "LC", "CR", "RC", "CG", "UC", "CU" };

	std::set<std::string> names;
	int registered = 0;
	int unlinked = 0;
	for (PacketID_t id = 0; id < Packet::PACKET_MAX; id++)
	{
		if (!tables.factories.hasFactory(id))
			continue;
		registered++;
		const std::string name = tables.factories.getPacketName(id);
		bool linked = false;
		for (const char* link : links)
			if (name.size() > 2 && name.compare(0, 2, link) == 0)
				linked = true;
		if (!linked)
		{
			std::printf("  id %u is registered as \"%s\"\n", (unsigned)id, name.c_str());
			unlinked++;
		}
		// No two ids are registered under one name.
		CHECK(names.insert(name).second);
	}
	CHECK(registered >= 200);
	CHECK_EQ(0, unlinked);
}

TEST(GameServerLink, InitBeforeTheFactoriesIsAnError)
{
	// GameInit.cpp builds the factory table first; a validator folded
	// from no table would admit nothing in game.
	PacketFactoryManager* const previous = g_pPacketFactoryManager;
	g_pPacketFactoryManager = NULL;

	PacketValidator validator;
	bool refused = false;
	try {
		validator.init();
	} catch (Error&) {
		refused = true;
	}

	g_pPacketFactoryManager = previous;
	CHECK(refused);
}

//----------------------------------------------------------------------
// What the game targets replay stays admitted
//----------------------------------------------------------------------

TEST(GameServerLink, EveryGoldenOfARegisteredGameServerPacketIsAdmittedInGame)
{
	Tables tables;

	int goldens = 0;
	int unregistered = 0;
	for (const auto& entry : std::filesystem::directory_iterator(WIRE_GOLDEN_DIR))
	{
		const std::string file = entry.path().filename().string();
		if (entry.path().extension() != ".hex")
			continue;
		const std::string name = file.substr(0, file.find('.'));
		if (!GameServerLink::SentByAGameServer(name))
			continue;
		const PacketID_t id = RegisteredIdOf(tables.factories, name);
		if (id == Packet::PACKET_MAX)
		{
			// A GC packet class the client never registers: its golden
			// pins the layout, and no connection reads it.
			unregistered++;
			continue;
		}
		if (!AdmittedInGame(tables, id))
			std::printf("  golden %s is refused in game\n", file.c_str());
		CHECK(AdmittedInGame(tables, id));
		goldens++;
	}
	CHECK(goldens > 0);
	std::printf("  %d goldens of registered GC packets admitted, %d name no registered factory\n",
		goldens, unregistered);
}

TEST(GameServerLink, EveryFrameOfAHandlerSeedIsAdmittedInGame)
{
	Tables tables;

	// tests/unit/test_fuzz_seeds.cpp writes each seed as the encrypt code
	// byte and whole frames; the headers are never encrypted.
	const std::filesystem::path dir =
		std::filesystem::path(FUZZ_SEED_DIR) / "client_stream_handlers";
	int seeds = 0;
	int frames = 0;
	for (const auto& entry : std::filesystem::directory_iterator(dir))
	{
		if (entry.path().extension() != ".hex")
			continue;
		const std::vector<unsigned char> bytes = BytesOfHexFile(entry.path());
		CHECK(bytes.size() >= 1 + szPacketHeader);

		std::size_t pos = 1;
		while (pos + szPacketHeader <= bytes.size())
		{
			PacketID_t id = 0;
			PacketSize_t size = 0;
			std::memcpy(&id, &bytes[pos], szPacketID);
			std::memcpy(&size, &bytes[pos + szPacketID], szPacketSize);
			if (!AdmittedInGame(tables, id))
				std::printf("  seed %s: the frame at %u (id %u) is refused in game\n",
					entry.path().filename().string().c_str(), (unsigned)pos, (unsigned)id);
			CHECK(AdmittedInGame(tables, id));
			pos += szPacketHeader + size;
			frames++;
		}
		// The frames end where the seed ends.
		CHECK_EQ(bytes.size(), pos);
		seeds++;
	}
	CHECK(seeds > 0);
	CHECK(frames > 0);
}

//----------------------------------------------------------------------
// The production loop
//----------------------------------------------------------------------

TEST(ClientPlayerReceiveLoop, AClientToServerPacketInGameIsRefusedBeforeItsBodyIsRead)
{
	Tables tables;
	InGamePlayer player;
	player.setPlayerStatus(CPS_NORMAL);

	// CGSMSSend with no receiver, no caller and no message: a body its
	// reader would take, which must never get the chance.
	std::vector<unsigned char> wire;
	AppendFrame(wire, Packet::PACKET_CG_SMS_SEND, std::vector<unsigned char>{ 0, 0, 0 });
	CHECK(SocketInputStreamTestAccess::Preload(player.inputStream(), &wire[0],
		(unsigned int)wire.size()));

	bool refused = false;
	try {
		player.processCommand();
	} catch (InvalidProtocolException&) {
		refused = true;
	}
	CHECK(refused);

	// The frame is still in the ring: the loop peeked its header and read
	// nothing of it.
	CHECK_EQ((unsigned int)wire.size(), player.inputStream().length());
}
