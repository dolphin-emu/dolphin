// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <iterator>
#include <version>

namespace Common
{
#if defined(__cpp_lib_ranges_contains) && __cpp_lib_ranges_contains >= 202202L

// Use the standard library functions if available (C++23)
inline constexpr auto& Contains = std::ranges::contains;
inline constexpr auto& ContainsSubrange = std::ranges::contains_subrange;

#else
// TODO C++23: This old implementation likely isn't needed once migrated to C++23. Remove them or
// this file itself. Ad hoc implementations for C++20
struct ContainsFn
{
  template <std::input_iterator I, std::sentinel_for<I> S, class T, class Proj = std::identity>
  requires std::indirect_binary_predicate<std::ranges::equal_to, std::projected<I, Proj>, const T*>
  constexpr bool operator()(I first, S last, const T& value, Proj proj = {}) const
  {
    return std::ranges::find(std::move(first), last, value, std::move(proj)) != last;
  }

  template <std::ranges::input_range R, class T, class Proj = std::identity>
  requires std::indirect_binary_predicate<
      std::ranges::equal_to, std::projected<std::ranges::iterator_t<R>, Proj>, const T*>
  constexpr bool operator()(R&& r, const T& value, Proj proj = {}) const
  {
    return (*this)(std::ranges::begin(r), std::ranges::end(r), value, std::move(proj));
  }
};

struct ContainsSubrangeFn
{
  template <std::forward_iterator I1, std::sentinel_for<I1> S1, std::forward_iterator I2,
            std::sentinel_for<I2> S2, class Pred = std::ranges::equal_to,
            class Proj1 = std::identity, class Proj2 = std::identity>
  requires std::indirectly_comparable<I1, I2, Pred, Proj1, Proj2>
  constexpr bool operator()(I1 first1, S1 last1, I2 first2, S2 last2, Pred pred = {},
                            Proj1 proj1 = {}, Proj2 proj2 = {}) const
  {
    return !std::ranges::search(std::move(first1), std::move(last1), std::move(first2),
                                std::move(last2), std::move(pred), std::move(proj1),
                                std::move(proj2))
                .empty();
  }

  template <std::ranges::forward_range R1, std::ranges::forward_range R2,
            class Pred = std::ranges::equal_to, class Proj1 = std::identity,
            class Proj2 = std::identity>
  requires std::indirectly_comparable<std::ranges::iterator_t<R1>, std::ranges::iterator_t<R2>,
                                      Pred, Proj1, Proj2>
  constexpr bool operator()(R1&& r1, R2&& r2, Pred pred = {}, Proj1 proj1 = {},
                            Proj2 proj2 = {}) const
  {
    return (*this)(std::ranges::begin(r1), std::ranges::end(r1), std::ranges::begin(r2),
                   std::ranges::end(r2), std::move(pred), std::move(proj1), std::move(proj2));
  }
};

inline constexpr ContainsFn Contains{};
inline constexpr ContainsSubrangeFn ContainsSubrange{};

#endif
}  // namespace Common
