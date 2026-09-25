#include "test_framework.h"
#include "CDirectInput.h"
#include "DXLibBackend.h"
#include "DXInputHost.h"
#include "DXInputEvents.h"
#include <SDL.h>
#include <vector>
#include <limits>
#include <cstring>
#include <string>

TEST(InputAdapter, VirtualKeyTapSurvivesTwoEventPumpsAndOneAdapterFrame)
{
	CSDLInput input;
	CHECK(input.Init(nullptr, nullptr));
	dxlib_input_virtual_key(DIK_F1, 1);
	dxlib_input_virtual_key(DIK_F1, 0);
	dxlib_input_update();
	dxlib_input_update();
	input.UpdateInput();
	CHECK(input.KeyDown(DIK_F1));
	input.UpdateInput();
	CHECK(!input.KeyDown(DIK_F1));
}

TEST(InputAdapter, VirtualHeldKeysAndCancellationCannotStick)
{
	CSDLInput input;
	CHECK(input.Init(nullptr, nullptr));
	dxlib_input_virtual_key(DIK_LMENU, 1);
	input.UpdateInput();
	input.UpdateInput();
	CHECK(input.KeyDown(DIK_LMENU));
	dxlib_input_virtual_key(DIK_F2, 1);
	dxlib_input_virtual_reset();
	input.UpdateInput();
	CHECK(!input.KeyDown(DIK_LMENU));
	CHECK(!input.KeyDown(DIK_F2));
	dxlib_input_virtual_key(-1, 1);
	dxlib_input_virtual_key(256, 1);
	CHECK(!dxlib_input_key_down(-1));
	CHECK(!dxlib_input_key_down(256));
}

#ifndef DXLIB_BACKEND_SDL
#error dxlib must publish its backend selection to consumers
#endif

namespace {
struct Event { CSDLInput::E_MOUSE_EVENT kind; int x, y, z; };
std::vector<Event> events;
int hostX = 0, hostY = 0;
bool activated = false;
unsigned int keyValue = 0;
std::string textValue, editValue;
int editStart = 0, editLength = 0;
int graphicsResets = 0;
void Receive(CSDLInput::E_MOUSE_EVENT kind, int x, int y, int z)
{
	CHECK_EQ(x, hostX); CHECK_EQ(y, hostY);
	events.push_back({kind, x, y, z});
}

struct Session {
	CSDLInput input;
	Session()
	{
		CHECK_EQ(0, SDL_InitSubSystem(SDL_INIT_EVENTS));
		SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
		DXInput::SetHost({.mousePosition = [](int x, int y) { hostX = x; hostY = y; }});
		CHECK(input.Init(nullptr, nullptr));
		input.SetMouseEventReceiver(Receive);
		events.clear();
	}
	~Session() { DXInput::SetHost({}); SDL_QuitSubSystem(SDL_INIT_EVENTS); }
};

void Wheel(int amount)
{
	SDL_Event event{};
	event.type = SDL_MOUSEWHEEL;
	event.wheel.y = amount;
	// sdl2-compat converts pushed wheel events through SDL3, losing the integer
	// delta, and cannot push text events. Feed the actual post-poll dispatcher.
	DXInput::ProcessEvent(event);
}
}

TEST(InputAdapter, LinksRealInputAndPublishesPositionBeforeOrderedCallbacks)
{
	DXInput::SetHost({.mousePosition = [](int x, int y) { hostX = x; hostY = y; }});
	CSDLInput input;
	input.SetMouseEventReceiver(Receive);
	events.clear();
	input.DispatchMouseAt(CSDLInput::LEFTDOWN, 15, 25);
	CHECK_EQ(2, events.size());
	if (events.size() == 2) {
		CHECK_EQ(CSDLInput::MOVE, events[0].kind);
		CHECK_EQ(CSDLInput::LEFTDOWN, events[1].kind);
		CHECK_EQ(15, events[1].x); CHECK_EQ(25, events[1].y);
	}
	DXInput::SetHost({});
	input.SetMouseEventReceiver(nullptr);
	input.DispatchMouseAt(CSDLInput::MOVE, 30, 40);
	CHECK_EQ(30, input.m_mouse_x); CHECK_EQ(40, input.m_mouse_y);
}

TEST(InputAdapter, KeyQueriesOutsideTheTableCannotReadMouseState)
{
	CSDLInput input;
	input.m_mouse_x = input.m_mouse_y = input.m_mouse_z = 1;
	CHECK(!input.KeyDown(256)); CHECK(!input.KeyDown(257)); CHECK(!input.KeyDown(258));
	CHECK(!input.KeyDown(0)); CHECK(!input.KeyDown(255));
	CHECK(!input.KeyDown((std::numeric_limits<DWORD>::max)()));
}

TEST(InputAdapter, RendererResetEventsRequestAnApplicationRedraw)
{
	Session session;
	graphicsResets = 0;
	DXInput::SetHost({.graphicsReset = [] { ++graphicsResets; }});
	SDL_Event event{};
	event.type = SDL_RENDER_TARGETS_RESET;
	DXInput::ProcessEvent(event);
	event.type = SDL_RENDER_DEVICE_RESET;
	DXInput::ProcessEvent(event);
	CHECK_EQ(2, graphicsResets);
	DXInput::SetHost({});
	DXInput::ProcessEvent(event);
	CHECK_EQ(2, graphicsResets);
}

