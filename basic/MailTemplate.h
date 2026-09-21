#pragma once

#include <string>
#include <string_view>

namespace MailTemplate {
struct Data {
	int width = 0, height = 0;
	std::string sender, title, contents;
};

// Four decoded physical rows: dimensions, sender, title, contents. Trailing
// whitespace is allowed. Geometry must fit the caller's positive viewport.
// Failure preserves output; empty fields require their own physical row.
bool Parse(std::string_view text, int maxWidth, int maxHeight, Data& output);
}
