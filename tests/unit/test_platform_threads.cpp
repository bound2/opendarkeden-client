#include "test_framework.h"
#include "Platform.h"
#include <array>
#include <atomic>

namespace {
struct Work {
	std::atomic<int>* sum;
	int value;
};

DWORD AddValue(void* parameter)
{
	const auto& work = *static_cast<Work*>(parameter);
	work.sum->fetch_add(work.value);
	return 0xFEEDBEEFu;
}
}

TEST(PlatformThreads, CallsTheSuppliedFunctionAndRetainsItsParameter)
{
	std::atomic<int> sum{0};
	Work work{&sum, 42};
	const auto thread = platform_thread_create(AddValue, &work);
	CHECK(thread != nullptr);
	if (!thread) return;
	CHECK_EQ(0, platform_thread_wait(thread));
	CHECK_EQ(42, sum.load());
#ifdef PLATFORM_WINDOWS
	DWORD code = 0;
	CHECK(GetExitCodeThread(thread, &code) != 0);
	CHECK_EQ(0xFEEDBEEFu, code);
#endif
	platform_thread_close(thread);
}

TEST(PlatformThreads, ConcurrentStartsOwnSeparateCallbackData)
{
	std::atomic<int> sum{0};
	std::array<Work, 16> work{};
	std::array<platform_thread_t, 16> threads{};
	for (int i = 0; i < 16; ++i) {
		work[i] = {&sum, i};
		threads[i] = platform_thread_create(AddValue, &work[i]);
		CHECK(threads[i] != nullptr);
	}
	for (auto thread : threads) {
		if (!thread) continue;
		CHECK_EQ(0, platform_thread_wait(thread));
		platform_thread_close(thread);
	}
	CHECK_EQ(120, sum.load());
}

TEST(PlatformThreads, MissingCallbacksAreRejectedBeforeStartingAThread)
{
	CHECK(platform_thread_create(nullptr, nullptr) == nullptr);
	CHECK_EQ(1, platform_thread_wait(nullptr));
	platform_thread_close(nullptr);
}
