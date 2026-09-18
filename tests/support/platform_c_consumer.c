#include "Platform.h"

unsigned int platform_c_type_contract(void)
{
	return (sizeof(DWORD) == 4 ? 1u : 0u) |
		(sizeof(LONG) == 4 ? 2u : 0u) |
		(sizeof(LPARAM) == sizeof(void*) ? 4u : 0u);
}

uint64_t platform_c_performance_frequency(void)
{
	return platform_get_performance_frequency();
}

char platform_c_path_separator(void)
{
	return platform_get_path_separator();
}
