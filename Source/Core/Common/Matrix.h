// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

// Tiny matrix/vector library.
// Used for things like Free-Look in the gfx backend.

union Vec3
{
  Vec3() {}
  Vec3(float _x, float _y, float _z) : data{_x, _y, _z} {}

  std::array<float, 3> data = {};

  struct
  {
    float x;
    float y;
    float z;
  };
};

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
  static Matrix44 FromArray(const float mtxArray[16]);

  static Matrix44 Translate(const Vec3& vec);
  static Matrix44 Shear(const float a, const float b = 0);

  static void Multiply(const Matrix44& a, const Matrix44& b, Matrix44& result);

  Matrix44& operator*=(const Matrix44& rhs)
  {
    Multiply(Matrix44(*this), rhs, *this);
    return *this;
  }

  std::array<float, 16> data;
};

inline Matrix44 operator*(Matrix44 lhs, const Matrix44& rhs)
{
  return lhs *= rhs;
}