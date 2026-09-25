#include "test_framework.h"
#include "WebSocketTransport.h"
#include "Exception.h"
#include <cstdlib>

TEST(WebSocket, EnvironmentValuesRemainOwnedAcrossConfigurationChanges)
{
	const char* name = "DARKEDEN_TRANSPORT_ENV_TEST";
	const auto previous = NetworkTransport::ReadEnvironment(name);
	auto set = [name](const char* value) {
#ifdef _WIN32
		CHECK_EQ(0, _putenv_s(name, value ? value : ""));
#else
		CHECK_EQ(0, value ? setenv(name, value, 1) : unsetenv(name));
#endif
	};
	set("first");
	const auto first = NetworkTransport::ReadEnvironment(name);
	set("second");
	CHECK(first && *first == "first");
	CHECK(NetworkTransport::ReadEnvironment(name).value_or("") == "second");
	set(nullptr);
	CHECK(!NetworkTransport::ReadEnvironment(name));
	set(previous ? previous->c_str() : nullptr);
}

// The transport is chosen once per process. Every socket, the login/world
// handoffs and the UDP paths gated on UsesWebSocket() must agree, and the
// party position update asks on every move, so the environment is not
// re-read (each _dupenv_s call allocates).
TEST(WebSocket, TransportChoiceIsFixedForTheProcess)
{
	const char* name = "DARKEDEN_WEBSOCKET_URL";
	const auto previous = NetworkTransport::ReadEnvironment(name);
	auto set = [name](const char* value) {
#ifdef _WIN32
		CHECK_EQ(0, _putenv_s(name, value ? value : ""));
#else
		CHECK_EQ(0, value ? setenv(name, value, 1) : unsetenv(name));
#endif
	};
	const bool first = NetworkTransport::UsesWebSocket();
	const std::string gateway = NetworkTransport::WebSocketGateway();
	set(first ? nullptr : "ws://127.0.0.1:1/game");
	CHECK_EQ(first, NetworkTransport::UsesWebSocket());
	CHECK(gateway == NetworkTransport::WebSocketGateway());
	set(previous ? previous->c_str() : nullptr);
}

TEST(WebSocket, MapsAdvertisedHandoffEndpointsWithoutChangingPacketData)
{
	CHECK(NetworkTransport::WebSocketURL("wss://play.example/game", "192.0.2.4", 9998) ==
		"wss://play.example/game?host=192.0.2.4&port=9998");
	CHECK(NetworkTransport::WebSocketURL("ws://127.0.0.1:8080/game", "login.example", 9999) ==
		"ws://127.0.0.1:8080/game?host=login.example&port=9999");
}

TEST(WebSocket, RejectsAmbiguousGatewayAndEndpointConfiguration)
{
	for (const auto* gateway : {"", "https://example/game", "ws:///game", "ws://:8080/game", "wss://user@example/game",
		"wss://example/game?port=22", "wss://example/#fragment", "wss://example/\r\nHost:"}) {
		bool rejected = false;
		try { NetworkTransport::WebSocketURL(gateway, "127.0.0.1", 9999); }
		catch (ConnectException&) { rejected = true; }
		CHECK(rejected);
	}
	for (const auto* host : {"", "example&port=22", "../private", "192.0.2.1\r\n", "a:b"}) {
		bool rejected = false;
		try { NetworkTransport::WebSocketURL("wss://example/game", host, 9999); }
		catch (ConnectException&) { rejected = true; }
		CHECK(rejected);
	}
	for (const auto port : {0u, 65536u, 0xffffffffu}) {
		bool rejected = false;
		try { NetworkTransport::WebSocketURL("wss://example/game", "127.0.0.1", port); }
		catch (ConnectException&) { rejected = true; }
		CHECK(rejected);
	}
}
