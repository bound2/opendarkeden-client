#include "test_framework.h"
#include "CDirectInput.h"
#include "DXLibBackend.h"
#include "DXInputHost.h"
#include <SDL.h>
#include <vector>

#ifndef DXLIB_BACKEND_SDL
#error dxlib must publish its backend selection to consumers
#endif

namespace {
struct Event { CSDLInput::E_MOUSE_EVENT kind; int x, y, z; };
std::vector<Event> events;
int hostX = 0, hostY = 0;
void Receive(CSDLInput::E_MOUSE_EVENT kind, int x, int y, int z)
{
	CHECK_EQ(x, hostX); CHECK_EQ(y, hostY);
	events.push_back({kind, x, y, z});
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
