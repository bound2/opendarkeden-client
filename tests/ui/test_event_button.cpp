#include "test_framework.h"
#include "VS_UI_widget.h"

#include <cstring>
#include <new>

TEST(UIEventButton, MissingImageUsesIdRegardlessOfPreviousStorage)
{
	alignas(C_VS_UI_EVENT_BUTTON) unsigned char storage[sizeof(C_VS_UI_EVENT_BUTTON)];
	std::memset(storage, 0xCD, sizeof(storage));
	auto* button = ::new (storage) C_VS_UI_EVENT_BUTTON(0, 0, 30, 20, 45, nullptr);
	CHECK_EQ(45, button->m_image_index);
	CHECK_EQ(0, button->m_alpha);
	button->~C_VS_UI_EVENT_BUTTON();
}

TEST(UIEventButton, ExplicitImageSurvivesConstructionAndAnimationReset)
{
	alignas(C_VS_UI_EVENT_BUTTON) unsigned char storage[sizeof(C_VS_UI_EVENT_BUTTON)];
	std::memset(storage, 0xFF, sizeof(storage));
	auto* button = ::new (storage) C_VS_UI_EVENT_BUTTON(0, 0, 30, 20, 45, nullptr, 7);
	CHECK_EQ(7, button->m_image_index);
	button->m_alpha = 20;
	button->Init();
	CHECK_EQ(7, button->m_image_index);
	CHECK_EQ(0, button->m_alpha);
	button->~C_VS_UI_EVENT_BUTTON();
}
