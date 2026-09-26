//----------------------------------------------------------------------
// test_pack_paths.cpp
//----------------------------------------------------------------------
//
// The SpriteLib pack loaders that take a file name - CTypePack and
// CTypePack2 (LoadFromFile, LoadFromFileRunning, LoadFromFileData),
// CAlphaSpritePack::LoadFromFileSprite and spritectl_load_pack - are
// handed the game's spelling of a path: backslashes, and a letter case
// that need not match the disk (FileDef.inf, VS_UI_filepath.h). They
// open it through Basic::NormalizeDataPath (basic/DataPath.h), so the
// same spelling opens on a case-sensitive filesystem and in the browser,
// where a backslash is an ordinary character in a file name. On Windows
// NormalizeDataPath is the identity and the filesystem forgives both,
// so these pass there as they always did; the Linux and macOS jobs are
// the ones that exercise the resolution.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "CTypePack.h"
#include "CAlphaSpritePack.h"
#include "SpriteLibBackend.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct PackDirectory {
	std::filesystem::path path;

	PackDirectory()
	{
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		path = std::filesystem::temp_directory_path()
			/ ("darkeden_pack_paths_" + std::to_string(stamp));
		std::filesystem::create_directories(path / "Data" / "Image");
	}

	~PackDirectory()
	{
		std::error_code error;
		std::filesystem::remove_all(path, error);
	}

	void Write(const char* name, const std::vector<unsigned char>& bytes) const
	{
		std::ofstream out(path / "Data" / "Image" / name, std::ios::binary | std::ios::trunc);
		out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}

	// The file as the game would name it: upper case, backslashes.
	std::string Requested(const char* name) const
	{
		std::string upper(name);
		std::transform(upper.begin(), upper.end(), upper.begin(),
			[](char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c; });
		std::string requested = (path / "DATA" / "IMAGE" / upper).generic_string();
		std::replace(requested.begin(), requested.end(), '/', '\\');
		return requested;
	}
};

class Element {
public:
	bool IsInit() const { return value != 0; }
	void Release() { value = 0; }
	bool SaveToFile(std::ofstream&) { return true; }
	bool LoadFromFile(std::ifstream& input) {
		char byte = 0;
		if (!input.get(byte) || byte == 0) return false;
		value = static_cast<unsigned char>(byte);
		return true;
	}
	int value = 0;
};

template<class Pack> void CheckWindowsSpelling()
{
	PackDirectory directory;
	directory.Write("Pack.spk", {1, 0, 42});
	directory.Write("Pack.spki", {1, 0, 2, 0, 0, 0});
	const std::string data = directory.Requested("Pack.spk");
	const std::string index = directory.Requested("Pack.spki");

	{
		Pack pack;
		CHECK(pack.LoadFromFile(data.c_str()));
		CHECK_EQ(1, static_cast<int>(pack.GetSize()));
		if (pack.GetSize() == 1) CHECK_EQ(42, pack[0].value);
	}
	{
		Pack pack;
		pack.Init(1);
		CHECK(pack.LoadFromFileData(0, 0, data.c_str(), index.c_str()));
		CHECK_EQ(42, pack[0].value);
	}
	{
		// A running load appends 'i' to the requested name for its index.
		Pack pack;
		CHECK(pack.LoadFromFileRunning(data.c_str()));
		CHECK_EQ(1, static_cast<int>(pack.GetSize()));
		if (pack.GetSize() == 1) CHECK_EQ(42, pack.Get(0).value);
	}
}

void AppendWords(std::vector<unsigned char>& bytes, const std::vector<WORD>& words)
{
	for (WORD word : words) {
		bytes.push_back(static_cast<unsigned char>(word));
		bytes.push_back(static_cast<unsigned char>(word >> 8));
	}
}

} // namespace

TEST(PackPaths, TypePacksOpenTheWindowsSpellingOfAPath)
{
	CheckWindowsSpelling<CTypePack<Element>>();
	CheckWindowsSpelling<CTypePack2<Element, Element, Element>>();
}

TEST(PackPaths, AlphaSpritePackOpensTheWindowsSpellingOfAPath)
{
	PackDirectory directory;
	// One 1x1 sprite (the fixture of test_alpha_sprite_pack.cpp) behind a
	// sprite count, and an index whose one entry points just past it.
	std::vector<unsigned char> data;
	AppendWords(data, {1, 1, 1, 5, 1, 0, 1, 0x1010, 0x07e0});
	std::vector<unsigned char> index;
	AppendWords(index, {1, 2, 0});
	directory.Write("Mark.aspk", data);
	directory.Write("Mark.aspki", index);

	CAlphaSpritePack pack;
	pack.Init(1, true);
	CHECK(pack.LoadFromFileSprite(0, 0, directory.Requested("Mark.aspk").c_str(),
		directory.Requested("Mark.aspki").c_str()));
	CHECK(pack[0].IsInit());
	if (pack[0].IsInit()) CHECK_EQ(0x07e0, pack[0].GetPixelLine(0)[4]);
}

TEST(PackPaths, BackendPackLoaderOpensTheWindowsSpellingOfAPath)
{
	PackDirectory directory;
	directory.Write("Empty.spk", {0, 0});

	spritectl_pack_t pack = nullptr;
	CHECK_EQ(0, spritectl_load_pack(directory.Requested("Empty.spk").c_str(), &pack));
	CHECK(pack != nullptr);
	spritectl_free_pack(pack);
}
