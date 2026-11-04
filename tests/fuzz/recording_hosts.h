//----------------------------------------------------------------------
// recording_hosts.h
//----------------------------------------------------------------------
//
// The presentation hosts the handler targets install in place of the
// executable's (task 4 of docs/reviews/NEXT.md). The real adapters call
// into VS_UI, which the world never starts (gC_vs_ui is not initialised,
// so g_pUIDialog is NULL; handler_world.h makes only the mouse pointer),
// so these do what the real ones do to the model objects a handler hands
// them, which is what a sanitizer can see:
//
//   - an entry whose real adapter reads its object reads it here too,
//     so ASan reports an object a handler freed before it handed it on;
//     PickUpItem's (UI_PickUpItem) reads it without testing it, so a
//     NULL aborts here, the harness's crash path, as it would crash
//     there;
//   - the shop, the storage, the pet storage and the personal shop
//     windows draw from the object they were given on every frame
//     (their Show walks its slots). As in VS_UI, a Run entry opens a
//     window with no object if none is open, a Set entry hands an open
//     window its object and is dropped while it is closed, and a close
//     forgets it; the closes are those a handler reaches
//     (ZoneLoading::CloseWindows closes the shop and the storage,
//     CloseStorage, ClosePetStorage and ClosePersnalShop their window,
//     ShopDestroyed the shop window if it shows that shop) and
//     CloseAllDialog, taken to close all four, which can only miss a
//     finding. IsRunning answers from that. After every call of the
//     receive loop, as the game draws once a call, and at the end of a
//     run, every open window's object is walked as its Show walks it, so
//     ASan reports one a packet freed while the window still drew it; a
//     call a rejected input ends closes them all first, since the client
//     leaves for the title (handler_world.h's LeaveForTitle). A free that
//     a later packet of the same call hides is not drawn by the game
//     either.
//   - the mouse pointer holds the item PickUpItem hands it, as
//     C_VS_UI_MOUSE_POINTER keeps the one it is handed until DropItem
//     forgets it, and it is read with the windows, so ASan reports an
//     item a packet freed while the mouse still drew it. A handler
//     reaches the item on the mouse only through GetMouseItem, which
//     stays empty here, so nothing here moves it on; the UI never frees
//     it either, so what the mouse holds when a run ends, or when a
//     later pick-up replaces it, is lost, a leak the runs do not count
//     (docs/protocol/fuzzing.md).
//
//   - the other-info window names its subject: GetOtherInfoName answers
//     kOtherInfoName, the peer handler_world.h stands in the zone, so
//     GCOtherModifyInfo for it reaches RunOtherInfo.
//
//   - every entry counts the calls it gets (g_Reached, by Entry). When
//     DE_FUZZ_REACH holds a comma-separated list of entry names, the
//     process prints every count as it ends and fails, status 1, if an
//     entry named was never reached or a name is no entry's: the owner
//     of what the seeds reach, the fuzz_seeds_reach_client_stream_handlers
//     ctest, which replays the seed directory with it.
//
// What the real adapters do with a handler's integers is not modelled;
// the walks of packet input safety followed them by reading.
//
//----------------------------------------------------------------------

#ifndef __RECORDING_HOSTS_H__
#define __RECORDING_HOSTS_H__

#include "DialogPresentationHost.h"
#include "GameInfoPresentationHost.h"
#include "VsUiPresentationHost.h"
#include "ZoneLoadingHost.h"
#include "MCreature.h"
#include "MItem.h"
#include "MShop.h"
#include "MShopShelf.h"
#include "MStorage.h"
#include "CrtCompat.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>

