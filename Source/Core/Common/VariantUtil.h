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

// Visits a functor with a variant_alternative of the given index.
// e.g. WithVariantAlternative<variant<int, float>>(1, func) calls func<float>()
// An out-of-bounds index causes no visitation.
template <typename Variant, std::size_t I = 0, typename Func>
void WithVariantAlternative(std::size_t index, Func&& func)
{
  if constexpr (I < std::variant_size_v<Variant>)
  {
    if (index == 0)
      func.template operator()<std::variant_alternative_t<I, Variant>>();
    else
      WithVariantAlternative<Variant, I + 1>(index - 1, std::forward<Func>(func));
  }
}
