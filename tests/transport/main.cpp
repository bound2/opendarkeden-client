// Exercise the production Socket adapter against the binary gateway fixture.
#define SDL_MAIN_HANDLED
#include "Socket.h"
#include "SocketOutputStream.h"
#include "WebSocketTransport.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <thread>
#endif

namespace {
struct Probe {
	std::unique_ptr<Socket> socket;
	std::unique_ptr<SocketOutputStream> output;
	std::vector<unsigned char> expected;
	std::vector<unsigned char> received;
	std::size_t sent = 0;
	int phase = 0;
	bool done = false;
	int result = 1;
	std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds(25);

	Probe() : socket(std::make_unique<Socket>("192.0.2.2", 9999)), expected(256 * 1024)
	{
		for (std::size_t i = 0; i < expected.size(); ++i) expected[i] = static_cast<unsigned char>(i * 31);
		socket->setNonBlocking(true);
		socket->connect();
		output = std::make_unique<SocketOutputStream>(socket.get(), 1024);
		output->write(reinterpret_cast<const char*>(expected.data()), static_cast<uint>(expected.size()));
		// The game queues packets immediately after connect. This must yield
		// while the asynchronous handshake is pending, retaining every byte.
		sent = output->flush();
	}

	void tick()
	{
		if (done) return;
		if (std::chrono::steady_clock::now() >= deadline) {
			std::fputs("TRANSPORT FAIL: timed out\n", stderr);
			done = true;
			return;
		}
		try {
			if (phase < 3 && sent < expected.size())
				sent += output->flush();
			unsigned char bytes[97];
			for (int i = 0; i < 512; ++i) {
				try {
					const auto count = socket->receive(bytes, sizeof(bytes));
					if (count == 0) throw ConnectException("Unexpected empty read");
					received.insert(received.end(), bytes, bytes + count);
				} catch (NonBlockingIOException&) { break; }
			}
			if (phase < 3 && received.size() >= expected.size()) {
				if (received != expected) throw ConnectException("Echo stream differs from sent bytes");
				++phase;
				sent = 0;
				received.clear();
				socket->reconnect(phase == 1 ? "192.0.2.3" : "192.0.2.2", phase == 1 ? 9998 : phase == 3 ? 9997 : 9999);
				if (phase < 3)
					output->write(reinterpret_cast<const char*>(expected.data()), static_cast<uint>(expected.size()));
			}
		} catch (ConnectException& error) {
			// The fixture either rejects the last route or sends a text frame.
			if (phase == 3 && received.empty()) {
				result = 0;
				std::puts("TRANSPORT PASS: binary stream, partial reads, login/world/relogin, rejected connection");
			} else {
				std::fprintf(stderr, "TRANSPORT FAIL: %s\n", error.toString().c_str());
			}
			done = true;
		} catch (Throwable& error) {
			std::fprintf(stderr, "TRANSPORT FAIL: %s\n", error.toString().c_str());
			done = true;
		}
	}
};
}

int main(int argc, char** argv)
{
	if (argc != 2) { std::fputs("Pass the fixture WebSocket URL\n", stderr); return 2; }
#ifdef _WIN32
	_putenv_s("DARKEDEN_WEBSOCKET_URL", argv[1]);
#else
	setenv("DARKEDEN_WEBSOCKET_URL", argv[1], 1);
#endif
	try {
#ifdef __EMSCRIPTEN__
		auto* probe = new Probe;
		emscripten_set_main_loop_arg([](void* data) {
			auto* active = static_cast<Probe*>(data);
			active->tick();
			if (active->done) {
				const int result = active->result;
				emscripten_cancel_main_loop();
				delete active;
				emscripten_force_exit(result);
			}
		}, probe, 0, false);
		return 0;
#else
		Probe probe;
		while (!probe.done) { probe.tick(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
		return probe.result;
#endif
	} catch (Throwable& error) {
		std::fprintf(stderr, "TRANSPORT FAIL: %s\n", error.toString().c_str());
		return 1;
	}
}
