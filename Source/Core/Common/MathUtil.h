// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>

#include "Common/CommonTypes.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace MathUtil
{
template <class T>
constexpr T Clamp(const T val, const T& min, const T& max)
{
  return std::max(min, std::min(max, val));
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
    left = Clamp(left, x1, x2);
    right = Clamp(right, x1, x2);
    top = Clamp(top, y2, y1);
    bottom = Clamp(bottom, y2, y1);
  }

  // If the rectangle is in a coordinate system with an upper-left origin,
  // use this Clamp.
  void ClampUL(T x1, T y1, T x2, T y2)
  {
    left = Clamp(left, x1, x2);
    right = Clamp(right, x1, x2);
    top = Clamp(top, y1, y2);
    bottom = Clamp(bottom, y1, y2);
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

// Tiny matrix/vector library.
// Used for things like Free-Look in the gfx backend.

class Matrix33
{
public:
  static void LoadIdentity(Matrix33& mtx);

  // set mtx to be a rotation matrix around the x axis
  static void RotateX(Matrix33& mtx, float rad);
  // set mtx to be a rotation matrix around the y axis
  static void RotateY(Matrix33& mtx, float rad);

  // set result = a x b
  static void Multiply(const Matrix33& a, const Matrix33& b, Matrix33& result);
  static void Multiply(const Matrix33& a, const float vec[3], float result[3]);

  float data[9];
};

class Matrix44
{
public:
  static void LoadIdentity(Matrix44& mtx);
  static void LoadMatrix33(Matrix44& mtx, const Matrix33& m33);
  static void Set(Matrix44& mtx, const float mtxArray[16]);

  static void Translate(Matrix44& mtx, const float vec[3]);
  static void Shear(Matrix44& mtx, const float a, const float b = 0);

  static void Multiply(const Matrix44& a, const Matrix44& b, Matrix44& result);

  float data[16];
};
