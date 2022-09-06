// SPDX-License-Identifier: CC0-1.0

#pragma once

#include <cstddef>

#include "Common/Future/CppLibConcepts.h"

namespace Common
{
template <std::unsigned_integral T>
constexpr T AlignUp(T value, size_t size)
{
  return static_cast<T>(value + (size - value % size) % size);
}

template <std::unsigned_integral T>
constexpr T AlignDown(T value, size_t size)
{
  return static_cast<T>(value - value % size);
}

}  // namespace Common
