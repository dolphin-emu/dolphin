// SPDX-License-Identifier: CC0-1.0

#pragma once

#include <type_traits>

namespace Common
{
// TODO: Replace with std::to_underlying in C++23
template <typename Enum>
constexpr std::underlying_type_t<Enum> ToUnderlying(Enum e) noexcept
{
  return static_cast<std::underlying_type_t<Enum>>(e);
}
}  // namespace Common
