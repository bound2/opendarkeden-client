#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

namespace NetworkTransport {

// Return owned configuration text, rather than mutable CRT environment storage.
std::optional<std::string> ReadEnvironment(const char* name);

// DARKEDEN_WEBSOCKET_URL, read once per process: every socket and every UDP
// path gated on UsesWebSocket() must agree on one transport. Empty for native
// TCP. Browsers always use a gateway. The advertised game endpoints remain
// unchanged, including login/world/relogin handoff packets.
const std::string& WebSocketGateway();
bool UsesWebSocket();
std::string WebSocketURL(const std::string& gateway, const std::string& host, unsigned int port);

class WebSocketTransport {
public:
	static constexpr std::size_t MaxBufferedBytes = 2 * 1024 * 1024;
	explicit WebSocketTransport(const std::string& url);
	~WebSocketTransport();
	WebSocketTransport(const WebSocketTransport&) = delete;
	WebSocketTransport& operator=(const WebSocketTransport&) = delete;

	unsigned int send(const void* bytes, unsigned int length);
	unsigned int receive(void* bytes, unsigned int length);
	unsigned int available() const;

private:
	struct State;
	std::unique_ptr<State> m_State;
};

}
