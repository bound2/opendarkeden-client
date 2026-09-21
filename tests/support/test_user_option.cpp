#include "test_framework.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <cstring>
#include <new>
#include "UserOption.h"
#include "KeyAccelerator.h"

namespace {
struct SettingsFixture {
    std::filesystem::path path = "user-option-test.tmp";
    KeyAccelerator keys;
    SettingsFixture() {
        keys.Init(4);
        keys.SetAcceleratorKey(1, DIK_I);
        g_pKeyAccelerator = &keys;
    }
    ~SettingsFixture() {
        g_pKeyAccelerator = nullptr;
        std::filesystem::remove(path);
    }
    std::string Read() {
        std::ifstream file(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }
    void Write(const std::string& contents) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(contents.data(), contents.size());
    }
};
}

TEST(UserOption, EntireBackupIdIsInitialized)
{
    alignas(UserOption) unsigned char storage[sizeof(UserOption)];
    std::memset(storage, 0x1A, sizeof storage);
    auto* options = new (storage) UserOption;
    for (char byte : options->BackupID) CHECK_EQ(0, byte);
    options->~UserOption();
}

TEST(UserOption, LegacyIdFieldDoesNotSerializeAdjacentMembers)
{
    SettingsFixture fixture;
    UserOption options;
    std::memcpy(options.BackupID, "abcdefghij", sizeof options.BackupID);
    options.UseEnterChat = 0x1A;
    options.UseXbrz = FALSE;
    options.SaveToFile(fixture.path.string().c_str());
    const auto bytes = fixture.Read();
    const auto id = bytes.find("abcdefghij");
    CHECK(id != std::string::npos);
    if (id == std::string::npos) return;
    CHECK(std::string("abcdefghij") + std::string(5, '\0') == bytes.substr(id, 15));
    UserOption loaded;
    CHECK(loaded.LoadFromFile(fixture.path.string().c_str()));
    CHECK(std::string("abcdefghij") == loaded.BackupID);
    CHECK_EQ(options.UseEnterChat, loaded.UseEnterChat);
    CHECK_EQ(FALSE, loaded.UseXbrz);
}

TEST(UserOption, LegacyPaddingAndCrLfRemainReadable)
{
    SettingsFixture fixture;
    UserOption options;
    std::memcpy(options.BackupID, "abcdefghij", sizeof options.BackupID);
    options.UseEnterChat = TRUE;
    options.UseXbrz = FALSE;
    options.SaveToFile(fixture.path.string().c_str());
    auto bytes = fixture.Read();
    const auto id = bytes.find("abcdefghij");
    CHECK(id != std::string::npos);
    if (id == std::string::npos) return;
    // Ignore the old writer's trailing padding/adjacent-member bytes.
    bytes.replace(id + 11, 4, std::string(4, '\x7F'));
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (bytes[i] == '\n' && (i == 0 || bytes[i - 1] != '\r')) {
            bytes.insert(i, 1, '\r');
            ++i;
        }
    }
    fixture.Write(bytes);
    UserOption loaded;
    CHECK(loaded.LoadFromFile(fixture.path.string().c_str()));
    CHECK(std::string("abcdefghij") == loaded.BackupID);
    CHECK_EQ(TRUE, loaded.UseEnterChat);
    CHECK_EQ(FALSE, loaded.UseXbrz);
}

TEST(UserOption, DiskIdIsBoundedAndTruncatedFieldIsRejected)
{
    SettingsFixture fixture;
    UserOption options;
    std::memcpy(options.BackupID, "abcdefghij", sizeof options.BackupID);
    options.UseEnterChat = TRUE;
    options.SaveToFile(fixture.path.string().c_str());
    auto bytes = fixture.Read();
    const auto id = bytes.find("abcdefghij");
    CHECK(id != std::string::npos);
    if (id == std::string::npos) return;
    bytes.replace(id, 15, "abcdefghijklmno");
    fixture.Write(bytes);
    UserOption loaded;
    CHECK(loaded.LoadFromFile(fixture.path.string().c_str()));
    CHECK(std::string("abcdefghij") == loaded.BackupID);
    CHECK_EQ(TRUE, loaded.UseEnterChat);
    bytes.resize(id + 14);
    fixture.Write(bytes);
    CHECK(!loaded.LoadFromFile(fixture.path.string().c_str()));
}

TEST(UserOption, XbrzDefaultsOnAndBothChoicesSurviveReload)
{
    SettingsFixture fixture;
    UserOption options;
    CHECK_EQ(TRUE, options.UseXbrz);
    options.DrawFPS = TRUE;
    options.VolumeMusic = 7;
    for (int enabled : {0, 1}) {
        options.UseXbrz = enabled;
        options.SaveToFile(fixture.path.string().c_str());
        UserOption loaded;
        CHECK(loaded.LoadFromFile(fixture.path.string().c_str()));
        CHECK_EQ(enabled, loaded.UseXbrz);
        CHECK_EQ(TRUE, loaded.DrawFPS);
        CHECK_EQ(7, loaded.VolumeMusic);
        CHECK_EQ(DIK_I, fixture.keys.GetKey(1));
    }
}

TEST(UserOption, OldMissingAndInvalidSettingsDefaultXbrzOn)
{
    SettingsFixture fixture;
    UserOption options;
    options.UseXbrz = FALSE;
    options.SaveToFile(fixture.path.string().c_str());
    auto legacy = fixture.Read();
    const auto tail = legacy.rfind("0\tUseXbrz");
    CHECK(tail != std::string::npos);
    if (tail == std::string::npos) return;
    legacy.resize(tail);
    for (const char* extension : {"", "0\tUnknownOption\n", "2\tUseXbrz\n", "0"}) {
        fixture.Write(legacy + extension);
        options.UseXbrz = FALSE;
        CHECK(options.LoadFromFile(fixture.path.string().c_str()));
        CHECK_EQ(TRUE, options.UseXbrz);
    }
    std::filesystem::remove(fixture.path);
    options.UseXbrz = FALSE;
    CHECK(!options.LoadFromFile(fixture.path.string().c_str()));
    CHECK_EQ(TRUE, options.UseXbrz);
}
