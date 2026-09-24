#include "test_framework.h"
#include "WebSocketTransport.h"
#include "Exception.h"

TEST(WebSocket, MapsAdvertisedHandoffEndpointsWithoutChangingPacketData)
{
	CHECK(NetworkTransport::WebSocketURL("wss://play.example/game", "192.0.2.4", 9998) ==
		"wss://play.example/game?host=192.0.2.4&port=9998");
	CHECK(NetworkTransport::WebSocketURL("ws://127.0.0.1:8080/game", "login.example", 9999) ==
		"ws://127.0.0.1:8080/game?host=login.example&port=9999");
}

TEST(WebSocket, RejectsAmbiguousGatewayAndEndpointConfiguration)
{
	for (const auto* gateway : {"", "https://example/game", "ws:///game", "wss://user@example/game",
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