namespace RecordingHosts {

// The name of the creature the other-info window is open on.
inline const char* const	kOtherInfoName = "Peer";

// What a read leaves, so the compiler keeps the read.
inline volatile unsigned long	g_Sink = 0;

// The entries that take a handler's object or open a window, as
// DE_FUZZ_REACH names them.
enum Entry
{
	ENTRY_PICK_UP_ITEM,
	ENTRY_OK_MIXING_FORGE,
	ENTRY_OK_REMOVE_OPTION,
	ENTRY_RUN_SHOP,
	ENTRY_SET_SHOP,
	ENTRY_RUN_STORAGE,
	ENTRY_SET_STORAGE,
	ENTRY_RUN_PET_STORAGE,
	ENTRY_SET_PET_STORAGE,
	ENTRY_RUN_OTHER_INFO,
	ENTRY_RUN_QUEST_INVENTORY,
	ENTRY_RUN_XMAS_CARD_WINDOW,
	ENTRY_RUN_BULLETIN_BOARD_WINDOW,
	ENTRY_RUN_PERSNAL_SHOP,
	ENTRY_SET_PERSNAL_SHOP,
	ENTRY_MAX
};

inline const char* const	kEntryNames[ENTRY_MAX] = {
	"PickUpItem", "OkMixingForge", "OkRemoveOption",
	"RunShop", "SetShop", "RunStorage", "SetStorage", "RunPetStorage", "SetPetStorage",
	"RunOtherInfo", "RunQuestInventory", "RunXmasCardWindow", "RunBulletinBoardWindow",
	"RunPersnalShop", "SetPersnalShop",
};

// How many times each entry was called since the process started.
inline unsigned	g_Reached[ENTRY_MAX] = {};

inline void	Reach(Entry entry)
{
	++g_Reached[entry];
}

// Prints every entry's count and ends the process with status 1 if an
// entry DE_FUZZ_REACH names was never reached, or a name is no entry's;
// nothing when DE_FUZZ_REACH is not set. Registered to run at exit.
inline void	CheckReachAtExit()
{
	const std::optional<std::string> names = Basic::GetEnvironment("DE_FUZZ_REACH");
	if (!names)
		return;
	for (int i = 0; i < ENTRY_MAX; i++)
		std::fprintf(stderr, "recording_hosts: %s reached %u times\n", kEntryNames[i], g_Reached[i]);

	bool bReached = true;
	const std::string& list = *names;
	std::size_t start = 0;
	while (start <= list.size())
	{
		std::size_t end = list.find(',', start);
		if (end == std::string::npos)
			end = list.size();
		const std::string name = list.substr(start, end - start);
		start = end + 1;
		if (name.empty())
			continue;
		int found = -1;
		for (int i = 0; i < ENTRY_MAX; i++)
			if (name == kEntryNames[i])
				found = i;
		if (found < 0)
		{
			std::fprintf(stderr, "recording_hosts: DE_FUZZ_REACH names no entry %s\n", name.c_str());
			bReached = false;
		}
		else if (g_Reached[found] == 0)
		{
			std::fprintf(stderr, "recording_hosts: %s was not reached\n", name.c_str());
			bReached = false;
		}
	}
	if (!bReached)
		std::_Exit(1);
}

inline void	Read(const MItem* pItem)
{
	if (pItem != NULL)
		g_Sink += pItem->GetID() + pItem->GetItemClass();
}

inline void	Read(const MCreature* pCreature)
{
	if (pCreature != NULL)
		g_Sink += pCreature->GetID();
}

// As C_VS_UI_STORAGE::Show and the personal shop's walk the slots of the
// current storage.
inline void	Read(const MStorage* pStorage)
{
	if (pStorage == NULL)
		return;
	g_Sink += pStorage->GetSize() + pStorage->GetCurrent();
	for (unsigned int slot = 0; slot < STORAGE_SLOT; ++slot)
		Read(pStorage->GetItem(slot));
}

// C_VS_UI_SHOP::Show walks the current shelf's items; every shelf's is a
// superset of that.
inline void	Read(const MShop* pShop)
{
	if (pShop == NULL)
		return;
	for (unsigned int i = 0; i < pShop->GetSize(); ++i)
		if (const MShopShelf* pShelf = pShop->GetShelf(i))
			for (unsigned int slot = 0; slot < SHOP_SHELF_SLOT; ++slot)
				Read(pShelf->GetItem(slot));
}

// A window the UI draws an object in while it is open.
template <class T>
struct Window
{
	T*		pObject = NULL;
	bool	bOpen = false;

