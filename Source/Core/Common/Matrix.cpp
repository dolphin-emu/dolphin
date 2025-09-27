// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Matrix.h"

#include <algorithm>
#include <cmath>

#include "Common/MathUtil.h"

namespace
{
// Multiply a NxM matrix by a MxP matrix.
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
Quaternion Quaternion::Identity()
{
  return Quaternion(1, 0, 0, 0);
}

Quaternion Quaternion::RotateX(float rad)
{
  return Rotate(rad, Vec3(1, 0, 0));
}

Quaternion Quaternion::RotateY(float rad)
{
  return Rotate(rad, Vec3(0, 1, 0));
}

Quaternion Quaternion::RotateZ(float rad)
{
  return Rotate(rad, Vec3(0, 0, 1));
}

Quaternion Quaternion::RotateXYZ(const Vec3& rads)
{
  const auto length = rads.Length();
  return length ? Common::Quaternion::Rotate(length, rads / length) :
                  Common::Quaternion::Identity();
}

Quaternion Quaternion::Rotate(float rad, const Vec3& axis)
{
  const auto sin_angle_2 = std::sin(rad / 2);

  return Quaternion(std::cos(rad / 2), axis.x * sin_angle_2, axis.y * sin_angle_2,
                    axis.z * sin_angle_2);
}

Quaternion::Quaternion(float w, float x, float y, float z) : data(x, y, z, w)
{
}

float Quaternion::Norm() const
{
  return std::sqrt(data.Dot(data));
}

Quaternion Quaternion::Normalized() const
{
  Quaternion result(*this);
  result.data /= Norm();
  return result;
}

Quaternion Quaternion::Conjugate() const
{
  return Quaternion(data.w, -1 * data.x, -1 * data.y, -1 * data.z);
}

Quaternion Quaternion::Inverted() const
{
  return Normalized().Conjugate();
}

Quaternion& Quaternion::operator*=(const Quaternion& rhs)
{
  auto& a = data;
  auto& b = rhs.data;

  data = Vec4{a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
              a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
              a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
              // W
              a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
  return *this;
}

Quaternion operator*(Quaternion lhs, const Quaternion& rhs)
{
  return lhs *= rhs;
}

Vec3 operator*(const Quaternion& lhs, const Vec3& rhs)
{
  const auto result = lhs * Quaternion(0, rhs.x, rhs.y, rhs.z) * lhs.Conjugate();
  return Vec3(result.data.x, result.data.y, result.data.z);
}

Vec3 FromQuaternionToEuler(const Quaternion& q)
{
  Vec3 result;

  const float qx = q.data.x;
  const float qy = q.data.y;
  const float qz = q.data.z;
  const float qw = q.data.w;

  const float sinr_cosp = 2 * (qw * qx + qy * qz);
  const float cosr_cosp = 1 - 2 * (qx * qx + qy * qy);
  result.x = std::atan2(sinr_cosp, cosr_cosp);

  const float sinp = 2 * (qw * qy - qz * qx);
  if (std::abs(sinp) >= 1)
    result.y = std::copysign(MathUtil::PI / 2, sinp);  // use 90 degrees if out of range
  else
    result.y = std::asin(sinp);

  const float siny_cosp = 2 * (qw * qz + qx * qy);
  const float cosy_cosp = 1 - 2 * (qy * qy + qz * qz);
  result.z = std::atan2(siny_cosp, cosy_cosp);

  return result;
}

Matrix33 Matrix33::Identity()
{
  Matrix33 mtx = {};
  mtx.data[0] = 1.0f;
  mtx.data[4] = 1.0f;
  mtx.data[8] = 1.0f;
  return mtx;
}

Matrix33 Matrix33::FromQuaternion(const Quaternion& q)
{
  const auto qx = q.data.x;
  const auto qy = q.data.y;
  const auto qz = q.data.z;
  const auto qw = q.data.w;

  return {
      1 - 2 * qy * qy - 2 * qz * qz, 2 * qx * qy - 2 * qz * qw,     2 * qx * qz + 2 * qy * qw,
      2 * qx * qy + 2 * qz * qw,     1 - 2 * qx * qx - 2 * qz * qz, 2 * qy * qz - 2 * qx * qw,
      2 * qx * qz - 2 * qy * qw,     2 * qy * qz + 2 * qx * qw,     1 - 2 * qx * qx - 2 * qy * qy,
  };
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

Matrix33 Matrix33::Rotate(float rad, const Vec3& axis)
{
  const float s = std::sin(rad);
  const float c = std::cos(rad);
  Matrix33 mtx;
  mtx.data[0] = axis.x * axis.x * (1 - c) + c;
  mtx.data[1] = axis.x * axis.y * (1 - c) - axis.z * s;
  mtx.data[2] = axis.x * axis.z * (1 - c) + axis.y * s;
  mtx.data[3] = axis.y * axis.x * (1 - c) + axis.z * s;
  mtx.data[4] = axis.y * axis.y * (1 - c) + c;
  mtx.data[5] = axis.y * axis.z * (1 - c) - axis.x * s;
  mtx.data[6] = axis.z * axis.x * (1 - c) - axis.y * s;
  mtx.data[7] = axis.z * axis.y * (1 - c) + axis.x * s;
  mtx.data[8] = axis.z * axis.z * (1 - c) + c;
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

float Matrix33::Determinant() const
{
  const auto m = [this](int x, int y) { return data[y + x * 3]; };

  return m(0, 0) * (m(1, 1) * m(2, 2) - m(2, 1) * m(1, 2)) -
         m(0, 1) * (m(1, 0) * m(2, 2) - m(1, 2) * m(2, 0)) +
         m(0, 2) * (m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0));
}

Matrix33 Matrix33::Inverted() const
{
  const auto m = [this](int x, int y) { return data[y + x * 3]; };

  const auto invdet = 1 / Determinant();

  Matrix33 result;

  const auto minv = [&result](int x, int y) -> auto& { return result.data[y + x * 3]; };

  minv(0, 0) = (m(1, 1) * m(2, 2) - m(2, 1) * m(1, 2)) * invdet;
  minv(0, 1) = (m(0, 2) * m(2, 1) - m(0, 1) * m(2, 2)) * invdet;
  minv(0, 2) = (m(0, 1) * m(1, 2) - m(0, 2) * m(1, 1)) * invdet;
  minv(1, 0) = (m(1, 2) * m(2, 0) - m(1, 0) * m(2, 2)) * invdet;
  minv(1, 1) = (m(0, 0) * m(2, 2) - m(0, 2) * m(2, 0)) * invdet;
  minv(1, 2) = (m(1, 0) * m(0, 2) - m(0, 0) * m(1, 2)) * invdet;
  minv(2, 0) = (m(1, 0) * m(2, 1) - m(2, 0) * m(1, 1)) * invdet;
  minv(2, 1) = (m(2, 0) * m(0, 1) - m(0, 0) * m(2, 1)) * invdet;
  minv(2, 2) = (m(0, 0) * m(1, 1) - m(1, 0) * m(0, 1)) * invdet;

  return result;
}

Matrix33 Matrix33::Transposed() const
{
  Matrix33 result;

  result.data[0] = data[0];
  result.data[1] = data[3];
  result.data[2] = data[6];
  result.data[3] = data[1];
  result.data[4] = data[4];
  result.data[5] = data[7];
  result.data[6] = data[2];
  result.data[7] = data[5];
  result.data[8] = data[8];

  return result;
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

Matrix44 Matrix44::FromQuaternion(const Quaternion& q)
{
  return FromMatrix33(Matrix33::FromQuaternion(q));
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

float Matrix44::Determinant() const
{
  const auto& m = data;
  return m[12] * m[9] * m[6] * m[3] - m[8] * m[13] * m[6] * m[3] - m[12] * m[5] * m[10] * m[3] +
         m[4] * m[13] * m[10] * m[3] + m[8] * m[5] * m[14] * m[3] - m[4] * m[9] * m[14] * m[3] -
         m[12] * m[9] * m[2] * m[7] + m[8] * m[13] * m[2] * m[7] + m[12] * m[1] * m[10] * m[7] -
         m[0] * m[13] * m[10] * m[7] - m[8] * m[1] * m[14] * m[7] + m[0] * m[9] * m[14] * m[7] +
         m[12] * m[5] * m[2] * m[11] - m[4] * m[13] * m[2] * m[11] - m[12] * m[1] * m[6] * m[11] +
         m[0] * m[13] * m[6] * m[11] + m[4] * m[1] * m[14] * m[11] - m[0] * m[5] * m[14] * m[11] -
         m[8] * m[5] * m[2] * m[15] + m[4] * m[9] * m[2] * m[15] + m[8] * m[1] * m[6] * m[15] -
         m[0] * m[9] * m[6] * m[15] - m[4] * m[1] * m[10] * m[15] + m[0] * m[5] * m[10] * m[15];
}

Matrix44 Matrix44::Inverted() const
{
  const auto m = [this](int x, int y) { return data[y + x * 4]; };

  const auto invdet = 1 / Determinant();

  Matrix44 result;

  const auto minv = [&result](int x, int y) -> auto& { return result.data[y + x * 4]; };

  const double A2323 = m(2, 2) * m(3, 3) - m(2, 3) * m(3, 2);
  const double A1323 = m(2, 1) * m(3, 3) - m(2, 3) * m(3, 1);
  const double A1223 = m(2, 1) * m(3, 2) - m(2, 2) * m(3, 1);
  const double A0323 = m(2, 0) * m(3, 3) - m(2, 3) * m(3, 0);
  const double A0223 = m(2, 0) * m(3, 2) - m(2, 2) * m(3, 0);
  const double A0123 = m(2, 0) * m(3, 1) - m(2, 1) * m(3, 0);
  const double A2313 = m(1, 2) * m(3, 3) - m(1, 3) * m(3, 2);
  const double A1313 = m(1, 1) * m(3, 3) - m(1, 3) * m(3, 1);
  const double A1213 = m(1, 1) * m(3, 2) - m(1, 2) * m(3, 1);
  const double A2312 = m(1, 2) * m(2, 3) - m(1, 3) * m(2, 2);
  const double A1312 = m(1, 1) * m(2, 3) - m(1, 3) * m(2, 1);
  const double A1212 = m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1);
  const double A0313 = m(1, 0) * m(3, 3) - m(1, 3) * m(3, 0);
  const double A0213 = m(1, 0) * m(3, 2) - m(1, 2) * m(3, 0);
  const double A0312 = m(1, 0) * m(2, 3) - m(1, 3) * m(2, 0);
  const double A0212 = m(1, 0) * m(2, 2) - m(1, 2) * m(2, 0);
  const double A0113 = m(1, 0) * m(3, 1) - m(1, 1) * m(3, 0);
  const double A0112 = m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0);

  minv(0, 0) = invdet * (m(1, 1) * A2323 - m(1, 2) * A1323 + m(1, 3) * A1223);
  minv(0, 1) = invdet * -(m(0, 1) * A2323 - m(0, 2) * A1323 + m(0, 3) * A1223);
  minv(0, 2) = invdet * (m(0, 1) * A2313 - m(0, 2) * A1313 + m(0, 3) * A1213);
  minv(0, 3) = invdet * -(m(0, 1) * A2312 - m(0, 2) * A1312 + m(0, 3) * A1212);
  minv(1, 0) = invdet * -(m(1, 0) * A2323 - m(1, 2) * A0323 + m(1, 3) * A0223);
  minv(1, 1) = invdet * (m(0, 0) * A2323 - m(0, 2) * A0323 + m(0, 3) * A0223);
  minv(1, 2) = invdet * -(m(0, 0) * A2313 - m(0, 2) * A0313 + m(0, 3) * A0213);
  minv(1, 3) = invdet * (m(0, 0) * A2312 - m(0, 2) * A0312 + m(0, 3) * A0212);
  minv(2, 0) = invdet * (m(1, 0) * A1323 - m(1, 1) * A0323 + m(1, 3) * A0123);
  minv(2, 1) = invdet * -(m(0, 0) * A1323 - m(0, 1) * A0323 + m(0, 3) * A0123);
  minv(2, 2) = invdet * (m(0, 0) * A1313 - m(0, 1) * A0313 + m(0, 3) * A0113);
  minv(2, 3) = invdet * -(m(0, 0) * A1312 - m(0, 1) * A0312 + m(0, 3) * A0112);
  minv(3, 0) = invdet * -(m(1, 0) * A1223 - m(1, 1) * A0223 + m(1, 2) * A0123);
  minv(3, 1) = invdet * (m(0, 0) * A1223 - m(0, 1) * A0223 + m(0, 2) * A0123);
  minv(3, 2) = invdet * -(m(0, 0) * A1213 - m(0, 1) * A0213 + m(0, 2) * A0113);
  minv(3, 3) = invdet * (m(0, 0) * A1212 - m(0, 1) * A0212 + m(0, 2) * A0112);

  return result;
}

Matrix44 Matrix44::Transposed() const
{
  Matrix44 result;
  result.data[0] = data[0];
  result.data[1] = data[4];
  result.data[2] = data[8];
  result.data[3] = data[12];
  result.data[4] = data[1];
  result.data[5] = data[5];
  result.data[6] = data[9];
  result.data[7] = data[13];
  result.data[8] = data[2];
  result.data[9] = data[6];
  result.data[10] = data[10];
  result.data[11] = data[14];
  result.data[12] = data[3];
  result.data[13] = data[7];
  result.data[14] = data[11];
  result.data[15] = data[15];
  return result;
}

}  // namespace Common
