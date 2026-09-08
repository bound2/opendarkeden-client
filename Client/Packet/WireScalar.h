//////////////////////////////////////////////////////////////////////////////
// Filename    : WireScalar.h
// Description : C++20 constraints for fixed-width packet scalar values
//////////////////////////////////////////////////////////////////////////////

#ifndef __WIRE_SCALAR_H__
#define __WIRE_SCALAR_H__

#include <bit>
#include <concepts>
#include <cstdint>
#include <type_traits>

namespace packetwire {

template <typename T, bool IsEnum = std::is_enum_v<std::remove_cv_t<T>>>
struct WireStorage
{
	using type = std::remove_cv_t<T>;
};

template <typename T>
struct WireStorage<T, true>
{
	using type = std::underlying_type_t<std::remove_cv_t<T>>;
};

template <typename T>
using WireStorageT = typename WireStorage<T>::type;

// The eight exact-width types, plus long long and unsigned long long
// by name. Under MSVC those two *are* int64_t and uint64_t; on LP64
// Linux and macOS the exact-width pair is long, and `long long` is a
// distinct type of the same width, which is what the tree's ulonglong
// (unsigned long long off MSVC) is. Without the two names the exchange
// packets' readWire(ulonglong&) has no overload there.
//
// Deliberately not by size: an "any 8-byte integral" rule would admit
// `long` and `unsigned long` on LP64 as 8-byte scalars, while MSVC,
// where they are 4 bytes and neither int32_t nor int64_t, rejects them
// - the same source line would be two widths. As written, `long` is a
// wire scalar only where it is the int64_t spelling; the narrowing
// read(long&)/write(long) overloads are the way a long reaches the
// wire, at four bytes on every platform (tests/unit/test_wire_widths.cpp).
template <typename T>
concept FixedWidthInteger =
	std::same_as<std::remove_cv_t<T>, std::int8_t> ||
	std::same_as<std::remove_cv_t<T>, std::uint8_t> ||
	std::same_as<std::remove_cv_t<T>, std::int16_t> ||
	std::same_as<std::remove_cv_t<T>, std::uint16_t> ||
	std::same_as<std::remove_cv_t<T>, std::int32_t> ||
	std::same_as<std::remove_cv_t<T>, std::uint32_t> ||
	std::same_as<std::remove_cv_t<T>, std::int64_t> ||
	std::same_as<std::remove_cv_t<T>, std::uint64_t> ||
	std::same_as<std::remove_cv_t<T>, long long> ||
	std::same_as<std::remove_cv_t<T>, unsigned long long>;

template <typename T>
concept ScopedWireEnum =
	std::is_enum_v<std::remove_cv_t<T>> &&
	!std::is_convertible_v<std::remove_cv_t<T>, WireStorageT<T>>;

template <typename T>
concept WireScalar =
	FixedWidthInteger<T> ||
	// Scoped enums always have a fixed underlying type. C++20 cannot
	// portably make that guarantee for an arbitrary unscoped enum.
	(ScopedWireEnum<T> && FixedWidthInteger<WireStorageT<T>>);

template <typename T>
concept WritableWireScalar =
	WireScalar<T> && !std::is_const_v<T>;

static_assert(std::endian::native == std::endian::little,
	"The packet protocol requires a little-endian target");

} // namespace packetwire

#endif // __WIRE_SCALAR_H__
