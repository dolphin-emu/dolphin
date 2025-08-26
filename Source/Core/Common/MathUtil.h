// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <concepts>
#include <limits>
#include <type_traits>
#include <utility>

#include "Common/CommonTypes.h"

namespace MathUtil
{
constexpr double TAU = 6.2831853071795865;
constexpr double PI = TAU / 2;
constexpr double GRAVITY_ACCELERATION = 9.80665;

template <typename T>
constexpr auto Sign(const T& val) -> decltype((T{} < val) - (val < T{}))
{
  return (T{} < val) - (val < T{});
}

template <typename T, typename F>
constexpr auto Lerp(const T& x, const T& y, const F& a) -> decltype(x + (y - x) * a)
{
  return x + (y - x) * a;
}

// Casts the specified value to a Dest. The value will be clamped to fit in the destination type.
// Warning: The result of SaturatingCast(NaN) is undefined.
template <std::integral Dest, typename T>
constexpr Dest SaturatingCast(T value)
{
  constexpr Dest lo = std::numeric_limits<Dest>::min();
  constexpr Dest hi = std::numeric_limits<Dest>::max();

  if constexpr (std::is_integral_v<T>)
  {
    if (std::cmp_less(value, lo))
      return lo;
    if (std::cmp_greater(value, hi))
      return hi;
  }
  else
  {
    if (value < static_cast<T>(lo))
      return lo;
    if (value >= static_cast<T>(hi))
      return hi;
  }
  return static_cast<Dest>(value);
}

template <typename T>
constexpr bool IsPow2(T imm)
{
  return imm > 0 && (imm & (imm - 1)) == 0;
}

constexpr u32 NextPowerOf2(u32 value)
{
  --value;
  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;
  ++value;

  return value;
}

template <class T>
struct Rectangle
{
  T left{};
  T top{};
  T right{};
  T bottom{};

  constexpr Rectangle() = default;

  constexpr Rectangle(T theLeft, T theTop, T theRight, T theBottom)
      : left(theLeft), top(theTop), right(theRight), bottom(theBottom)
  {
  }

  constexpr bool operator==(const Rectangle& r) const
  {
    return left == r.left && top == r.top && right == r.right && bottom == r.bottom;
  }

  constexpr T GetWidth() const { return GetDistance(left, right); }
  constexpr T GetHeight() const { return GetDistance(top, bottom); }
  // If the rectangle is in a coordinate system with a lower-left origin, use
  // this Clamp.
  void ClampLL(T x1, T y1, T x2, T y2)
  {
    left = std::clamp(left, x1, x2);
    right = std::clamp(right, x1, x2);
    top = std::clamp(top, y2, y1);
    bottom = std::clamp(bottom, y2, y1);
  }

  // If the rectangle is in a coordinate system with an upper-left origin,
  // use this Clamp.
  void ClampUL(T x1, T y1, T x2, T y2)
  {
    left = std::clamp(left, x1, x2);
    right = std::clamp(right, x1, x2);
    top = std::clamp(top, y1, y2);
    bottom = std::clamp(bottom, y1, y2);
  }

private:
  constexpr T GetDistance(T a, T b) const
  {
    if constexpr (std::is_unsigned<T>())
      return b > a ? b - a : a - b;
    else
      return std::abs(b - a);
  }
};

template <typename T>
class RunningMean
{
public:
  constexpr void Clear() { *this = {}; }

  constexpr void Push(T x) { m_mean = m_mean + (x - m_mean) / ++m_count; }

  constexpr size_t Count() const { return m_count; }
  constexpr T Mean() const { return m_mean; }

private:
  size_t m_count = 0;
  T m_mean{};
};

template <typename T>
class RunningVariance
{
public:
  constexpr void Clear() { *this = {}; }

  constexpr void Push(T x)
  {
    const auto old_mean = m_running_mean.Mean();
    m_running_mean.Push(x);
    m_variance += (x - old_mean) * (x - m_running_mean.Mean());
  }

  constexpr size_t Count() const { return m_running_mean.Count(); }
  constexpr T Mean() const { return m_running_mean.Mean(); }

  constexpr T Variance() const { return m_variance / (Count() - 1); }
  T StandardDeviation() const { return std::sqrt(Variance()); }

  constexpr T PopulationVariance() const { return m_variance / Count(); }
  T PopulationStandardDeviation() const { return std::sqrt(PopulationVariance()); }

private:
  RunningMean<T> m_running_mean;
  T m_variance{};
};

// Rounds down. 0 -> undefined
constexpr int IntLog2(u64 val)
{
  return 63 - std::countl_zero(val);
}
}  // namespace MathUtil
