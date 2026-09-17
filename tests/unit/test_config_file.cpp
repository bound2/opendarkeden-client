#include "test_framework.h"
#include "ConfigFile.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

namespace {
constexpr const char* path = "platform_config_test.bin";
struct Fixture {
	Fixture() { std::remove(path); }
	~Fixture() { std::remove(path); }
};

std::string Read()
{
	std::ifstream in(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(in), {}};
}
void Write(const std::string& text)
{
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out << text;
}
std::string Get(const char* key, const char* name)
{
	char buffer[2048]{};
	DWORD size = sizeof(buffer);
	CHECK_EQ(0, ConfigFile::GetString(path, key, name, buffer, &size));
	CHECK_EQ(std::strlen(buffer) + 1, size);
	return buffer;
}
}

TEST(ConfigFile, RepeatedWritesReturnTheLatestValueAndKeepOneEntry)
{
	Fixture fixture;
	for (const char* value : {"first", "second", "last"}) {
		CHECK_EQ(0, ConfigFile::SetString(path, "ui", "size", value));
		CHECK(Get("ui", "size") == value);
	}
	const auto contents = Read();
	const auto first = contents.find("ui.size=");
	CHECK(first != std::string::npos);
	CHECK(contents.find("ui.size=", first + 1) == std::string::npos);
}

TEST(ConfigFile, ReplacesOldDuplicatesWithoutChangingNeighboringKeysOrComments)
{
	Fixture fixture;
	Write("# settings\r\nui.size=old\r\nui.sizeExtra=kept\r\n\r\nui.size=stale\r\nother.size=end");
	CHECK_EQ(0, ConfigFile::SetString(path, "ui", "size", "new"));
	CHECK(Get("ui", "size") == "new");
	CHECK(Get("ui", "sizeExtra") == "kept");
	CHECK(Get("other", "size") == "end");
	const auto contents = Read();
	CHECK(contents.find("# settings\r\n") == 0);
	CHECK(contents.find("ui.sizeExtra=kept\r\n\r\n") != std::string::npos);
	CHECK(contents.find("stale") == std::string::npos);
	CHECK(contents.find("old") == std::string::npos);
}

TEST(ConfigFile, LongKeysAndValuesRoundTripWithoutFixedLineBuffers)
{
	Fixture fixture;
	const std::string key(600, 'k'), value(1200, 'v');
	CHECK_EQ(0, ConfigFile::SetString(path, key.c_str(), "entry", value.c_str()));
	CHECK(Get(key.c_str(), "entry") == value);
	CHECK_EQ(0, ConfigFile::SetString(path, "other", "entry", "value=with=equals"));
	CHECK(Get("other", "entry") == "value=with=equals");
	CHECK_EQ(0, ConfigFile::SetString(path, key.c_str(), "entry", ""));
	CHECK(Get(key.c_str(), "entry").empty());
}

TEST(ConfigFile, BufferQueriesAndInvalidArgumentsDoNotWriteOutsideTheDestination)
{
	Fixture fixture;
	Write("ui.size=ok\n");
	char small[] = "K";
	DWORD size = sizeof(small);
	CHECK_EQ(1, ConfigFile::GetString(path, "ui", "size", small, &size));
	CHECK_EQ(3, size); CHECK(std::string(small) == "K");
	size = 0;
	CHECK_EQ(1, ConfigFile::GetString(path, "ui", "size", nullptr, &size));
	CHECK_EQ(3, size);
	char exact[3]{}; size = sizeof(exact);
	CHECK_EQ(0, ConfigFile::GetString(path, "ui", "size", exact, &size));
	CHECK(std::string(exact) == "ok"); CHECK_EQ(3, size);
	CHECK_EQ(1, ConfigFile::GetString(path, "ui", "size", exact, nullptr));
	CHECK_EQ(1, ConfigFile::GetString(nullptr, "ui", "size", exact, &size));
	CHECK_EQ(1, ConfigFile::GetString(path, nullptr, "size", exact, &size));
	CHECK_EQ(1, ConfigFile::SetString(path, "ui", "size", nullptr));
	CHECK_EQ(1, ConfigFile::SetString(path, "ui", nullptr, "bad"));
}

TEST(ConfigFile, LineInjectionAndFailedUpdatesPreserveTheExistingFile)
{
	Fixture fixture;
	Write("# kept\nui.size=old\n");
	const auto before = Read();
	CHECK_EQ(1, ConfigFile::SetString(path, "ui\nother", "size", "new"));
	CHECK_EQ(1, ConfigFile::SetString(path, "ui", "size=other", "new"));
	CHECK_EQ(1, ConfigFile::SetString(path, "ui", "size", "new\r\nother.key=injected"));
	const std::string impossible = std::string(path) + "/child";
	CHECK_EQ(1, ConfigFile::SetString(impossible.c_str(), "ui", "size", "new"));
	CHECK(before == Read());
}

TEST(ConfigFile, FileAndOutputLimitsAreCheckedBeforeReplacingSettings)
{
	Fixture fixture;
	constexpr size_t limit = 1024 * 1024;
	const std::string prefix = "ui.size=";
	Write(prefix + std::string(limit - prefix.size() - 1, 'x') + "\n");
	DWORD size = 0;
	CHECK_EQ(1, ConfigFile::GetString(path, "ui", "size", nullptr, &size));
	CHECK_EQ(limit - prefix.size(), size);
	CHECK_EQ(0, ConfigFile::SetString(path, "ui", "size", "small"));
	CHECK(Get("ui", "size") == "small");
	const auto before = Read();
	CHECK_EQ(1, ConfigFile::SetString(path, "ui", "size", std::string(limit, 'x').c_str()));
	CHECK(before == Read());
	const std::string oversized(limit + 1, 'x');
	Write(oversized);
	CHECK_EQ(1, ConfigFile::SetString(path, "ui", "size", "new"));
	CHECK(oversized == Read());
	Write(std::string("ui.size=x\0hidden", 16));
	const auto binary = Read();
	CHECK_EQ(1, ConfigFile::SetString(path, "ui", "size", "new"));
	CHECK(binary == Read());
}

#ifdef PLATFORM_WINDOWS
TEST(ConfigFile, AFailedReplacementPreservesTheOriginalSettings)
{
	Fixture fixture;
	Write("ui.size=old\n");
	const auto before = Read();
	struct RestoreAttributes {
		~RestoreAttributes() { SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL); }
	} restore;
	CHECK(SetFileAttributesA(path, FILE_ATTRIBUTE_READONLY) != 0);
	CHECK_EQ(1, ConfigFile::SetString(path, "ui", "size", "new"));
	CHECK(before == Read());
}
#endif
