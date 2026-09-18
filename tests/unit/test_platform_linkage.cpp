// Include the public header first, without a C++ library header masking its
// dependencies. The companion source is compiled as C and calls the same API.
#include "Platform.h"
#include "test_framework.h"

extern "C" unsigned int platform_c_type_contract(void);
extern "C" uint64_t platform_c_performance_frequency(void);
extern "C" char platform_c_path_separator(void);

TEST(PlatformLinkage, CAndCppConsumersShareTypesAndExportedFunctions)
{
	CHECK_EQ(7, platform_c_type_contract());
	CHECK(platform_c_performance_frequency() > 0);
	CHECK_EQ(platform_get_performance_frequency(), platform_c_performance_frequency());
	CHECK_EQ(platform_get_path_separator(), platform_c_path_separator());
}
