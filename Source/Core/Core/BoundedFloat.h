// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <type_traits>

// NormalizedFloat: a floating point value constrained to the range [0, 1]
template <typename T = double>
class NormalizedFloat
{
  static_assert(std::is_arithmetic_v<T>, "NormalizedFloat requires an arithmetic type");

public:
  // Constructs from a raw value, clamping into [0, 1].
  explicit constexpr NormalizedFloat(T v) noexcept : value_(std::clamp(v, T(0), T(1))) {}

  // Implicit conversion back to the underlying type for easy arithmetic/use
  constexpr operator T() const noexcept { return value_; }

private:
  T value_;
};

// UnitFloat: a floating point value constrained to the range [-1, 1]
template <typename T = double>
class UnitFloat
{
  static_assert(std::is_arithmetic_v<T>, "UnitFloat requires an arithmetic type");

public:
  // Constructs from a raw value, clamping into [-1, 1].
  explicit constexpr UnitFloat(T v) noexcept : value_(std::clamp(v, T(-1), T(1))) {}

  // Implicit conversion back to the underlying type for easy arithmetic/use
  constexpr operator T() const noexcept { return value_; }

private:
  T value_;
};

template <typename T>
NormalizedFloat(T) -> NormalizedFloat<T>;

template <typename I, std::enable_if_t<std::is_integral_v<I>, int> = 0>
NormalizedFloat(I, I) -> NormalizedFloat<>;

template <typename T>
UnitFloat(T) -> UnitFloat<T>;

template <typename I, std::enable_if_t<std::is_integral_v<I>, int> = 0>
UnitFloat(I, I) -> UnitFloat<>;
