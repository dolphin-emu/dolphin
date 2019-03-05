// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Matrix.h"

#include <algorithm>
#include <cmath>

namespace
{
// Multiply a NxM matrix by a NxP matrix.
template <int N, int M, int P, typename T>
auto MatrixMultiply(const std::array<T, N * M>& a, const std::array<T, M * P>& b)
    -> std::array<T, N * P>
{
  std::array<T, N * P> result;

  for (int n = 0; n != N; ++n)
  {
    for (int p = 0; p != P; ++p)
    {
      T temp = {};
      for (int m = 0; m != M; ++m)
      {
        temp += a[n * M + m] * b[m * P + p];
      }
      result[n * P + p] = temp;
    }
  }

  return result;
}

}  // namespace

namespace Common
{
Matrix33 Matrix33::Identity()
{
  Matrix33 mtx = {};
  mtx.data[0] = 1.0f;
  mtx.data[4] = 1.0f;
  mtx.data[8] = 1.0f;
  return mtx;
}

Matrix33 Matrix33::RotateX(float rad)
{
  const float s = std::sin(rad);
  const float c = std::cos(rad);
  Matrix33 mtx = {};
  mtx.data[0] = 1;
  mtx.data[4] = c;
  mtx.data[5] = -s;
  mtx.data[7] = s;
  mtx.data[8] = c;
  return mtx;
}

Matrix33 Matrix33::RotateY(float rad)
{
  const float s = std::sin(rad);
  const float c = std::cos(rad);
  Matrix33 mtx = {};
  mtx.data[0] = c;
  mtx.data[2] = s;
  mtx.data[4] = 1;
  mtx.data[6] = -s;
  mtx.data[8] = c;
  return mtx;
}

Matrix33 Matrix33::RotateZ(float rad)
{
  const float s = std::sin(rad);
  const float c = std::cos(rad);
  Matrix33 mtx = {};
  mtx.data[0] = c;
  mtx.data[1] = -s;
  mtx.data[3] = s;
  mtx.data[4] = c;
  mtx.data[8] = 1;
  return mtx;
}

Matrix33 Matrix33::Scale(const Vec3& vec)
{
  Matrix33 mtx = {};
  mtx.data[0] = vec.x;
  mtx.data[4] = vec.y;
  mtx.data[8] = vec.z;
  return mtx;
}

void Matrix33::Multiply(const Matrix33& a, const Matrix33& b, Matrix33* result)
{
  result->data = MatrixMultiply<3, 3, 3>(a.data, b.data);
}

void Matrix33::Multiply(const Matrix33& a, const Vec3& vec, Vec3* result)
{
  result->data = MatrixMultiply<3, 3, 1>(a.data, vec.data);
}

Matrix44 Matrix44::Identity()
{
  Matrix44 mtx = {};
  mtx.data[0] = 1.0f;
  mtx.data[5] = 1.0f;
  mtx.data[10] = 1.0f;
  mtx.data[15] = 1.0f;
  return mtx;
}

Matrix44 Matrix44::FromMatrix33(const Matrix33& m33)
{
  Matrix44 mtx;
  for (int i = 0; i < 3; ++i)
  {
    for (int j = 0; j < 3; ++j)
    {
      mtx.data[i * 4 + j] = m33.data[i * 3 + j];
    }
  }

  for (int i = 0; i < 3; ++i)
  {
    mtx.data[i * 4 + 3] = 0;
    mtx.data[i + 12] = 0;
  }
  mtx.data[15] = 1.0f;
  return mtx;
}

Matrix44 Matrix44::FromArray(const std::array<float, 16>& arr)
{
  Matrix44 mtx;
  mtx.data = arr;
  return mtx;
}

Matrix44 Matrix44::Translate(const Vec3& vec)
{
  Matrix44 mtx = Matrix44::Identity();
  mtx.data[3] = vec.x;
  mtx.data[7] = vec.y;
  mtx.data[11] = vec.z;
  return mtx;
}

Matrix44 Matrix44::Shear(const float a, const float b)
{
  Matrix44 mtx = Matrix44::Identity();
  mtx.data[2] = a;
  mtx.data[6] = b;
  return mtx;
}

Matrix44 Matrix44::Perspective(float fov_y, float aspect_ratio, float z_near, float z_far)
{
  Matrix44 mtx{};
  const float tan_half_fov_y = std::tan(fov_y / 2);
  mtx.data[0] = 1 / (aspect_ratio * tan_half_fov_y);
  mtx.data[5] = 1 / tan_half_fov_y;
  mtx.data[10] = -(z_far + z_near) / (z_far - z_near);
  mtx.data[11] = -(2 * z_far * z_near) / (z_far - z_near);
  mtx.data[14] = -1;
  return mtx;
}

void Matrix44::Multiply(const Matrix44& a, const Matrix44& b, Matrix44* result)
{
  result->data = MatrixMultiply<4, 4, 4>(a.data, b.data);
}

Vec3 Matrix44::Transform(const Vec3& v, float w) const
{
  const auto result = MatrixMultiply<4, 4, 1>(data, {v.x, v.y, v.z, w});
  return Vec3{result[0], result[1], result[2]};
}

void Matrix44::Multiply(const Matrix44& a, const Vec4& vec, Vec4* result)
{
  result->data = MatrixMultiply<4, 4, 1>(a.data, vec.data);
}

}  // namespace Common
