// Compile this consumer without per-source definitions: the libraries must
// publish the macros used by their public headers through their link targets.
#ifndef PLATFORM_USE_SDL
static_assert(false, "basic must publish its SDL selection to consumers");
#endif

#if defined(_WIN32) && (!defined(__WIN32__) || !defined(__WINDOWS__))
static_assert(false, "packetwire must publish its Windows header definitions to consumers");
#endif

#include "Platform.h"
#include "DXLibBackend.h"
#include "Assert1.h"
#include "Types/SystemTypes.h"

#ifndef DXLIB_BACKEND_SDL
#error dxlib consumers must select the same SDL backend as the library
#endif

#ifndef Assert
#error packetwire assertions must be available to ordinary consumers
#endif
