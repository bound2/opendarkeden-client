//----------------------------------------------------------------------
// test_cpp20_container_helpers_2.cpp
//----------------------------------------------------------------------
//
// The second slice of library sites converted to the C++20 container,
// string and range helpers
// (docs/cpp17-cpp20-compatibility-assessment-2026-09-04.md, the
// post-migration backlog's priority 2): a map lookup compared with the
// end iterator written as contains(), four line-prefix tests written as
// starts_with(), and three hand written scans written as
// std::ranges::find / any_of / none_of.
//
// The conversion is meant to be invisible at run time, so these tests
// pin the OBSERVABLE contract of each converted function rather than
// its spelling, with the inputs that tell the two spellings apart:
// present versus absent, the empty container, the empty line, the line
// that is one character short of the keyword, and the line that is
// nothing but the character being tested for. They were written against
// the pre-conversion code and pass unchanged after it.
//
// Every check here goes through a public entry point of the library the
// converted line lives in; nothing reaches into a container directly.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "gamemodel_world.h"
#include "MGameStringTable.h"
#include "MSkillManager.h"
#include "SkillDef.h"
#include "SystemAvailabilities.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace {

//----------------------------------------------------------------------
// The skill fixture: a three-step chain, one level apart, so
// MSkillDomain's tree walk has children to go down into. Same shape as
// test_skill_core.cpp's - the clock is all the skill core wants from an
// item host.
//----------------------------------------------------------------------
DWORD	s_Frame = 0;
DWORD	s_Now = 0;

int			DropFrameCount(TYPE_FRAMEID)	{ return 0; }
void		RefreshAffect(MItem*)			{}
void		PlayItemSound(TYPE_SOUNDID)		{}
void		RecalculateStatus()				{}
void		ResetQuickItemSlot()			{}
void		RepairHint()					{}
MMagazine*	EmptyMagazineFor(MItem*)		{ return NULL; }

const MItemHost	s_Host = { &s_Frame, DropFrameCount, RefreshAffect, PlayItemSound, &s_Now,
							RecalculateStatus, ResetQuickItemSlot, RepairHint, EmptyMagazineFor };

const ACTIONINFO	kRoot	= SKILL_SINGLE_BLOW;
const ACTIONINFO	kChild	= SKILL_DOUBLE_IMPACT;
const ACTIONINFO	kLeaf	= SKILL_FAST_RELOAD;

struct SkillWorld : GameModelWorld
{
	SkillWorld()
	{
		s_Now = 0;

		g_pSkillInfoTable = new MSkillInfoTable;
		g_pSkillManager = new MSkillManager;
		g_pSkillAvailable = new MSkillSet;

		(*g_pSkillInfoTable)[kRoot].Set(0, "Single Blow", 0, 0, 0, "Single Blow");
		(*g_pSkillInfoTable)[kChild].Set(1, "Double Impact", 1, 0, 0, "Double Impact");
		(*g_pSkillInfoTable)[kLeaf].Set(2, "Fast Reload", 2, 0, 0, "Fast Reload");
		(*g_pSkillInfoTable)[kRoot].AddNextSkill(kChild);
		(*g_pSkillInfoTable)[kChild].AddNextSkill(kLeaf);

		MItem::SetHost(&s_Host);
	}

	~SkillWorld()
	{
		delete g_pSkillAvailable;	g_pSkillAvailable = NULL;
		delete g_pSkillManager;		g_pSkillManager = NULL;
		delete g_pSkillInfoTable;	g_pSkillInfoTable = NULL;
	}
};

//----------------------------------------------------------------------
// The language file the game string table reads its language out of.
//----------------------------------------------------------------------
const char* const	kLanguageFile	= "cpp20_container_helpers_2_language.inf";
const char* const	kMissingFile	= "cpp20_container_helpers_2_no_such_file.inf";

void	WriteLanguageFile(const char* text)
{
	std::ofstream out(kLanguageFile, std::ios::out | std::ios::binary | std::ios::trunc);
	out.write(text, (std::streamsize)strlen(text));
}

