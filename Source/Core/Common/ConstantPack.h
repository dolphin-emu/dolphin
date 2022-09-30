// 2022 Dolphin Emulator Project
// SPDX-License-Identifier: CC0-1.0

#pragma once

#include <cstddef>
#include <utility>

// ConstantPack allows one to recursively iterate through a std::integer_sequence
// e.g. Common::ConstantPack<int, 1, 2, 3, 4>::peel::peel::first == 3

namespace Common
{
template <typename T, T... vals>
struct ConstantPack : public std::integer_sequence<T, vals...>
{
  using value_type = T;
  using peel = ConstantPack<T, vals...>;
};

template <typename T, T val, T... vals>
struct ConstantPack<T, val, vals...> : public std::integer_sequence<T, val, vals...>
{
  using value_type = T;
  using peel = ConstantPack<T, vals...>;
  static constexpr T first = val;
};

template <std::size_t... vals>
using IndexPack = ConstantPack<std::size_t, vals...>;

namespace detail
{
template <typename T, T... vals>
void PassConstantPack(ConstantPack<T, vals...>);
}  // namespace detail

template <class T, typename U = typename T::value_type>
concept AnyConstantPack = requires(T t)
{
  detail::PassConstantPack<U>(t);
};
}  // namespace Common
