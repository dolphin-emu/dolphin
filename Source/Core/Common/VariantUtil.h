// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <variant>

namespace detail
{
template <typename... From>
struct VariantCastProxy
{
  const std::variant<From...>& v;

  template <typename... To>
  operator std::variant<To...>() const
  {
    return std::visit([](auto&& arg) { return std::variant<To...>{arg}; }, v);
  }
};
}  // namespace detail

template <typename... From>
auto VariantCast(const std::variant<From...>& v)
{
  return detail::VariantCastProxy<From...>{v};
}
