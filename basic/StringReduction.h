#pragma once

// In-place legacy byte truncation. The input must be a writable, terminated
// string. These functions only shorten it and never write past its existing
// terminator; non-positive limits leave it unchanged. Character-boundary
// handling is a separate concern from this storage guarantee.
void ReduceString(char* text, int maxBytes);
void ReduceString2(char* text, int maxBytes);
void ReduceString3(char* text, int maxBytes);
