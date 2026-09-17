//----------------------------------------------------------------------
// UserInformation.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "UserInformation.h"
//#include <fstream.h>

//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
UserInformation	*	g_pUserInformation  = NULL;

//----------------------------------------------------------------------
// 
// constructor
//
//----------------------------------------------------------------------
UserInformation::UserInformation()
{
	Slot		= 0;
//	Invisible	= false;
	
	PCSNumber	= 0;

	// Looped rather than unrolled, so that raising MAX_PCS_SLOT cannot
	// leave the new entries uninitialised while the handler guards
	// already admit them. The index pass that added those guards said
	// "the arrays and the guards cannot disagree" and left these three
	// literals behind, which is only half of that.
	for (int i = 0; i < MAX_PCS_SLOT; i++)
	{
		OtherPCSNumber[i] = 0;
	}

	FaceStyleSlot[0] = 0;
	FaceStyleSlot[1] = 0;
	FaceStyleSlot[2] = 0;

	HairColor = 0;
	SkinColor = 0;

	FaceStyle = 0;

	GoreLevel = true;

	KeepConnection = FALSE;
	IsMaster = FALSE;
	ItemDropEnableTime = MonotonicClock::TimePoint();
	HasSkillRestore = false;
	HasMagicGroundAttack = false;
	HasMagicHallu = false;
	HasMagicBloodyWarp = false;
	HasMagicBloodySnake = false;

	// the epoch: no logout scheduled
	LogoutTime = MonotonicClock::TimePoint();

	GameVersion = 0;

	// 넷마블용
	IsNetmarble = false;
	WorldID = 0;
	ServerID = 0;
	
	bMetrotech = false;

	WarInfo.clear();

	bChinese = false;
	bKorean = true;
	bJapanese = false;
	bEnglish = false;
	
	// 머리가격 기본값 100%다.
	HeadPrice = 100;
	bCompetence = false;
	bCompetenceShape = false;
	
	IsAutoLogIn = false;
	pLogInClientPlayer = NULL;
	IsNonPK = false;
	dwUnionID = 0;
	bUnionGrade = 0;
	IsTestServer = false;
}

UserInformation::~UserInformation()
{
}

//----------------------------------------------------------------------
// SecondsToLogout
//----------------------------------------------------------------------
DWORD
UserInformation::SecondsToLogout(MonotonicClock::TimePoint now) const
{
	if (!IsLogoutScheduled() || LogoutTime <= now)
	{
		return 0;
	}

	return (DWORD)((LogoutTime - now).count() / 1000);
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

