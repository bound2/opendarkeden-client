#include "test_framework.h"
#include "u_button.h"

#include <cstring>
#include <new>

namespace {
struct ButtonHandler : Exec
{
	int calls = 0;
	id_t last = 999;
	void Run(id_t id) override { ++calls; last = id; }
};

struct DefaultButton
{
	alignas(Button) unsigned char storage[sizeof(Button)];
	Button* button;
	DefaultButton()
	{
		std::memset(storage, 0xCD, sizeof(storage));
		button = ::new (storage) Button;
		button->Set(0, 0, 30, 20);
	}
	~DefaultButton() { button->~Button(); }
};
}

TEST(UIButton, DefaultButtonRunsOnReleaseWithDefaultId)
{
	DefaultButton fixture;
	ButtonHandler handler;
	Button& button = *fixture.button;
	CHECK_EQ(0, button.GetID());
	button.SetExecHandler(&handler);
	button.MouseControl(M_MOVING, 10, 10);
	button.MouseControl(M_LEFTBUTTON_DOWN, 10, 10);
	CHECK_EQ(0, handler.calls);
	button.MouseControl(M_LEFTBUTTON_UP, 10, 10);
	CHECK_EQ(1, handler.calls);
	CHECK_EQ(0, handler.last);
}

TEST(UIButton, DefaultButtonAcceptsClicksBeforeAHandlerIsInstalled)
{
	DefaultButton fixture;
	Button& button = *fixture.button;
	// Set the click mode explicitly to exercise the default handler pointer.
	button.SetClickOption(Button::RUN_WHEN_PUSHUP);
	button.MouseControl(M_MOVING, 10, 10);
	button.MouseControl(M_LEFTBUTTON_DOWN, 10, 10);
	button.MouseControl(M_LEFTBUTTON_UP, 10, 10);
	CHECK(!button.GetPressState());
}

TEST(UIButton, ExplicitButtonSettingsStillRunOnPress)
{
	ButtonHandler handler;
	Button button(0, 0, 30, 20, 37, &handler, Button::RUN_WHEN_PUSH);
	button.MouseControl(M_MOVING, 10, 10);
	button.MouseControl(M_LEFTBUTTON_DOWN, 10, 10);
	CHECK_EQ(1, handler.calls);
	CHECK_EQ(37, handler.last);
	button.MouseControl(M_LEFTBUTTON_UP, 10, 10);
	CHECK_EQ(1, handler.calls);
}
