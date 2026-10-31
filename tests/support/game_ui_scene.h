//----------------------------------------------------------------------
// game_ui_scene.h - the whole game UI, started headless
//----------------------------------------------------------------------
//
// GameUIScene starts the UI's game windows (gC_vs_ui's C_VS_UI_GAME) as
// UI_StartGame does, for a slayer, so a test can drive the real windows:
// the stash, the shop, the inventory and their dialogs. What the game
// makes before it starts them, and the windows read as they are built:
//
//   - the UI's own objects (the base on a back surface, the process
//     runner, the window managers, the global resource, the mouse
//     pointer, the skins with 64 points and rects per row), on the heap,
//     since C_VS_UI::Release frees them;
//   - the game's strings (InitGameStringTable, once per process: the
//     windows cache pointers into the table in function statics);
//   - the skill tree, each slayer domain's root skill at the apprentice
//     step, as the game's data has it (the info window counts each
//     domain's apprentice list), the rank bonus table, the experience
//     table and the profiles;
//   - a creature table of 1024 named rows (the quest status window joins
//     names it reads by fixed index), the party, the user's options and
//     accelerators, the chat manager, the money, the inventory, the three
//     gears and the available skills.
//
// It runs in a working directory of its own, since the game windows read
// and write the character's files under UserSet/. When it ends, what it made
// is freed (the UI's objects by C_VS_UI::Release, the rest by the scene),
// the globals it sets are put back, with the ones the game windows are seen
// to set as they run (the item locks, the chat history height, the empty
// move, the character slot), and the IME's blinker, the text surface, the
// descriptor and the focused editor are cleared. A window's function
// statics are not reset. It draws no frame. A vampire or ousters interface
// needs its domain's root skill at its first step as well; no test asks for
// one yet.
//
//----------------------------------------------------------------------

#ifndef __GAME_UI_SCENE_H__
#define __GAME_UI_SCENE_H__

#include "VS_UI.h"
#include "VS_UI_Base.h"
#include "VS_UI_GameCommon.h"
#include "VS_UI_GlobalResource.h"
#include "VS_UI_mouse_pointer.h"
#include "MGameStringTable.h"
#include "MStringArray.h"
#include "MZoneTable.h"
#include "UserInformation.h"
#include "RaceType.h"
#include "SkinManager.h"
#include "MSkillManager.h"
#include "RankBonusTable.h"
#include "ProfileManager.h"
#include "ExperienceTable.h"
#include "MParty.h"
#include "UserOption.h"
#include "KeyAccelerator.h"
#include "AcceleratorDef.h"
#include "MChatManager.h"
#include "MMoneyManager.h"
#include "MInventory.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "MCreatureTable.h"
#include "pi_core.h"
#include "u_window.h"
#include "timer2.h"
#include "Ci.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackendSDL.h"
#include "SpriteLib/SpriteLibBackend.h"
#include "type_table_access.h"
#include "VS_UI_descriptor.h"
#include "src/InputFocusManager.h"
#include "Fl2.h"
#include "test_framework.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

struct GameUIScene
{
	static const int kWidth = 800;
	static const int kHeight = 600;

	CSpriteSurface back;
	CI_KOREAN ime;

