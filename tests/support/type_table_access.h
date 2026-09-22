#pragma once

#include "CTypeTable.h"
#include <stdexcept>

namespace testfw {
// Test setup needs writable, allocated entries. A missing row fails that setup
// instead of quietly seeding a shared fallback or dereferencing a null pointer.
template <class Type, class... Indices>
decltype(auto) MutableRow(CTypeTable<Type>& table, int index, Indices... indices)
{
	Type* entry = table.GetMutable(index);
	if (entry == nullptr)
		throw std::out_of_range("test setup requested an unallocated table row");
	if constexpr (sizeof...(indices) == 0)
		return *entry;
	else
		return MutableRow(*entry, indices...);
}
}
