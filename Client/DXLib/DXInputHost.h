#pragma once

namespace DXInput {
// Application state and text editors reached by the input event pump.
// Callbacks are copied; an absent callback is a no-op (or no text focus).
struct Host {
	void (*mousePosition)(int x, int y) = nullptr;
	void (*activeApp)(bool active) = nullptr;
	bool (*hasTextFocus)() = nullptr;
	void (*keyDown)(unsigned int key) = nullptr;
	void (*textInput)(const char* text) = nullptr;
	void (*textEditing)(const char* text, int start, int length) = nullptr;
	void (*graphicsReset)() = nullptr;
};

void SetHost(const Host& host);
const Host& GetHost();
void SetMousePosition(int x, int y);
void SetActiveApp(bool active);
}
