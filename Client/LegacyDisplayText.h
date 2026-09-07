#ifndef LEGACY_DISPLAY_TEXT_H
#define LEGACY_DISPLAY_TEXT_H

#include "MString.h"
#include <string>
#include <string_view>

// The distributed .inf tables contain private-server branding in display
// names and item descriptions. Apply only to those fields, never resource
// filenames, player names, chat, or the general string decoder.
inline void CleanLegacyDisplayText(MString& value)
{
    if (!value.GetString()) return;
    std::string text(value.GetString(), value.GetLength());
    auto lower = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
    auto word = [](unsigned char c) { return c >= 128 || (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); };
    auto space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    bool changed = false;
    for (std::string_view token : {"https://www.dk2th.com", "http://www.dk2th.com",
            "https://dk2th.com", "http://dk2th.com", "www.dk2th.com", "dk2th.com", "dk2th"}) {
        for (size_t i = 0; i + token.size() <= text.size();) {
            bool matches = (i == 0 || !word(text[i - 1]))
                && (i + token.size() == text.size() || !word(text[i + token.size()]));
            for (size_t j = 0; matches && j < token.size(); ++j)
                matches = lower(text[i + j]) == token[j];
            if (!matches) { ++i; continue; }
            size_t end = i + token.size();
            // Remove adjacent name separators, preserving one space between
            // real words (for example, "Cabra dk2th  B1" -> "Cabra B1").
            while (i > 0 && (space(text[i - 1]) || text[i - 1] == '_')) --i;
            while (end < text.size() && (space(text[end]) || text[end] == '_')) ++end;
            text.replace(i, end - i, i > 0 && end < text.size() ? " " : "");
            changed = true;
        }
    }
    if (!changed) return;
    while (!text.empty() && space(text.back())) text.pop_back();
    while (!text.empty() && space(text.front())) text.erase(0, 1);
    value = text.c_str();
    // Empty descriptions must remain readable strings for legacy UI callers.
    if (!value.GetString()) value.Init(0);
}

#endif
