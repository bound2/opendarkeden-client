#include "WebSocketTransport.h"
#include "Exception.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten/websocket.h>
#else
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#endif

namespace NetworkTransport {

bool UsesWebSocket()
{
#ifdef __EMSCRIPTEN__
	return true;
#else
	const char* gateway = std::getenv("DARKEDEN_WEBSOCKET_URL");
	return gateway && *gateway;
#endif
}

std::string WebSocketURL(const std::string& gateway, const std::string& host, unsigned int port)
{
	if ((!gateway.starts_with("ws://") && !gateway.starts_with("wss://")) ||
		gateway.find_first_of("?#@\\\r\n\t ") != std::string::npos || gateway.size() > 2048 ||
		host.empty() || host.size() > 253 || port == 0 || port > 65535 ||
		host.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-") != std::string::npos)
		throw ConnectException("Invalid WebSocket gateway or game endpoint");
	const auto authorityStart = gateway.find("://") + 3;
	if (authorityStart == gateway.size() || gateway[authorityStart] == '/')
		throw ConnectException("WebSocket gateway has no host");
	return gateway + "?host=" + host + "&port=" + std::to_string(port);
}

struct WebSocketTransport::State {
	using Clock = std::chrono::steady_clock;
	mutable std::mutex mutex;
	std::vector<unsigned char> input;
	std::size_t head = 0;
	bool opened = false;
	bool failed = false;
	Clock::time_point deadline = Clock::now() + std::chrono::seconds(10);
#ifdef __EMSCRIPTEN__
	EMSCRIPTEN_WEBSOCKET_T socket = 0;
#else
	ix::WebSocket socket;
#endif

	void check() const
	{
		if (failed || (!opened && Clock::now() >= deadline))
			throw ConnectException("WebSocket connection closed or timed out");
	}

	bool append(const void* bytes, std::size_t length)
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (failed) return false;
		if (length > MaxBufferedBytes - (input.size() - head)) {
			failed = true;
			return false;
		}
		if (head) {
			input.erase(input.begin(), input.begin() + head);
			head = 0;
		}
		const auto* first = static_cast<const unsigned char*>(bytes);
		input.insert(input.end(), first, first + length);
		return true;
	}
};

WebSocketTransport::WebSocketTransport(const std::string& url) : m_State(std::make_unique<State>())
{
	auto& state = *m_State;
#ifdef __EMSCRIPTEN__
	if (!emscripten_websocket_is_supported())
		throw ConnectException("This browser does not support WebSockets");
	EmscriptenWebSocketCreateAttributes attributes{};
	attributes.url = url.c_str();
	attributes.protocols = "binary";
	attributes.createOnMainThread = true;
	state.socket = emscripten_websocket_new(&attributes);
	if (state.socket <= 0) throw ConnectException("Cannot create WebSocket");
	emscripten_websocket_set_onopen_callback(state.socket, &state,
		[](int, const EmscriptenWebSocketOpenEvent*, void* data) {
			static_cast<State*>(data)->opened = true;
			return true;
		});
	emscripten_websocket_set_onerror_callback(state.socket, &state,
		[](int, const EmscriptenWebSocketErrorEvent*, void* data) {
			static_cast<State*>(data)->failed = true;
			return true;
		});
	emscripten_websocket_set_onclose_callback(state.socket, &state,
		[](int, const EmscriptenWebSocketCloseEvent*, void* data) {
			static_cast<State*>(data)->failed = true;
			return true;
		});
	emscripten_websocket_set_onmessage_callback(state.socket, &state,
		[](int, const EmscriptenWebSocketMessageEvent* event, void* data) {
			auto& current = *static_cast<State*>(data);
			try {
				if (!event->isText && current.append(event->data, event->numBytes)) return true;
			} catch (...) { /* Exceptions must never unwind through browser callbacks. */ }
			current.failed = true;
			emscripten_websocket_close(current.socket, 1009, "Invalid game stream");
			return true;
		});
#else
	// Winsock is reference counted. Balance this independent use at process exit.
	struct Network {
		Network() { if (!ix::initNetSystem()) throw ConnectException("Cannot initialize networking"); }
		~Network() { ix::uninitNetSystem(); }
	};
	static Network network;
	state.socket.setUrl(url);
	// IX otherwise invents a ws:// Origin; this is a native client, whose
	// empty Origin is accepted only when the gateway enables allowNative.
	state.socket.setExtraHeaders({{"Origin", ""}});
	state.socket.addSubProtocol("binary");
	state.socket.disableAutomaticReconnection();
	state.socket.disablePerMessageDeflate();
	state.socket.setHandshakeTimeout(10);
	state.socket.setOnMessageCallback([current = &state](const ix::WebSocketMessagePtr& message) {
		if (message->type == ix::WebSocketMessageType::Message) {
			try {
				if (message->binary && current->append(message->str.data(), message->str.size())) return;
			} catch (...) { /* A failed allocation terminates this connection. */ }
			{
				std::lock_guard<std::mutex> lock(current->mutex);
				current->failed = true;
			}
			current->socket.close(1009, "Invalid game stream");
		} else {
			std::lock_guard<std::mutex> lock(current->mutex);
			if (message->type == ix::WebSocketMessageType::Open) current->opened = true;
			if (message->type == ix::WebSocketMessageType::Close || message->type == ix::WebSocketMessageType::Error)
				current->failed = true;
		}
	});
	state.socket.start();
#endif
}

WebSocketTransport::~WebSocketTransport()
{
	auto& state = *m_State;
#ifdef __EMSCRIPTEN__
	// Detach before destroying State, including an in-flight handshake.
	emscripten_websocket_set_onopen_callback(state.socket, nullptr, nullptr);
	emscripten_websocket_set_onmessage_callback(state.socket, nullptr, nullptr);
	emscripten_websocket_set_onerror_callback(state.socket, nullptr, nullptr);
	emscripten_websocket_set_onclose_callback(state.socket, nullptr, nullptr);
	emscripten_websocket_close(state.socket, 1000, "Client disconnected");
	emscripten_websocket_delete(state.socket);
#else
	state.socket.stop();
#endif
}

unsigned int WebSocketTransport::send(const void* bytes, unsigned int length)
{
	auto& state = *m_State;
	{
		std::lock_guard<std::mutex> lock(state.mutex);
		state.check();
		if (!state.opened) return 0;
	}
	// Keep messages small; packet boundaries belong to SocketOutputStream.
	length = (std::min)(length, 64u * 1024u);
	if (!length) return 0;
#ifdef __EMSCRIPTEN__
	std::size_t buffered = 0;
	if (emscripten_websocket_get_buffered_amount(state.socket, &buffered) != EMSCRIPTEN_RESULT_SUCCESS)
		throw ConnectException("Cannot query WebSocket output");
	if (buffered > MaxBufferedBytes - length) return 0;
	if (emscripten_websocket_send_binary(state.socket, const_cast<void*>(bytes), length) != EMSCRIPTEN_RESULT_SUCCESS)
		throw ConnectException("WebSocket send failed");
#else
	if (state.socket.bufferedAmount() > MaxBufferedBytes - length) return 0;
	if (!state.socket.sendBinary(ix::IXWebSocketSendData(static_cast<const char*>(bytes), length)).success)
		throw ConnectException("WebSocket send failed");
#endif
	return length;
}

unsigned int WebSocketTransport::receive(void* bytes, unsigned int length)
{
	auto& state = *m_State;
	std::lock_guard<std::mutex> lock(state.mutex);
	const auto count = (std::min)(static_cast<std::size_t>(length), state.input.size() - state.head);
	if (count) {
		std::memcpy(bytes, state.input.data() + state.head, count);
		state.head += count;
		if (state.head == state.input.size()) { state.input.clear(); state.head = 0; }
		return static_cast<unsigned int>(count);
	}
	state.check();
	throw NonBlockingIOException("WebSocket input is not ready");
}

unsigned int WebSocketTransport::available() const
{
	const auto& state = *m_State;
	std::lock_guard<std::mutex> lock(state.mutex);
	const auto count = state.input.size() - state.head;
	if (!count) state.check();
	return static_cast<unsigned int>(count);
}

}
