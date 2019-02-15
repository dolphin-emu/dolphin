// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cmath>

// Tiny matrix/vector library.
// Used for things like Free-Look in the gfx backend.

namespace Common
{
union Vec3
{
  Vec3() = default;
  Vec3(float _x, float _y, float _z) : data{_x, _y, _z} {}

  Vec3& operator+=(const Vec3& rhs)
  {
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    return *this;
  }

  std::array<float, 3> data = {};

  struct
  {
    float x;
    float y;
    float z;
  };
};

inline Vec3 operator+(Vec3 lhs, const Vec3& rhs)
{
  return lhs += rhs;
}

template <typename T>
union TVec2
{
  TVec2() = default;
  TVec2(T _x, T _y) : data{_x, _y} {}

  T Cross(const TVec2& rhs) const { return (x * rhs.y) - (y * rhs.x); }
  T Dot(const TVec2& rhs) const { return (x * rhs.x) + (y * rhs.y); }
  T LengthSquared() const { return Dot(*this); }
  T Length() const { return std::sqrt(LengthSquared()); }
  TVec2 Normalized() const { return *this / Length(); }

  TVec2& operator+=(const TVec2& rhs)
  {
    x += rhs.x;
    y += rhs.y;
    return *this;
  }

  TVec2& operator-=(const TVec2& rhs)
  {
    x -= rhs.x;
    y -= rhs.y;
    return *this;
  }

  TVec2& operator*=(T scalar)
  {
    x *= scalar;
    y *= scalar;
    return *this;
  }

  TVec2 operator-() const { return {-x, -y}; }

  std::array<T, 2> data = {};

  struct
  {
    T x;
    T y;
  };
};

template <typename T>
TVec2<T> operator+(TVec2<T> lhs, const TVec2<T>& rhs)
{
  return lhs += rhs;
}

template <typename T>
TVec2<T> operator-(TVec2<T> lhs, const TVec2<T>& rhs)
{
  return lhs -= rhs;
}

template <typename T>
TVec2<T> operator*(TVec2<T> lhs, T scalar)
{
  return lhs *= scalar;
}

using Vec2 = TVec2<float>;
using DVec2 = TVec2<double>;

class Matrix33
{
public:
  static Matrix33 Identity();

  // Return a rotation matrix around the x,y,z axis
  static Matrix33 RotateX(float rad);
  static Matrix33 RotateY(float rad);
  static Matrix33 RotateZ(float rad);

  static Matrix33 Scale(const Vec3& vec);

  // set result = a x b
  static void Multiply(const Matrix33& a, const Matrix33& b, Matrix33* result);
  static void Multiply(const Matrix33& a, const Vec3& vec, Vec3* result);

  Matrix33& operator*=(const Matrix33& rhs)
  {
    Multiply(Matrix33(*this), rhs, this);
    return *this;
  }

  std::array<float, 9> data;
};

inline Matrix33 operator*(Matrix33 lhs, const Matrix33& rhs)
{
  return lhs *= rhs;
}

inline Vec3 operator*(const Matrix33& lhs, const Vec3& rhs)
{
  Vec3 result;
  Matrix33::Multiply(lhs, rhs, &result);
  return result;
}

class Matrix44
{
public:
  static Matrix44 Identity();
  static Matrix44 FromMatrix33(const Matrix33& m33);
  static Matrix44 FromArray(const std::array<float, 16>& arr);

  static Matrix44 Translate(const Vec3& vec);
  static Matrix44 Shear(const float a, const float b = 0);

  static void Multiply(const Matrix44& a, const Matrix44& b, Matrix44* result);

  Matrix44& operator*=(const Matrix44& rhs)
  {
    Multiply(Matrix44(*this), rhs, this);
    return *this;
  }

  std::array<float, 16> data;
};

inline Matrix44 operator*(Matrix44 lhs, const Matrix44& rhs)
{
  return lhs *= rhs;
}
}  // namespace Common
