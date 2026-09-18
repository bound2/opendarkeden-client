#include "DXInputHost.h"

namespace DXInput {
namespace { Host host; }
void SetHost(const Host& value) { host = value; }
const Host& GetHost() { return host; }
void SetMousePosition(int x, int y) { if (host.mousePosition) host.mousePosition(x, y); }
void SetActiveApp(bool active) { if (host.activeApp) host.activeApp(active); }
}
