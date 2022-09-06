// SPDX-License-Identifier: CC0-1.0

#pragma once

#include <cstddef>
#include "Common/Concepts.h"

namespace Common
{
template <UnsignedIntegral T>
constexpr T AlignUp(T value, size_t size)
{
  return static_cast<T>(value + (size - value % size) % size);
}

template <UnsignedIntegral T>
constexpr T AlignDown(T value, size_t size)
{
  return static_cast<T>(value - value % size);
}

}  // namespace Common
