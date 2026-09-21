#pragma once

// In-place UTF-8 truncation by byte budget, reserving three bytes for "..."
// when maxBytes >= 3. Smaller positive limits clip without a marker. The input
// must be writable and terminated. These functions only shorten it and never
// write past its existing terminator; non-positive limits leave it unchanged.
// They do not decode legacy encodings or repair already-malformed input.
void ReduceString(char* text, int maxBytes);
void ReduceString2(char* text, int maxBytes);
void ReduceString3(char* text, int maxBytes);
