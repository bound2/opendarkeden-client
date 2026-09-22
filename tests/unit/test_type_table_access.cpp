#include "test_framework.h"
#include "Platform.h"
#include "CTypeTable.h"
#include "MString.h"

#include <type_traits>
#include <string>
#include <utility>

namespace {
template <class Table>
void CheckInvalidWriteCannotPoisonAnotherTable()
{
	Table first;
	Table second;
	using IndexResult = decltype(first[-1]);
	using GetResult = decltype(first.Get(-1));
	// Exercise the original failure when an accessor permits assignment. Once
	// reads are immutable, the type contract below keeps this path unavailable.
	if constexpr (std::is_assignable_v<IndexResult, int>) {
		first[-1] = 17;
		CHECK_EQ(0, second[-2]);
		first[-1] = 0;
	}
	if constexpr (std::is_assignable_v<GetResult, int>) {
		first.Get(-1) = 23;
		CHECK_EQ(0, second.Get(-2));
		first.Get(-1) = 0;
	}
	CHECK_EQ(0, first[-1]);
	CHECK_EQ(0, second[100]);
	CHECK_EQ(0, first.Get(100));
	CHECK_EQ(0, std::as_const(second)[-1]);
}
}

TEST(TypeTableAccess, ReadsCannotExposeWritableFallbacks)
{
	using Table = CTypeTable<int>;
	using IndexResult = decltype(std::declval<Table&>()[0]);
	using GetResult = decltype(std::declval<Table&>().Get(0));
	CHECK((!std::is_assignable_v<IndexResult, int>));
	CHECK((!std::is_assignable_v<GetResult, int>));
}

TEST(TypeTableAccess, BadIndicesCannotChangeAnotherTablesDefault)
{
	CheckInvalidWriteCannotPoisonAnotherTable<CTypeTable<int>>();
}

TEST(TypeTableAccess, ConstTablesCannotExposeMutableInternalStorage)
{
	using Table = CTypeTable<int>;
	using Storage = decltype(std::declval<const Table&>().GetInternalPointer());
	CHECK((!std::is_convertible_v<Storage, int*>));
}

TEST(TypeTableAccess, MutationRejectsMissingRowsAndPreservesExistingValues)
{
	CTypeTable<int> table;
	CHECK(table.GetMutable(0) == nullptr);
	CHECK(!table.Set(0, 7));
	table.Init(2);
	CHECK(table.Set(0, 11));
	CHECK(table.Set(1, 22));
	for (int index : {-1, 2, 100}) {
		CHECK(table.GetMutable(index) == nullptr);
		CHECK(!table.Set(index, 99));
		CHECK_EQ(0, table[index]);
	}
	CHECK_EQ(11, table[0]);
	CHECK_EQ(22, table[1]);
	table.Release();
	CHECK(table.GetMutable(0) == nullptr);
	CHECK(!table.Set(0, 7));
}

TEST(TypeTableAccess, MutableRowsBelongOnlyToTheirTable)
{
	CTypeTable<int> first;
	CTypeTable<int> second;
	first.Init(1);
	second.Init(1);
	CHECK(first.Set(0, 11));
	const int& borrowed = first.Get(0);
	int* row = first.GetMutable(0);
	CHECK(row != nullptr);
	if (row) *row = 42;
	CHECK_EQ(42, borrowed);
	CHECK_EQ(42, first[0]);
	CHECK_EQ(0, second[0]);
	CHECK_EQ(0, first[-1]);
	CHECK_EQ(0, second[-1]);
}

TEST(TypeTableAccess, NewAggregateRowsAreInitialized)
{
	struct Row { int count; unsigned value; };
	CTypeTable<Row> table;
	table.Init(2);
	for (int i = 0; i < 2; ++i) {
		CHECK_EQ(0, table[i].count);
		CHECK_EQ(0u, table[i].value);
	}
}

TEST(TypeTableAccess, StringMutationDoesNotChangeFallbackOrOtherTables)
{
	CTypeTable<MString> first;
	CTypeTable<MString> second;
	first.Init(1);
	second.Init(1);
	CHECK(first.Set(0, "kept"));
	CHECK(!first.Set(-1, "rejected"));
	CHECK(!first.Set(1, "rejected"));
	CHECK(first[0].GetString() != nullptr);
	if (first[0].GetString()) CHECK(std::string(first[0].GetString()) == "kept");
	CHECK(first[-1].GetString() == nullptr);
	CHECK(second[0].GetString() == nullptr);
	CHECK(second[-1].GetString() == nullptr);
}