	// A window that is open stays as it is, as VS_UI makes one only once.
	void	Run()			{ if (!bOpen) { bOpen = true; pObject = NULL; } }
	void	Set(T* p)		{ if (bOpen) { pObject = p; Read(p); } }
	void	Close()			{ bOpen = false; pObject = NULL; }
	void	ReadIfOpen() const	{ if (bOpen) Read(pObject); }
};

inline Window<MShop>	g_Shop;
inline Window<MStorage>	g_Storage;
inline Window<MStorage>	g_PetStorage;
inline Window<MStorage>	g_PersnalShop;

// The item on the mouse pointer, the last one PickUpItem was handed,
// until DropItem.
inline MItem*	g_pMouseItem = NULL;

inline void	Reset()
{
	g_Shop.Close();
	g_Storage.Close();
	g_PetStorage.Close();
	g_PersnalShop.Close();
	g_pMouseItem = NULL;
}

// After every call of the receive loop, and at the end of a run before
// the world frees what it made.
inline void	ReadOpenWindows()
{
	g_Shop.ReadIfOpen();
	g_Storage.ReadIfOpen();
	g_PetStorage.ReadIfOpen();
	g_PersnalShop.ReadIfOpen();
	Read(g_pMouseItem);
}

// Installed over the empty hosts, after the model's; every other entry
// stays empty, and a missing entry is skipped as before.
inline void	Install()
{
	static const bool bChecksAtExit = (std::atexit(CheckReachAtExit), true);
	(void)bChecksAtExit;

	Reset();

	static DialogPresentation::Host		dialog;
	static GameInfoPresentation::Host	info;
	static VsUiPresentation::Host		ui;
	static ZoneLoading::Host			loading;

	dialog = {};
	dialog.PickUpItem = [](MItem* pItem) {
		Reach(ENTRY_PICK_UP_ITEM);
		if (pItem == NULL)
		{
			std::fprintf(stderr, "recording_hosts: PickUpItem(NULL), which UI_PickUpItem reads\n");
			std::abort();
		}
		Read(pItem);
		g_pMouseItem = pItem;
	};
	dialog.DropItem = []() { g_pMouseItem = NULL; };
	dialog.OkMixingForge = [](std::uint32_t, MItem* pItem, MItem* pItem2) { Reach(ENTRY_OK_MIXING_FORGE); Read(pItem); Read(pItem2); };
	dialog.OkRemoveOption = [](std::uint32_t, MItem* pItem) { Reach(ENTRY_OK_REMOVE_OPTION); Read(pItem); };
	dialog.RunShop = []() { Reach(ENTRY_RUN_SHOP); g_Shop.Run(); };
	dialog.SetShop = [](MShop* pShop) { Reach(ENTRY_SET_SHOP); g_Shop.Set(pShop); };
	dialog.IsRunningShop = []() { return g_Shop.bOpen; };
	dialog.RunStorage = []() { Reach(ENTRY_RUN_STORAGE); g_Storage.Run(); };
	dialog.SetStorage = [](MStorage* pStorage) { Reach(ENTRY_SET_STORAGE); g_Storage.Set(pStorage); };
	dialog.IsRunningStorage = []() { return g_Storage.bOpen; };
	dialog.RunPetStorage = []() { Reach(ENTRY_RUN_PET_STORAGE); g_PetStorage.Run(); };
	dialog.SetPetStorage = [](MStorage* pStorage) { Reach(ENTRY_SET_PET_STORAGE); g_PetStorage.Set(pStorage); };
	dialog.CloseAllDialog = []() { Reset(); };
	dialog.CloseStorage = []() { g_Storage.Close(); };
	dialog.ClosePetStorage = []() { g_PetStorage.Close(); };
	dialog.ShopDestroyed = [](const MShop* pShop) { if (g_Shop.bOpen && g_Shop.pObject == pShop) g_Shop.Close(); };
	DialogPresentation::SetHost(&dialog);

	info = {};
	info.GetOtherInfoName = []() -> const char* { return kOtherInfoName; };
	info.RunOtherInfo = [](MCreature* pCreature) { Reach(ENTRY_RUN_OTHER_INFO); Read(pCreature); };
	GameInfoPresentation::SetHost(&info);

	ui = {};
	ui.RunQuestInventory = [](const MItem* pItem) { Reach(ENTRY_RUN_QUEST_INVENTORY); Read(pItem); };
	ui.RunXmasCardWindow = [](const MItem* pItem) { Reach(ENTRY_RUN_XMAS_CARD_WINDOW); Read(pItem); };
	ui.RunBulletinBoardWindow = [](const MItem* pItem) { Reach(ENTRY_RUN_BULLETIN_BOARD_WINDOW); Read(pItem); };
	ui.RunPersnalShop = []() { Reach(ENTRY_RUN_PERSNAL_SHOP); g_PersnalShop.Run(); };
	ui.SetPersnalShop = [](MStorage* pStorage) { Reach(ENTRY_SET_PERSNAL_SHOP); g_PersnalShop.Set(pStorage); };
	ui.ClosePersnalShop = []() { g_PersnalShop.Close(); };
	ui.IsRunningPersnalShop = []() { return g_PersnalShop.bOpen; };
	VsUiPresentation::SetHost(&ui);

	loading = {};
	loading.CloseWindows = []() { g_Shop.Close(); g_Storage.Close(); };
	ZoneLoading::SetHost(&loading);
}

} // namespace RecordingHosts

#endif
