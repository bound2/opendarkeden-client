#pragma once

#include "Platform.h"

// File-backed implementation used by the POSIX platform_config_* entry points.
// An explicit path also lets tools use the same format without changing the
// executable's settings. Return zero on success, one on failure.
// Files and resulting updates are capped at 1 MiB. Writes replace matching
// entries and commit through a sibling temporary file; unrelated lines survive.
// A short/null output reports the required size (including NUL) and returns one.
namespace ConfigFile {
int GetString(const char* filename, const char* key, const char* value, char* buffer, DWORD* size);
int SetString(const char* filename, const char* key, const char* value, const char* data);
}
