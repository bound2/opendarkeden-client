#include "test_framework.h"
#include "MItem.h"
#include "MItemManager.h"
#include <algorithm>
#include <array>
#include <new>

namespace {
std::array<MItem*, 16> destroyed{};
size_t destroyedCount = 0;
void ItemDestroyed(MItem* item) noexcept
{
	if (destroyedCount < destroyed.size())
		destroyed[destroyedCount++] = item;
}

struct HostScope {
	const MItemHost* previous = MItem::GetHost();
	MItemHost host{};
	HostScope()
	{
		destroyed.fill(nullptr);
		destroyedCount = 0;
		host.ItemDestroyed = ItemDestroyed;
		MItem::SetHost(&host);
	}
	~HostScope() { MItem::SetHost(previous); }
};
}

TEST(ItemLifetime, StackAndVirtualDeletionNotifyWithTheirIdentityExactlyOnce)
{
	HostScope scope;
	MItem* first = nullptr;
	{
		MSword item;
		first = &item;
		CHECK_EQ(size_t(0), destroyedCount);
	}
	CHECK_EQ(size_t(1), destroyedCount);
	CHECK(destroyed[0] == first);
	MItem* second = new MSword;
	delete second;
	CHECK_EQ(size_t(2), destroyedCount);
	CHECK(destroyed[1] == second);
}

TEST(ItemLifetime, ContainerReleaseNotifiesForEveryOwnedItem)
{
	HostScope scope;
	MItemManager manager;
	std::array<MItem*, 3> items{new MSword, new MSword, new MSword};
	for (size_t i = 0; i < items.size(); ++i)
	{
		items[i]->SetID(static_cast<TYPE_OBJECTID>(i + 1));
		CHECK(manager.AddItem(items[i]));
	}
	CHECK_EQ(size_t(0), destroyedCount);
	manager.Release();
	CHECK_EQ(items.size(), destroyedCount);
	for (MItem* item : items)
		CHECK_EQ(1, std::count(destroyed.begin(), destroyed.end(), item));
	manager.Release();
	CHECK_EQ(items.size(), destroyedCount);
}

TEST(ItemLifetime, ReusedAddressesNotifyForEachDistinctLifetime)
{
	HostScope scope;
	alignas(MSword) unsigned char storage[sizeof(MSword)];
	for (int i = 0; i < 2; ++i)
	{
		MSword* item = ::new (storage) MSword;
		item->SetID(0);
		item->~MSword();
		CHECK_EQ(static_cast<size_t>(i + 1), destroyedCount);
		CHECK(destroyed[static_cast<size_t>(i)] == item);
	}
}

TEST(ItemLifetime, DestructionAllowsAnAbsentHostOrObserver)
{
	HostScope scope;
	MItem::SetHost(nullptr);
	{ MSword item; }
	MItemHost empty{};
	MItem::SetHost(&empty);
	{ MSword item; }
	CHECK_EQ(size_t(0), destroyedCount);
}
