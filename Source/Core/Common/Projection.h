// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <utility>

namespace Common::Projection
{
struct First
{
  // TODO C++23: static operator()
  template <class T>
  [[nodiscard]] constexpr auto&& operator()(T&& t) const noexcept
  {
    return std::forward<T>(t).first;
  }
};

struct Second
{
  // TODO C++23: static operator()
  template <class T>
  [[nodiscard]] constexpr auto&& operator()(T&& t) const noexcept
  {
    return std::forward<T>(t).second;
  }
};

using Key = First;
using Value = Second;
}  // namespace Common::Projection