	// bStartGame false builds what the game windows read but starts none, for
	// a test that builds a window of its own.
	explicit GameUIScene(bool bStartGame = true)
	{
		// Left over from another test, Release would free it with the scene's.
		CHECK(!gC_vs_ui.IsRunningTitle());
		spritectl_init();
		// The game windows read and write the character's files under UserSet/
		// in the working directory (ProfilePath): a directory of the scene's own.
		static unsigned sequence = 0;
		m_OldDir = std::filesystem::current_path();
		m_Dir = std::filesystem::temp_directory_path() / ("darkeden_game_ui_scene_"
			+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "_" + std::to_string(sequence++));
		std::filesystem::create_directories(m_Dir);
		std::filesystem::current_path(m_Dir);
		Keep(g_GameRect);
		g_GameRect = {0, 0, kWidth, kHeight};
		back.Init(kWidth, kHeight);
		Keep(gC_ci);
		gC_ci = &ime;
		Keep(g_eRaceInterface);
		g_eRaceInterface = RACE_SLAYER;
		// A window that closes asks for a mouse move on the next process.
		Keep(gbl_global_empty_move);
		// And what the game windows set as they start and run.
		Keep(gbl_item_lock);
		Keep(gbl_gear_lock);
		Keep(gbl_item_trade_lock);
		Keep(g_HISTORY_LINE);
		Keep(g_char_slot_ingame);
		g_char_slot_ingame = S_SLOT();

		// The UI's objects: C_VS_UI::Release frees them.
		Keep(gpC_base);
		Keep(gpC_process_runner);
		Keep(gpC_window_manager);
		Keep(gpC_vs_ui_window_manager);
		Keep(gpC_global_resource);
		Keep(gpC_mouse_pointer);
		Keep(g_pSkinManager);
		Keep(gpC_item);
		gpC_base = new Base;
		// Base::Init starts the IME's cursor blinker, the timer after this one;
		// it is deleted when the scene ends.
		timer_id_t probe = gC_timer2.Add(1000, NoTimer);
		m_Blink = probe + 1;
		gC_timer2.Delete(probe);
		gpC_base->Init(&back, Received);
		gpC_process_runner = new PI_ProcessRunner;
		gpC_window_manager = new WindowManager;
		gpC_vs_ui_window_manager = new C_VS_UI_WINDOW_MANAGER;
		gpC_global_resource = new C_GLOBAL_RESOURCE;
		gpC_mouse_pointer = new C_VS_UI_MOUSE_POINTER;
		gpC_item = NULL;
		g_pSkinManager = new SkinManager;
		for (int i = 0; i < SkinManager::INTERFACE_MAX; ++i)
			for (int n = 0; n < 64; ++n)
			{
				testfw::MutableRow(*g_pSkinManager, i).LoadFromLinePointList("0 0");
				testfw::MutableRow(*g_pSkinManager, i).LoadFromLineRectList("0 0 1 1");
			}

		Keep(g_pGameStringTable);
		g_pGameStringTable = &Strings();
		Make(g_pZoneTable, new CZoneTable);
		Make(g_pUserInformation, new UserInformation);
		Make(g_pSkillInfoTable, new MSkillInfoTable);
		for (int root : {SKILL_SINGLE_BLOW, SKILL_DOUBLE_IMPACT, SKILL_FAST_RELOAD, MAGIC_CREATE_HOLY_WATER, MAGIC_CURE_LIGHT_WOUNDS})
			testfw::MutableRow(*g_pSkillInfoTable, root).SetSkillStep(SKILL_STEP_APPRENTICE);
		Make(g_pSkillManager, new MSkillManager);
		g_pSkillManager->Init();
		Make(g_pRankBonusTable, new RankBonusTable);
		Make(g_pProfileManager, new ProfileManager);
		Make(g_pExperienceTable, new ExperienceTable);
		Make(g_pParty, new MParty);
		Make(g_pUserOption, new UserOption);
		Make(g_pKeyAccelerator, new KeyAccelerator);
		g_pKeyAccelerator->Init(MAX_ACCELERATOR);
		Make(g_pChatManager, new MChatManager);
		Make(g_pMoneyManager, new MMoneyManager);
		Make(g_pInventory, new MInventory);
		Make(g_pSlayerGear, new MSlayerGear);
		Make(g_pVampireGear, new MVampireGear);
		Make(g_pOustersGear, new MOustersGear);
		Make(g_pSkillAvailable, new MSkillSet);
		Make(g_pCreatureTable, new CREATURE_TABLE);
		g_pCreatureTable->Init(1024);
		for (int i = 0; i < 1024; ++i)
			testfw::MutableRow(*g_pCreatureTable, i).Name = "Creature";

		if (bStartGame)
			gC_vs_ui.StartGame();
	}

	~GameUIScene()
	{
		// The game windows and the UI's objects, then the model's.
		gC_vs_ui.Release();
		gC_timer2.Delete(m_Blink);
		g_SetFL2Surface(nullptr);
		g_descriptor_manager.Unset();
		InputFocusManager::GetInstance().SetFocusedEditor(nullptr);
		for (auto it = m_Undo.rbegin(); it != m_Undo.rend(); ++it)
			(*it)();
		std::filesystem::current_path(m_OldDir);
		std::error_code error;
		std::filesystem::remove_all(m_Dir, error);
	}

	GameUIScene(const GameUIScene&) = delete;
	GameUIScene& operator=(const GameUIScene&) = delete;

private:
	std::vector<std::function<void()>>	m_Undo;
	std::filesystem::path				m_OldDir;
	std::filesystem::path				m_Dir;
	timer_id_t						m_Blink = INVALID_TID;

	static void	Received(DWORD, intptr_t, intptr_t, void*) {}
	static void	NoTimer() {}

	// The game's strings, made once and kept to the end of the process.
	static MStringArray&	Strings()
	{
		static MStringArray table;
		static const bool filled = [] {
			MStringArray* old = g_pGameStringTable;
			g_pGameStringTable = &table;
			InitGameStringTable();
			g_pGameStringTable = old;
			return true;
		}();
		(void)filled;
		return table;
	}

	// Puts the global back to the value it holds now when the scene ends.
	template <class T>
	void	Keep(T& global)
	{
		T* slot = &global;
		m_Undo.push_back([slot, value = global]() { *slot = value; });
	}

	// Sets the global to a new object the scene frees, and puts it back.
	template <class T>
	void	Make(T*& global, T* value)
	{
		T** slot = &global;
		m_Undo.push_back([slot, old = global]() { delete *slot; *slot = old; });
		global = value;
	}
};

#endif
