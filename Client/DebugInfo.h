// Executable-side compatibility names for the library logger.
#ifndef __DEBUGINFO_H__
#define __DEBUGINFO_H__

#ifdef PLATFORM_WINDOWS
#include "MinTr.h"
#endif
#include "DebugLog.h"

#define DEBUG_MESSAGE(debugMessage) ((void)0)
#define DEBUG_CMD(cmd, message) ((void)0)

#endif
