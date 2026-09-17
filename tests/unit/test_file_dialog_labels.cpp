#include "test_framework.h"
#include "FileDialogListing.h"

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
