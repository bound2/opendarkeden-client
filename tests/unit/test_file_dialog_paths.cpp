#include "test_framework.h"
#include "FileDialogListing.h"

TEST(FileDialogPaths, FilterDelimitersKeepEmptySuffixes)
{
	CHECK(Basic::SplitDialogFilters(".bmp;.jpg") == std::vector<std::string>({".bmp", ".jpg"}));
	CHECK(Basic::SplitDialogFilters("") == std::vector<std::string>({""}));
	CHECK(Basic::SplitDialogFilters(";.bmp;") == std::vector<std::string>({"", ".bmp", ""}));
}

TEST(FileDialogPaths, NormalizedDriveAndChildPaths)
{
	char path[MAX_PATH] = "C:\\";
	Basic::NormalizeDialogSearchPath(path);
	CHECK(std::string(path) == "C:\\*.*");
	Basic::ChangeDialogSearchPath("\\pictures", path);
	Basic::NormalizeDialogSearchPath(path);
	CHECK(std::string(path) == "C:\\pictures\\*.*");
	Basic::ChangeDialogSearchPath("\\..", path);
	Basic::NormalizeDialogSearchPath(path);
	CHECK(std::string(path) == "C:\\*.*");
}
