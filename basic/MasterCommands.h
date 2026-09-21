#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace MasterCommands {
struct Limits {
	size_t maxCommandBytes = 128;
	// Counts nonempty visited rows, including nested invocations.
	size_t maxCommands = 1024, maxDepth = 10;
	size_t maxFileBytes = 1024 * 1024, maxTotalBytes = 4 * 1024 * 1024;
};
using Reader = std::function<bool(unsigned, std::string&)>;

// The client command is *mc followed by a space/tab and one digit. Outer ASCII
// spaces follow the chat UI's trim policy. Failure leaves number unchanged.
bool Invocation(std::string_view text, unsigned& number);

// Read raw resource bytes and strictly decode each file under the pack/BOM policy. Build
// a complete ordered plan before any caller dispatches it. Cycles, incomplete
// loads and limits reject the whole plan, leaving output unchanged. Repeated
// nonrecursive includes retain their order. No game commands are executed here.
bool Expand(unsigned number, const Reader& read, std::vector<std::string>& output, Limits limits = {});
bool Load(unsigned number, const std::string& directory, std::vector<std::string>& output, Limits limits = {});
}
