#include "test_framework.h"
#include "MasterCommands.h"
#include "TextEncoding.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>

namespace {
struct CommandFiles {
	std::map<unsigned, std::string> files;
	bool Read(unsigned number, std::string& text) const {
		const auto found = files.find(number);
		if (found == files.end()) return false;
		text = found->second;
		return true;
	}
	bool Expand(unsigned number, std::vector<std::string>& out, MasterCommands::Limits limits = {}) const {
		return MasterCommands::Expand(number, [&](unsigned n, std::string& text) { return Read(n, text); }, out, limits);
	}
};
struct EncodingScope {
	TextEncoding::Encoding previous = TextEncoding::GetResourceEncoding();
	~EncodingScope() { TextEncoding::SetResourceEncoding(previous); }
};
}

TEST(MasterCommands, RecognizesOnlyTheCompleteSingleDigitInvocation)
{
	unsigned number = 42;
	for (const std::string text : {"*mc 0", " *mc 9 ", "*mc\t2"}) CHECK(MasterCommands::Invocation(text, number));
	CHECK_EQ(2u, number);
	for (const std::string text : {"*mc", "*mc1", "*mcx1", "*mc 10", "*mc -1", "*mc 1 junk", "*mc  1", "*mcc 1"}) {
		CHECK(!MasterCommands::Invocation(text, number));
		CHECK_EQ(2u, number);
	}
}

TEST(MasterCommands, ExpandsNestedAndRepeatedFilesInOrderWithoutDispatching)
{
	CommandFiles files{{{0, " first \r\n*mc 1\nlast\r*mc 1"}, {1, "\nsecond\n*mc 2\n"}, {2, "third"}}};
	std::vector<std::string> result;
	CHECK(files.Expand(0, result));
	CHECK((result == std::vector<std::string>{"first", "second", "third", "last", "second", "third"}));
	files.files.clear();
	CHECK(result[1] == "second");
}

TEST(MasterCommands, CyclesMissingFilesAndInvalidIndicesPreservePreviousOutput)
{
	std::vector<std::string> result{"kept"};
	CommandFiles files{{{0, "first\n*mc 0"}}};
	CHECK(!files.Expand(0, result));
	files.files = {{0, "first\n *mc 1 "}, {1, "*mc 2"}, {2, "*mc 0"}};
	CHECK(!files.Expand(0, result));
	files.files.erase(2);
	CHECK(!files.Expand(0, result));
	CHECK(!files.Expand(10, result));
	CHECK(!MasterCommands::Expand(0, {}, result));
	CHECK((result == std::vector<std::string>{"kept"}));
}

TEST(MasterCommands, LimitsRejectWholePlansRatherThanSplittingOrTruncatingCommands)
{
	std::vector<std::string> result{"kept"};
	CommandFiles files{{{0, std::string(128, 'x')}}};
	CHECK(files.Expand(0, result));
	CHECK_EQ(size_t{128}, result.front().size());
	for (size_t length : {size_t{129}, size_t{512}, size_t{1200}}) {
		files.files[0].assign(length, 'x');
		CHECK(!files.Expand(0, result));
		CHECK_EQ(size_t{128}, result.front().size());
	}
	MasterCommands::Limits limits;
	files.files = {{0, "*mc 1\n*mc 1\n*mc 1"}, {1, ""}};
	limits.maxCommands = 2;
	CHECK(!files.Expand(0, result, limits));
	limits = {};
	limits.maxDepth = 1;
	CHECK(!files.Expand(0, result, limits));
	limits.maxDepth = 2;
	CHECK(files.Expand(0, result, limits));
	CHECK(result.empty());
	files.files = {{0, "abcd"}};
	limits = {};
	limits.maxFileBytes = 3;
	CHECK(!files.Expand(0, result, limits));
	limits.maxFileBytes = 4;
	limits.maxTotalBytes = 3;
	CHECK(!files.Expand(0, result, limits));
	limits.maxTotalBytes = 4;
	CHECK(files.Expand(0, result, limits));
	files.files = {{0, "*mc 1\n*mc 1"}, {1, "abc"}};
	limits = {};
	limits.maxTotalBytes = files.files[0].size() + 5;
	CHECK(!files.Expand(0, result, limits));
	files.files = {{0, std::string("abc\0def", 7)}};
	CHECK(!files.Expand(0, result));
}

TEST(MasterCommands, DecodesTheDeclaredPageAndRejectsDamagedCommandBytes)
{
	EncodingScope scope;
	CHECK(TextEncoding::SetResourceEncoding(TextEncoding::Encoding::Cp949));
	CommandFiles files{{{0, "\xC7\xD1\n*mc 1"}, {1, "\xEF\xBB\xBF\xEA\xB0\x80\\ntext"}}};
	std::vector<std::string> result;
	CHECK(files.Expand(0, result));
	CHECK((result == std::vector<std::string>{"\xED\x95\x9C", "\xEA\xB0\x80\\ntext"}));
	const auto previousPlan = result;
	files.files[1] = "\xEF\xBB\xBF\xFF\nx";
	CHECK(!files.Expand(0, result));
	CHECK(result == previousPlan);
	std::string legacy;
	for (int i = 0; i < 42; ++i) legacy += "\xC7\xD1";
	files.files = {{0, legacy}};
	CHECK(files.Expand(0, result));
	CHECK_EQ(size_t{126}, result.front().size());
	MasterCommands::Limits limits;
	limits.maxTotalBytes = 125;
	CHECK(!files.Expand(0, result, limits));
	files.files[0] += "\xC7\xD1";
	CHECK(!files.Expand(0, result));
	CHECK_EQ(size_t{126}, result.front().size());
}

TEST(MasterCommands, LoadsCompleteFilesAndReleasesTheirHandlesOnSuccessAndFailure)
{
	std::filesystem::path directory;
	for (int i = 0; i < 1000; ++i) {
		auto candidate = std::filesystem::temp_directory_path() / ("darkeden-master-commands-" + std::to_string(i));
		if (std::filesystem::create_directory(candidate)) { directory = candidate; break; }
	}
	if (directory.empty()) throw std::runtime_error("cannot create command fixture directory");
	const auto file0 = directory / "MasterCommand0.txt";
	const auto file1 = directory / "MasterCommand1.txt";
	struct Cleanup {
		std::filesystem::path directory, file0, file1;
		~Cleanup() { std::filesystem::remove(file0); std::filesystem::remove(file1); std::filesystem::remove(directory); }
	} cleanup{directory, file0, file1};
	{ std::ofstream out(file0, std::ios::binary); out << "\xEF\xBB\xBF" "one\n*mc 1"; }
	std::vector<std::string> result{"kept"};
	CHECK(!MasterCommands::Load(0, directory.string(), result));
	CHECK(result.front() == "kept");
	{ std::ofstream out(file1, std::ios::binary); out << "two\r\nthree"; }
	CHECK(MasterCommands::Load(0, directory.string(), result));
	CHECK((result == std::vector<std::string>{"one", "two", "three"}));
	MasterCommands::Limits limits;
	limits.maxFileBytes = 2;
	CHECK(!MasterCommands::Load(0, directory.string(), result, limits));
	CHECK(std::filesystem::remove(file0));
	CHECK(std::filesystem::remove(file1));
}
