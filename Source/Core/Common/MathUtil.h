// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <vector>

#include "Common/CommonTypes.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace MathUtil
{
constexpr double TAU = 6.2831853071795865;
constexpr double PI = TAU / 2;

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

  T GetWidth() const { return abs(right - left); }
  T GetHeight() const { return abs(bottom - top); }
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
};

}  // namespace MathUtil

float MathFloatVectorSum(const std::vector<float>&);

// Rounds down. 0 -> undefined
inline int IntLog2(u64 val)
{
#if defined(__GNUC__)
  return 63 - __builtin_clzll(val);

#elif defined(_MSC_VER)
  unsigned long result = ULONG_MAX;
  _BitScanReverse64(&result, val);
  return result;

#else
  int result = -1;
  while (val != 0)
  {
    val >>= 1;
    ++result;
  }
  return result;
#endif
}