//----------------------------------------------------------------------
// The availability script, loaded from a stream the way the game data
// hands it over.
//----------------------------------------------------------------------
bool	LoadScript(SystemAvailabilitiesManager& m, const char* text)
{
	std::istringstream in(text);
	return m.LoadFromStream(in);
}

} // namespace

//----------------------------------------------------------------------
// MSkillDomain::IsExistSkillStep - std::map membership, with the empty
// map the old lookup answered by comparing against the end iterator
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers2, SkillDomainAnswersStepMembershipFromTheMap)
{
	SkillWorld world;

	// Two steps across the three skills of the chain.
	(*g_pSkillInfoTable)[kRoot].SetSkillStep(SKILL_STEP_APPRENTICE);
	(*g_pSkillInfoTable)[kChild].SetSkillStep(SKILL_STEP_APPRENTICE);
	(*g_pSkillInfoTable)[kLeaf].SetSkillStep(SKILL_STEP_ADEPT);

	MSkillDomain domain;

	// An empty domain holds no step at all.
	CHECK(domain.IsExistSkillStep(SKILL_STEP_APPRENTICE) == FALSE);
	CHECK(domain.IsExistSkillStep(SKILL_STEP_ADEPT) == FALSE);
	CHECK(domain.IsExistSkillStep(SKILL_STEP_NULL) == FALSE);

	domain.SetRootSkill(kRoot);

	// Present, present, and two that the walk never put in.
	CHECK(domain.IsExistSkillStep(SKILL_STEP_APPRENTICE) != FALSE);
	CHECK(domain.IsExistSkillStep(SKILL_STEP_ADEPT) != FALSE);
	CHECK(domain.IsExistSkillStep(SKILL_STEP_MASTER) == FALSE);
	CHECK(domain.IsExistSkillStep(SKILL_STEP_NULL) == FALSE);

	// The answer follows the map rather than a remembered flag: a clear
	// takes both steps away again.
	domain.Clear();
	CHECK(domain.IsExistSkillStep(SKILL_STEP_APPRENTICE) == FALSE);
	CHECK(domain.IsExistSkillStep(SKILL_STEP_ADEPT) == FALSE);
	CHECK(domain.GetSkillStepList(SKILL_STEP_APPRENTICE) == NULL);
}

//----------------------------------------------------------------------
// MSkillDomain::AddSkillStep - the scan that asks whether a skill is in
// the step list already.
//
// Its "already there" arm has no reachable input through the public API
// today: every caller reaches AddSkillStep once per skill between two
// clears (the tree walk skips a skill the domain already holds, and
// LoadFromFile clears before it rebuilds), so the scan answers "not in
// it" every time it runs. What the tests below pin is the arm that does
// run - over the empty list a step starts as, and over the list that
// already holds other skills - and the invariant the guard is there
// for, which is that no step list names a skill twice.
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers2, AStepListTakesEachSkillOnceHoweverOftenTheWalkRuns)
{
	SkillWorld world;

	// All three in one step, and the learn levels run against the order
	// the tree walk meets them, so the list order is worth pinning too.
	(*g_pSkillInfoTable)[kRoot].SetSkillStep(SKILL_STEP_APPRENTICE);
	(*g_pSkillInfoTable)[kChild].SetSkillStep(SKILL_STEP_APPRENTICE);
	(*g_pSkillInfoTable)[kLeaf].SetSkillStep(SKILL_STEP_APPRENTICE);
	(*g_pSkillInfoTable)[kRoot].SetLearnLevel(30);
	(*g_pSkillInfoTable)[kChild].SetLearnLevel(20);
	(*g_pSkillInfoTable)[kLeaf].SetLearnLevel(10);

	MSkillDomain domain;

	// The first skill of a step goes into a list that is still empty -
	// the scan has nothing to walk and must answer "not in it".
	domain.SetRootSkill(kRoot);

	const MSkillDomain::SKILL_STEP_LIST* pList = domain.GetSkillStepList(SKILL_STEP_APPRENTICE);
	CHECK(pList != NULL);

	if (pList != NULL)
	{
		CHECK_EQ(3, (int)pList->size());
		CHECK_EQ((int)kLeaf, (int)(*pList)[0]);
		CHECK_EQ((int)kChild, (int)(*pList)[1]);
		CHECK_EQ((int)kRoot, (int)(*pList)[2]);
	}

	// A second walk over the tree that is already there leaves the list
	// exactly as it was.
	domain.SetRootSkill(kRoot);

	pList = domain.GetSkillStepList(SKILL_STEP_APPRENTICE);
	CHECK(pList != NULL);

	if (pList != NULL)
	{
		CHECK_EQ(3, (int)pList->size());
		CHECK_EQ((int)kLeaf, (int)(*pList)[0]);
		CHECK_EQ((int)kChild, (int)(*pList)[1]);
		CHECK_EQ((int)kRoot, (int)(*pList)[2]);
	}

	// A cleared domain starts its lists empty again, so the same walk
	// fills exactly the same list.
	domain.Clear();
	domain.SetRootSkill(kRoot);

	pList = domain.GetSkillStepList(SKILL_STEP_APPRENTICE);
	CHECK(pList != NULL);

	if (pList != NULL)
	{
		CHECK_EQ(3, (int)pList->size());
		CHECK_EQ((int)kLeaf, (int)(*pList)[0]);
	}
}

