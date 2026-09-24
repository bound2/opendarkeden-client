//////////////////////////////////////////////////////////////////////////////
/// \file LeakMemoryDumper.cpp
/// \author sonee
/// \date 2004.12.30
//////////////////////////////////////////////////////////////////////////////

#include "Client_PCH.h"
#include "LeakMemoryDumper.h"
#include "SafeFormat.h"

#ifdef ENABLE_LEAK_TRACKER

// LeakMemoryDumper.h ends with `#define new DEBUG_NEW`. Inside the tracker's
// own implementation that macro is a hazard rather than a help: any `new` here
// would be routed back through AddTrack() and recurse. This file allocates
// with malloc() and placement new only.
#undef new

typedef struct {
	uintptr_t	address;
	size_t	size;
	char	file[64];
	DWORD	line;
} ALLOC_INFO;

typedef std::list<ALLOC_INFO*> AllocList;

// The list must not be a plain global. Our operator delete is a replaceable
// global one, so deletes happen during the static initialization of other
// translation units - before a global here would have been constructed - and
// during static destruction after it would have been destroyed. Either way
// the list gets iterated while its head node is garbage, which faults.
//
// Construct on first use and never destroy: the storage is raw so no
// constructor runs at load time, and leaking it deliberately keeps the list
// valid for deletes that run after main() returns. The tracker is a debugging
// aid, so one leaked list at exit costs nothing.
static AllocList &getAllocList()
{
	static bool constructed = false;
	static char storage[sizeof(AllocList)];
	AllocList *list = reinterpret_cast<AllocList *>(storage);
	if (!constructed)
	{
		constructed = true;
		// Placement new spelled through the global scope operator, and only
		// safe here because this file's `#define new DEBUG_NEW` is undone
		// below - otherwise this would macro-expand into the tracked
		// placement new and recurse back into AddTrack().
		::new (static_cast<void *>(storage)) AllocList();
	}
	return *list;
}

void AddTrack(uintptr_t addr, size_t asize, const char *fname, DWORD lnum)
{
	ALLOC_INFO *info;
	// Must bypass the tracked new(__FILE__, __LINE__) here (see
	// LeakMemoryDumper.h's #define new DEBUG_NEW) - `new (ALLOC_INFO)`
	// would macro-expand to that same tracked placement new, which calls
	// AddTrack() again to track *this* allocation, infinitely recursing
	// (unconditional stack overflow the first time anything anywhere
	// calls `new`). Allocate the bookkeeping node with a plain malloc()
	// instead.
	info = (ALLOC_INFO*)malloc(sizeof(ALLOC_INFO));
	info->address = addr;
	SafeFormat::Copy(info->file, fname);
	info->line = lnum;
	info->size = asize;
	getAllocList().insert(getAllocList().begin(), info);
}

void RemoveTrack(uintptr_t addr)
{
	AllocList::iterator i;

	for(i = getAllocList().begin(); i != getAllocList().end(); i++)
	{
		if((*i)->address == addr)
		{
			getAllocList().remove((*i));
			break;
		}
	}
}

void DumpUnfreed()
{
	AllocList::iterator i;
	size_t totalSize = 0;
	char buf[1024];

	for(i = getAllocList().begin(); i != getAllocList().end(); i++)
	{
		snprintf(buf, sizeof(buf), "%-50s:\t\tLINE %d,\t\tADDRESS 0x%p\t%zu unfreed\n",
			(*i)->file, (*i)->line, (void*)(*i)->address, (*i)->size);
		OutputDebugString(buf);
		totalSize += (*i)->size;
	}
	snprintf(buf, sizeof(buf), "-----------------------------------------------------------\n");
	OutputDebugString(buf);
	snprintf(buf, sizeof(buf), "Total Unfreed: %zu bytes\n", totalSize);
	OutputDebugString(buf);
}

#endif