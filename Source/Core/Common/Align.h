// This file is under the public domain.

#pragma once

#include <cstddef>
#include <type_traits>

#if defined(_MSC_VER) && _MSC_VER <= 1800
#define constexpr inline
#define static_assert(a, b)
#endif

namespace Common
{
template <typename T>
constexpr T AlignUp(T value, size_t size)
{
  static_assert(std::is_unsigned<T>(), "T must be an unsigned value.");
  return static_cast<T>(value + (size - value % size) % size);
}

template <typename T>
constexpr T AlignDown(T value, size_t size)
{
  static_assert(std::is_unsigned<T>(), "T must be an unsigned value.");
  return static_cast<T>(value - value % size);
}

}  // namespace Common

#if defined(_MSC_VER) && _MSC_VER <= 1800
#undef constexpr
#undef static_assert
#endif
