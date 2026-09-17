#pragma once

union SDL_Event;

namespace DXInput {
// Handle one SDL2 event after polling. Requires initialized input; the game
// event pump and tests share this dispatcher. SDL event data is not retained.
void ProcessEvent(const SDL_Event& event);
}
