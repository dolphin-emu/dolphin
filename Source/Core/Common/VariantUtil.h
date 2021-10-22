// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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

template <class... Ts>
struct overloaded : Ts...
{
  using Ts::operator()...;
};

template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
