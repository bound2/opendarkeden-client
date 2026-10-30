#include "test_framework.h"
#include "game_ui_scene.h"
#include "VS_UI_ExtraDialog.h"
#include "VS_UI_Game.h"
#include "MShop.h"
#include "MShopShelf.h"
#include "MStorage.h"

#include <cstring>
#include <memory>
#include <new>

// The whole game UI, started headless (tests/support/game_ui_scene.h): that
// it starts and is released, what its game windows do that only it reaches
// (the stash window's close takes the inventory's deposit dialog with it, the
// shop window says which shop it shows), and two windows' first state, read
// off windows built over a fill pattern. No test here draws a frame.
namespace {

// The game windows, the tribe's interface and its inventory are private;
// an explicit instantiation may name a private member, so these read them.
template <class Tag, typename Tag::type Member>
struct Expose
{
	friend typename Tag::type Get(Tag) { return Member; }
};
struct GameOf { using type = C_VS_UI_GAME* C_VS_UI::*; friend type Get(GameOf); };
template struct Expose<GameOf, &C_VS_UI::m_pC_game>;
struct TribeOf { using type = C_VS_UI_TRIBE* C_VS_UI_GAME::*; friend type Get(TribeOf); };
template struct Expose<TribeOf, &C_VS_UI_GAME::m_pC_tribe_interface>;
struct InventoryOf { using type = C_VS_UI_INVENTORY* C_VS_UI_TRIBE::*; friend type Get(InventoryOf); };
template struct Expose<InventoryOf, &C_VS_UI_TRIBE::m_pC_inventory>;

struct InfoModeOf { using type = C_VS_UI_INFO::INFO_MODE C_VS_UI_INFO::*; friend type Get(InfoModeOf); };
template struct Expose<InfoModeOf, &C_VS_UI_INFO::m_info_mode>;
struct SkillOpenOf { using type = bool C_VS_UI_SKILL::*; friend type Get(SkillOpenOf); };
template struct Expose<SkillOpenOf, &C_VS_UI_SKILL::m_bl_open>;

// A window built over a fill pattern, as the heap may hand one out: a member
// its constructor leaves alone reads the pattern
// (docs/testing/uninitialised-members.md).
template <class T>
std::unique_ptr<T> OverFill()
{
	void* p = ::operator new(sizeof(T));
	std::memset(p, 0xAB, sizeof(T));
	return std::unique_ptr<T>(new (p) T);
}

// A member's bytes, read without loading it as its type.
template <class T>
unsigned Bytes(const T& member)
{
	unsigned value = 0;
	std::memcpy(&value, &member, sizeof(T) < sizeof(value) ? sizeof(T) : sizeof(value));
	return value;
}

// The inventory's money button, a protected id a derived class may name.
struct MoneyButton : C_VS_UI_INVENTORY
{
	static const int id = MONEY_ID;
};

C_VS_UI_INVENTORY* Inventory()
{
	C_VS_UI_GAME* pGame = gC_vs_ui.*Get(GameOf());
	C_VS_UI_TRIBE* pTribe = pGame != nullptr ? pGame->*Get(TribeOf()) : nullptr;
	return pTribe != nullptr ? pTribe->*Get(InventoryOf()) : nullptr;
}

// The money dialog in front of every window, if one is.
C_VS_UI_MONEY_DIALOG* MoneyDialogInFront()
{
	return dynamic_cast<C_VS_UI_MONEY_DIALOG*>(gpC_window_manager->GetFirstPriorityWindow());
}

// A shop with a shelf of every kind, as GCShopList makes one.
struct Shop : MShop
{
	Shop()
	{
		Init(MShopShelf::MAX_SHELF);
		for (int shelf = 0; shelf < MShopShelf::MAX_SHELF; ++shelf)
			SetShelf(shelf, MShopShelf::NewShelf(shelf));
	}
};

// A stash of one rack, as GCStashList makes it.
struct Stash
{
	MStorage* old = g_pStorage;
	MStorage stash;
	Stash()
	{
		stash.Init(1);
		g_pStorage = &stash;
	}
	~Stash() { g_pStorage = old; }
};

} // namespace

TEST(GameUIScene, TheGameWindowsStartAndAreReleased)
{
	WindowManager* const before = gpC_window_manager;
	{
		GameUIScene scene;
		CHECK(Inventory() != nullptr);
		CHECK(!gC_vs_ui.IsRunningStorage());
		Stash stash;
		gC_vs_ui.RunStorage();
		CHECK(gC_vs_ui.IsRunningStorage());
		gC_vs_ui.CloseStorage();
		CHECK(!gC_vs_ui.IsRunningStorage());
	}
	// Released: no game windows, and the UI's objects are the test's again.
	CHECK(gC_vs_ui.*Get(GameOf()) == nullptr);
	CHECK(gpC_window_manager == before);
}

// The deposit dialog the inventory's money button opens while the stash is
// up is the inventory's; closing the stash window closes it (task 13 of
// docs/reviews/NEXT.md).
TEST(GameUIScene, ClosingTheStashClosesTheDepositDialog)
{
	GameUIScene scene;
	g_pMoneyManager->SetMoney(500);
	Stash stash;
	gC_vs_ui.RunStorage();
	C_VS_UI_INVENTORY* pInventory = Inventory();
	CHECK(pInventory != nullptr);
	if (pInventory == nullptr)
		return;
	pInventory->Run(MoneyButton::id);
	C_VS_UI_MONEY_DIALOG* pDialog = MoneyDialogInFront();
	CHECK(pDialog != nullptr);
	if (pDialog != nullptr)
		CHECK(pDialog->GetType() == C_VS_UI_MONEY_DIALOG::MONEY_DEPOSIT);
	gC_vs_ui.CloseStorage();
	CHECK(MoneyDialogInFront() == nullptr);
}

// The shop window says whether it shows a shop, so the model can ask before
// it frees one (task 11): only the shop it was handed, and none once closed.
TEST(GameUIScene, TheShopWindowSaysWhichShopItShows)
{
	GameUIScene scene;
	Shop shop, other;
	CHECK(!gC_vs_ui.IsShowingShop(&shop));
	gC_vs_ui.RunShop();
	gC_vs_ui.SetShop(&shop);
	CHECK(gC_vs_ui.IsShowingShop(&shop));
	CHECK(!gC_vs_ui.IsShowingShop(&other));
	CHECK(!gC_vs_ui.IsShowingShop(nullptr));
	gC_vs_ui.CloseShop();
	CHECK(!gC_vs_ui.IsShowingShop(&shop));
}

// The info window opens on its character tab. Its constructor read the mode
// (through Run, the grade clamps) before anything set it; its Start sets the
// tab it opens on, later. Built beside no game windows of the scene's.
TEST(GameUIScene, TheInfoWindowStartsOnItsCharacterTab)
{
	GameUIScene scene(false);
	std::unique_ptr<C_VS_UI_INFO> pInfo = OverFill<C_VS_UI_INFO>();
	CHECK_EQ(unsigned(C_VS_UI_INFO::CHARACTER_INFO_MODE), Bytes((*pInfo).*Get(InfoModeOf())));
}

// The skill window starts closed, as its Start leaves it: the tribe window asks
// it for a focus slot as it appears, before that Start runs. Built beside no
// game windows of the scene's, whose skill window shares its sprite packs.
TEST(GameUIScene, TheSkillWindowStartsClosed)
{
	GameUIScene scene(false);
	std::unique_ptr<C_VS_UI_SKILL> pSkill = OverFill<C_VS_UI_SKILL>();
	CHECK_EQ(0u, Bytes((*pSkill).*Get(SkillOpenOf())));
}