//----------------------------------------------------------------------
// UseEnglishTextFrom - the language file's comment lines and its
// LANGUAGE keyword, which is a PREFIX test and not a whole-word one
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers2, LanguageFileKeywordIsMatchedAsAPrefix)
{
	// Nothing to read at all: the built-in English text stands.
	CHECK(UseEnglishTextFrom(NULL));
	CHECK(UseEnglishTextFrom(kMissingFile));

	// A file with nothing in it, a line that is nothing but the comment
	// character, and an empty line.
	WriteLanguageFile("");
	CHECK(UseEnglishTextFrom(kLanguageFile));
	WriteLanguageFile(";\n");
	CHECK(UseEnglishTextFrom(kLanguageFile));
	WriteLanguageFile("\n");
	CHECK(UseEnglishTextFrom(kLanguageFile));

	// The keyword selects a language.
	WriteLanguageFile("LANGUAGE 0\n");
	CHECK(!UseEnglishTextFrom(kLanguageFile));
	WriteLanguageFile("LANGUAGE 3\n");
	CHECK(UseEnglishTextFrom(kLanguageFile));

	// A commented-out keyword is not one: the comment test runs first.
	WriteLanguageFile(";LANGUAGE 0\n");
	CHECK(UseEnglishTextFrom(kLanguageFile));

	// One character short of the keyword, with nothing after it. The
	// test may not read past the end of the line to decide that.
	WriteLanguageFile("LANGUAG");
	CHECK(UseEnglishTextFrom(kLanguageFile));

	// The short line must not swallow the real one that follows it.
	WriteLanguageFile("LANGUAG\nLANGUAGE 0\n");
	CHECK(!UseEnglishTextFrom(kLanguageFile));

	// Exactly the keyword and nothing else: it matches, and the number
	// that is not there leaves English in place.
	WriteLanguageFile("LANGUAGE");
	CHECK(UseEnglishTextFrom(kLanguageFile));

	// A prefix match with no separator is still a match, and the digit
	// right behind the keyword is still read - this is the case that
	// tells a prefix test from a whole-word one.
	WriteLanguageFile("LANGUAGE0\n");
	CHECK(!UseEnglishTextFrom(kLanguageFile));

	std::remove(kLanguageFile);
}