TEST(InputAdapter, AModeChangeDoesNotReplayTheLastWheelMovement)
{
	Session session;
	Wheel(5);
	session.input.UpdateInput();
	events.clear();
	session.input.SetMouseMoveLimit(1024, 768);
	session.input.UpdateInput();
	CHECK(events.empty());
	CHECK_EQ(0, session.input.m_mouse_z);
}

TEST(InputAdapter, ConsecutiveFramesDeliverTheirOwnSignedWheelDelta)
{
	Session session;
	for (int delta : {3, -2, 0, 4}) {
		events.clear();
		if (delta) Wheel(delta);
		session.input.UpdateInput();
		CHECK_EQ(delta, session.input.m_mouse_z);
		CHECK_EQ(delta == 0 ? 0 : 1, events.size());
		if (delta && events.size() == 1) {
			CHECK_EQ(delta > 0 ? CSDLInput::WHEELUP : CSDLInput::WHEELDOWN, events[0].kind);
			CHECK_EQ(delta, events[0].z);
		}
	}
}

TEST(InputAdapter, BackendWheelReadsConsumeThePendingDelta)
{
	Session session;
	Wheel(7);
	dxlib_input_update();
	CHECK_EQ(7, dxlib_input_get_mouse_wheel());
	CHECK_EQ(0, dxlib_input_get_mouse_wheel());
}

TEST(InputAdapter, OuterLoopAndAdapterPumpsDoNotLosePendingWheelInput)
{
	Session session;
	Wheel(2);
	dxlib_input_update(); // The non-Windows application loop also pumps events.
	session.input.UpdateInput();
	CHECK_EQ(2, session.input.m_mouse_z);
	CHECK_EQ(1, events.size());
}

TEST(InputAdapter, ModeChangesDiscardPumpedButUnconsumedWheelInput)
{
	Session session;
	Wheel(2);
	dxlib_input_update();
	session.input.SetMouseMoveLimit(1024, 768);
	session.input.UpdateInput();
	CHECK_EQ(0, session.input.m_mouse_z);
	CHECK(events.empty());
}

TEST(InputAdapter, LargeWheelDeltasAreSummedBeforeNarrowing)
{
	Session session;
	const int maximum = (std::numeric_limits<int>::max)();
	Wheel(maximum); Wheel(maximum); Wheel(-maximum);
	session.input.UpdateInput();
	CHECK_EQ(maximum, session.input.m_mouse_z);
	events.clear();
	Wheel(-maximum); Wheel(-maximum);
	session.input.UpdateInput();
	CHECK_EQ((std::numeric_limits<int>::min)(), session.input.m_mouse_z);
	CHECK_EQ(1, events.size());
	if (events.size() == 1) CHECK_EQ(CSDLInput::WHEELDOWN, events[0].kind);
}

TEST(InputAdapter, EventDispatcherDeliversActivationAndTextThroughTheInstalledHost)
{
	Session session;
	activated = false; keyValue = 0; textValue.clear(); editValue.clear();
	DXInput::SetHost({
		.mousePosition = [](int x, int y) { hostX = x; hostY = y; },
		.activeApp = [](bool active) { activated = active; },
		.hasTextFocus = []() { return true; },
		.keyDown = [](unsigned int key) { keyValue = key; },
		.textInput = [](const char* text) { textValue = text; },
		.textEditing = [](const char* text, int start, int length) {
			editValue = text; editStart = start; editLength = length;
		}
	});
	SDL_Event event{};
	event.type = SDL_WINDOWEVENT; event.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
	DXInput::ProcessEvent(event);
	event = {}; event.type = SDL_KEYDOWN; event.key.keysym.sym = SDLK_LEFT;
	DXInput::ProcessEvent(event);
	event = {}; event.type = SDL_TEXTINPUT; std::memcpy(event.text.text, "hello", 6);
	DXInput::ProcessEvent(event);
	event = {}; event.type = SDL_TEXTEDITING; std::memcpy(event.edit.text, "abc", 4);
	event.edit.start = 1; event.edit.length = 2;
	DXInput::ProcessEvent(event);
	session.input.UpdateInput();
	CHECK(activated); CHECK_EQ(0x25, keyValue);
	CHECK(textValue == "hello"); CHECK(editValue == "abc");
	CHECK_EQ(1, editStart); CHECK_EQ(2, editLength);

	DXInput::SetHost({}); // A standalone consumer can omit application callbacks.
	DXInput::ProcessEvent(event);
	session.input.UpdateInput();
}

TEST(InputAdapter, EventPumpDispatchesQueuedControlKeys)
{
	Session session;
	keyValue = 0;
	DXInput::SetHost({
		.hasTextFocus = []() { return true; },
		.keyDown = [](unsigned int key) { keyValue = key; }
	});
	SDL_Event event{};
	event.type = SDL_KEYDOWN;
	event.key.state = SDL_PRESSED;
	event.key.keysym.sym = SDLK_LEFT;
	event.key.keysym.scancode = SDL_SCANCODE_LEFT;
	CHECK_EQ(1, SDL_PushEvent(&event));
	CHECK_EQ(0, keyValue);
	session.input.UpdateInput();
	CHECK_EQ(0x25, keyValue);
}
