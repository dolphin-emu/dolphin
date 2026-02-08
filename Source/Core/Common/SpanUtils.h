// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <cstddef>
#include <span>
#include <utility>

#include "Common/CommonTypes.h"

namespace Common
{

// Like std::span::subspan, except undefined behavior is replaced with returning a 0-length span.
template <class T>
[[nodiscard]] constexpr std::span<T> SafeSubspan(std::span<T> span, size_t offset,
                                                 size_t count = std::dynamic_extent)
{
  if (count == std::dynamic_extent || offset > span.size())
    return span.subspan(std::min(offset, span.size()));
  else
    return span.subspan(offset, std::min(count, span.size() - offset));
}

// Default-constructs an object of type T, then copies data into it from the specified offset in
// the specified span. Out-of-bounds reads will be skipped, meaning that specifying a too large
// offset results in the object partially or entirely remaining default constructed.
template <class T>
[[nodiscard]] T SafeSpanRead(std::span<const u8> span, size_t offset)
{
  static_assert(std::is_trivially_copyable<T>());

  const std::span<const u8> subspan = SafeSubspan(span, offset);
  T result{};
  std::memcpy(&result, subspan.data(), std::min(subspan.size(), sizeof(result)));
  return result;
}

}  // namespace Common