//----------------------------------------------------------------------
// SystemAvailabilitiesManager::LoadFromStream - the four line kinds the
// loader branches on, with the comment and empty lines that must not be
// counted as rows of the block they sit in
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers2, AvailabilityScriptRecognisesEveryLineKind)
{
	const char* const kScript =
		"; a comment before anything\r\n"
		";\r\n"					// nothing but the comment character
		"\r\n"					// an empty line
		"Z0\r\n"
		"100\r\n"
		"200\r\n"
		"99999\r\n"
		"*4 2\r\n"
		"10 2\r\n"
		"\r\n"					// an empty line inside the script block
		"; and a comment inside it\r\n"
		"11 2\r\n"
		"S0 1\r\n"
		"20 3 999\r\n";

	SystemAvailabilitiesManager m;
	CHECK(LoadScript(m, kScript));

	// The '*' block: both of its rows are in force, which they only are
	// if the empty line and the comment line were skipped rather than
	// counted - either would have closed the block one row early.
	m.SetFlag(~(1u << 4));
	CHECK(!m.ScriptFiltering(10, 1));
	CHECK(!m.ScriptFiltering(11, 1));
	CHECK(m.ScriptFiltering(12, 1));
	CHECK(m.ScriptFiltering(10, 0));

	// The 'Z' block: the two zones it named, and one it did not.
	m.SetOpenDegree(0);
	CHECK(m.ZoneFiltering(100));
	CHECK(m.ZoneFiltering(200));
	CHECK(!m.ZoneFiltering(300));

	// The 'S' block: its row names zone 999, which degree 0 does not
	// open, so that script is refused and its neighbours are not.
	CHECK(!m.ScriptFiltering(20, 2));
	CHECK(m.ScriptFiltering(20, 1));
	CHECK(m.ScriptFiltering(21, 2));
}

//----------------------------------------------------------------------
// SystemAvailabilitiesManager::ZoneFiltering - the scan over a degree's
// zone list, over an empty list and over the wildcard row
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers2, ZoneFilterAnswersFromTheDegreeList)
{
	const char* const kScript =
		"Z0\n"
		"99999\n"				// degree 0 names no zone at all
		"Z1\n"
		"90909\n"				// the row that opens every zone
		"99999\n"
		"Z2\n"
		"100\n"
		"99999\n";

	SystemAvailabilitiesManager m;
	CHECK(LoadScript(m, kScript));

	// The default degree is past every list, so nothing is consulted.
	CHECK(m.ZoneFiltering(100));

	// Degree 0's list is empty: the scan has nothing to walk.
	m.SetOpenDegree(0);
	CHECK(!m.ZoneFiltering(100));
	CHECK(!m.ZoneFiltering(90909));

	// Degree 1 adds the wildcard row, which opens a zone no list names.
	m.SetOpenDegree(1);
	CHECK(m.ZoneFiltering(100));
	CHECK(m.ZoneFiltering(12345));

	// Degree 2 adds a list with one ordinary zone in it; the walk goes
	// down to degree 0, so the wildcard is still reachable.
	m.SetOpenDegree(2);
	CHECK(m.ZoneFiltering(100));
	CHECK(m.ZoneFiltering(12345));
}

//----------------------------------------------------------------------
// SystemAvailabilitiesManager::CheckScript, through ScriptFiltering -
// the scan over one system's filter rows, which has to match on BOTH
// the script and the answer
//----------------------------------------------------------------------
TEST(Cpp20ContainerHelpers2, ScriptFilterMatchesOnBothIDsOrNotAtAll)
{
	const char* const kScript =
		"*4 2\n"
		"10 2\n"					// script 10, answer 2 (stored as 1)
		"20 4\n";					// script 20, answer 4 (stored as 3)

	SystemAvailabilitiesManager m;
	CHECK(LoadScript(m, kScript));

	// While system 4 is open the rows are never consulted.
	CHECK(m.ScriptFiltering(10, 1));

	m.SetFlag(~(1u << 4));

	// Both ids match: refused.
	CHECK(!m.ScriptFiltering(10, 1));
	CHECK(!m.ScriptFiltering(20, 3));

	// The right script with the wrong answer, and the right answer with
	// the wrong script, are both allowed - a scan that matched on one
	// id would refuse them.
	CHECK(m.ScriptFiltering(10, 3));
	CHECK(m.ScriptFiltering(20, 1));

	// Neither id appears at all.
	CHECK(m.ScriptFiltering(30, 1));

	// A system whose filter list is empty refuses nothing.
	m.SetFlag(0);
	CHECK(!m.ScriptFiltering(10, 1));
	CHECK(m.ScriptFiltering(30, 1));
}

