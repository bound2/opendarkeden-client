#include "test_framework.h"
#include "ZoneFileHeader.h"
#include "MImageObject.h"
#include "MShadowObject.h"
#include "MAnimationObject.h"
#include "MShadowAnimationObject.h"
#include "MInteractionObject.h"
#include <cstdio>
#include <cstring>
#include <memory>

TEST(ZoneMapRecords, RealRecordClassesLinkWithoutGameGlobals)
{
	MImageObject image;
	MShadowObject shadow;
	MAnimationObject animation;
	MShadowAnimationObject shadowAnimation;
	MInteractionObject interaction;
	CHECK_EQ(MObject::TYPE_IMAGEOBJECT, image.GetObjectType());
	CHECK_EQ(MObject::TYPE_SHADOWOBJECT, shadow.GetObjectType());
	CHECK_EQ(MObject::TYPE_ANIMATIONOBJECT, animation.GetObjectType());
	CHECK_EQ(MObject::TYPE_SHADOWANIMATIONOBJECT, shadowAnimation.GetObjectType());
	CHECK_EQ(MObject::TYPE_INTERACTIONOBJECT, interaction.GetObjectType());

	const char* path = "zone_map_records_test.bin";
	FILEINFO_ZONE_HEADER header;
	header.ZoneID = 42;
	header.ZoneGroupID = 7;
	header.ZoneName = "zone";
	header.ZoneType = 1;
	header.ZoneLevel = 2;
	header.Description = "description";
	{
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		header.SaveToFile(out);
		CHECK(bool(out));
	}
	{
		std::ifstream in(path, std::ios::binary);
		FILEINFO_ZONE_HEADER loaded;
		loaded.LoadFromFile(in);
		CHECK(bool(in));
		CHECK_EQ(42, loaded.ZoneID);
		CHECK_EQ(7, loaded.ZoneGroupID);
		CHECK(loaded.ZoneName == "zone");
		CHECK(loaded.Description == "description");
	}
	std::remove(path);
}

TEST(ZoneMapRecords, TheImageBaseDestroysTheOwnedDerivedRecord)
{
	bool destroyed = false;
	struct Derived : MImageObject {
		bool& destroyed;
		explicit Derived(bool& flag) : destroyed(flag) {}
		~Derived() { destroyed = true; }
	};
	{
		std::unique_ptr<MImageObject> record = std::make_unique<Derived>(destroyed);
	}
	CHECK(destroyed);
}
