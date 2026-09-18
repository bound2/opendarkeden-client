#include "test_framework.h"
#include "FileDialogListing.h"

TEST(FileDialogLabels, ShortPathsDoNotEraseBeforeTheBeginning)
{
	CHECK(Basic::BuildDialogPathLabel("", {}).empty());
	CHECK(Basic::BuildDialogPathLabel("x", {}) == "x");
	CHECK(Basic::BuildDialogPathLabel("x", {".png"}) == "x.png");
	CHECK(Basic::BuildDialogPathLabel("*.*", {}).empty());
}

TEST(FileDialogLabels, LongPathsAndFiltersAreShortenedAfterSafeComposition)
{
	const std::string path = "C:\\" + std::string(250, 'p') + "\\*.*";
	const std::vector<std::string> filters(100, ".long-extension");
	const std::string label = Basic::BuildDialogPathLabel(path, filters);
	CHECK(label.size() > 300);
	CHECK(Basic::ShortenDialogLabel(label) == label.substr(0, 35) + "...");
}

TEST(FileDialogLabels, LongFilenamesAndEmptyLabelsRemainOwned)
{
	const std::string filename(6000, 'n');
	CHECK(Basic::ShortenDialogLabel(filename) == std::string(35, 'n') + "...");
	CHECK(filename == std::string(6000, 'n'));
	CHECK(Basic::ShortenDialogLabel("").empty());
}

TEST(FileDialogLabels, ExistingSearchPatternsAndEmptyFilters)
{
	CHECK(Basic::BuildDialogPathLabel("C:\\*.*", {".bmp", ".jpg"}) == "C:\\*.bmp;.jpg");
	CHECK(Basic::BuildDialogPathLabel("C:\\Pictures\\*.*", {""}) == "C:\\Pictures\\*");
	CHECK(Basic::BuildDialogPathLabel("C:\\Pictures\\*.*", {}) == "C:\\Pictures\\");
}

TEST(FileDialogLabels, DisplayLimitAndOriginalLabelArePreserved)
{
	const std::string label(100, 'x');
	CHECK(Basic::ShortenDialogLabel(label) == std::string(35, 'x') + "...");
	CHECK(label == std::string(100, 'x'));
	CHECK(Basic::ShortenDialogLabel("\\picture.bmp") == "\\picture.bmp");
	CHECK(Basic::ShortenDialogLabel(std::string(38, 'x')) == std::string(38, 'x'));
}
