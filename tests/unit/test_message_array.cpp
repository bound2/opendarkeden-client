#include "test_framework.h"
#include "CMessageArray.h"
#include <string>

TEST(MessageArray, RealRingLinksFromBasicAndRetainsNewestRowsInOrder)
{
	CMessageArray messages;
	messages.Init(3, 12);
	messages.Add("first");
	messages.AddFormat("row %d", 2);
	messages.AddSafeFormat("row %d", 3);
	messages.Add("last");
	CHECK_EQ(3, messages.GetSize());
	CHECK(std::string(messages[0]) == "row 2");
	CHECK(std::string(messages[1]) == "row 3");
	CHECK(std::string(messages[2]) == "last");
}
