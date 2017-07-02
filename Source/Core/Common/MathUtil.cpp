// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"

namespace MathUtil
{
u32 ClassifyDouble(double dvalue)
{
  // TODO: Optimize the below to be as fast as possible.
  IntDouble value(dvalue);
  u64 sign = value.i & DOUBLE_SIGN;
  u64 exp = value.i & DOUBLE_EXP;
  if (exp > DOUBLE_ZERO && exp < DOUBLE_EXP)
  {
    // Nice normalized number.
    return sign ? PPC_FPCLASS_NN : PPC_FPCLASS_PN;
  }
  else
  {
    u64 mantissa = value.i & DOUBLE_FRAC;
    if (mantissa)
    {
      if (exp)
      {
        return PPC_FPCLASS_QNAN;
      }
      else
      {
        // Denormalized number.
        return sign ? PPC_FPCLASS_ND : PPC_FPCLASS_PD;
      }
    }
    else if (exp)
    {
      // Infinite
      return sign ? PPC_FPCLASS_NINF : PPC_FPCLASS_PINF;
    }
    else
    {
      // Zero
      return sign ? PPC_FPCLASS_NZ : PPC_FPCLASS_PZ;
    }
  }
}

u32 ClassifyFloat(float fvalue)
{
  // TODO: Optimize the below to be as fast as possible.
  IntFloat value(fvalue);
  u32 sign = value.i & FLOAT_SIGN;
  u32 exp = value.i & FLOAT_EXP;
  if (exp > FLOAT_ZERO && exp < FLOAT_EXP)
  {
    // Nice normalized number.
    return sign ? PPC_FPCLASS_NN : PPC_FPCLASS_PN;
  }
  else
  {
    u32 mantissa = value.i & FLOAT_FRAC;
    if (mantissa)
    {
      if (exp)
      {
        return PPC_FPCLASS_QNAN;  // Quiet NAN
      }
      else
      {
        // Denormalized number.
        return sign ? PPC_FPCLASS_ND : PPC_FPCLASS_PD;
      }
    }
    else if (exp)
    {
      // Infinite
      return sign ? PPC_FPCLASS_NINF : PPC_FPCLASS_PINF;
    }
    else
    {
      // Zero
      return sign ? PPC_FPCLASS_NZ : PPC_FPCLASS_PZ;
    }
  }
}

const std::array<BaseAndDec, 32> frsqrte_expected = {{
    {0x3ffa000, 0x7a4}, {0x3c29000, 0x700}, {0x38aa000, 0x670}, {0x3572000, 0x5f2},
    {0x3279000, 0x584}, {0x2fb7000, 0x524}, {0x2d26000, 0x4cc}, {0x2ac0000, 0x47e},
    {0x2881000, 0x43a}, {0x2665000, 0x3fa}, {0x2468000, 0x3c2}, {0x2287000, 0x38e},
    {0x20c1000, 0x35e}, {0x1f12000, 0x332}, {0x1d79000, 0x30a}, {0x1bf4000, 0x2e6},
    {0x1a7e800, 0x568}, {0x17cb800, 0x4f3}, {0x1552800, 0x48d}, {0x130c000, 0x435},
    {0x10f2000, 0x3e7}, {0x0eff000, 0x3a2}, {0x0d2e000, 0x365}, {0x0b7c000, 0x32e},
    {0x09e5000, 0x2fc}, {0x0867000, 0x2d0}, {0x06ff000, 0x2a8}, {0x05ab800, 0x283},
    {0x046a000, 0x261}, {0x0339800, 0x243}, {0x0218800, 0x226}, {0x0105800, 0x20b},
}};

double ApproximateReciprocalSquareRoot(double val)
{
  union
  {
    double valf;
    s64 vali;
  };
  valf = val;
  s64 mantissa = vali & ((1LL << 52) - 1);
  s64 sign = vali & (1ULL << 63);
  s64 exponent = vali & (0x7FFLL << 52);

  // Special case 0
  if (mantissa == 0 && exponent == 0)
    return sign ? -std::numeric_limits<double>::infinity() :
                  std::numeric_limits<double>::infinity();
  // Special case NaN-ish numbers
  if (exponent == (0x7FFLL << 52))
  {
    if (mantissa == 0)
    {
      if (sign)
        return std::numeric_limits<double>::quiet_NaN();

      return 0.0;
    }

    return 0.0 + valf;
  }

  // Negative numbers return NaN
  if (sign)
    return std::numeric_limits<double>::quiet_NaN();

  if (!exponent)
  {
    // "Normalize" denormal values
    do
    {
      exponent -= 1LL << 52;
      mantissa <<= 1;
    } while (!(mantissa & (1LL << 52)));
    mantissa &= (1LL << 52) - 1;
    exponent += 1LL << 52;
  }

  bool odd_exponent = !(exponent & (1LL << 52));
  exponent = ((0x3FFLL << 52) - ((exponent - (0x3FELL << 52)) / 2)) & (0x7FFLL << 52);

  int i = (int)(mantissa >> 37);
  vali = sign | exponent;
  int index = i / 2048 + (odd_exponent ? 16 : 0);
  const auto& entry = frsqrte_expected[index];
  vali |= (s64)(entry.m_base - entry.m_dec * (i % 2048)) << 26;
  return valf;
}

const std::array<BaseAndDec, 32> fres_expected = {{
    {0x7ff800, 0x3e1}, {0x783800, 0x3a7}, {0x70ea00, 0x371}, {0x6a0800, 0x340}, {0x638800, 0x313},
    {0x5d6200, 0x2ea}, {0x579000, 0x2c4}, {0x520800, 0x2a0}, {0x4cc800, 0x27f}, {0x47ca00, 0x261},
    {0x430800, 0x245}, {0x3e8000, 0x22a}, {0x3a2c00, 0x212}, {0x360800, 0x1fb}, {0x321400, 0x1e5},
    {0x2e4a00, 0x1d1}, {0x2aa800, 0x1be}, {0x272c00, 0x1ac}, {0x23d600, 0x19b}, {0x209e00, 0x18b},
    {0x1d8800, 0x17c}, {0x1a9000, 0x16e}, {0x17ae00, 0x15b}, {0x14f800, 0x15b}, {0x124400, 0x143},
    {0x0fbe00, 0x143}, {0x0d3800, 0x12d}, {0x0ade00, 0x12d}, {0x088400, 0x11a}, {0x065000, 0x11a},
    {0x041c00, 0x108}, {0x020c00, 0x106},
}};

// Used by fres and ps_res.
double ApproximateReciprocal(double val)
{
  // We are using namespace std scoped here because the Android NDK is complete trash as usual
  // For 32bit targets(mips, ARMv7, x86) it doesn't provide an implementation of std::copysign
  // but instead provides just global namespace copysign implementations.
  // The workaround for this is to just use namespace std within this function's scope
  // That way on real toolchains it will use the std:: variant like normal.
  using namespace std;
  union
  {
    double valf;
    s64 vali;
  };

  valf = val;
  s64 mantissa = vali & ((1LL << 52) - 1);
  s64 sign = vali & (1ULL << 63);
  s64 exponent = vali & (0x7FFLL << 52);

  // Special case 0
  if (mantissa == 0 && exponent == 0)
    return copysign(std::numeric_limits<double>::infinity(), valf);

  // Special case NaN-ish numbers
  if (exponent == (0x7FFLL << 52))
  {
    if (mantissa == 0)
      return copysign(0.0, valf);
    return 0.0 + valf;
  }

  // Special case small inputs
  if (exponent < (895LL << 52))
    return copysign(std::numeric_limits<float>::max(), valf);

  // Special case large inputs
  if (exponent >= (1149LL << 52))
    return copysign(0.0, valf);

  exponent = (0x7FDLL << 52) - exponent;

  int i = (int)(mantissa >> 37);
  const auto& entry = fres_expected[i / 1024];
  vali = sign | exponent;
  vali |= (s64)(entry.m_base - (entry.m_dec * (i % 1024) + 1) / 2) << 29;
  return valf;
}

}  // namespace

inline void MatrixMul(int n, const float* a, const float* b, float* result)
{
  for (int i = 0; i < n; ++i)
  {
    for (int j = 0; j < n; ++j)
    {
      float temp = 0;
      for (int k = 0; k < n; ++k)
      {
        temp += a[i * n + k] * b[k * n + j];
      }
      result[i * n + j] = temp;
    }
  }
}

// Calculate sum of a float list
float MathFloatVectorSum(const std::vector<float>& Vec)
{
  return std::accumulate(Vec.begin(), Vec.end(), 0.0f);
}

void Matrix33::LoadIdentity(Matrix33& mtx)
{
  memset(mtx.data, 0, sizeof(mtx.data));
  mtx.data[0] = 1.0f;
  mtx.data[4] = 1.0f;
  mtx.data[8] = 1.0f;
}

void Matrix33::RotateX(Matrix33& mtx, float rad)
{
  float s = sin(rad);
  float c = cos(rad);
  memset(mtx.data, 0, sizeof(mtx.data));
  mtx.data[0] = 1;
  mtx.data[4] = c;
  mtx.data[5] = -s;
  mtx.data[7] = s;
  mtx.data[8] = c;
}
void Matrix33::RotateY(Matrix33& mtx, float rad)
{
  float s = sin(rad);
  float c = cos(rad);
  memset(mtx.data, 0, sizeof(mtx.data));
  mtx.data[0] = c;
  mtx.data[2] = s;
  mtx.data[4] = 1;
  mtx.data[6] = -s;
  mtx.data[8] = c;
}

void Matrix33::Multiply(const Matrix33& a, const Matrix33& b, Matrix33& result)
{
  MatrixMul(3, a.data, b.data, result.data);
}

void Matrix33::Multiply(const Matrix33& a, const float vec[3], float result[3])
{
  for (int i = 0; i < 3; ++i)
  {
    result[i] = 0;

    for (int k = 0; k < 3; ++k)
    {
      result[i] += a.data[i * 3 + k] * vec[k];
    }
  }
}

void Matrix44::LoadIdentity(Matrix44& mtx)
{
  memset(mtx.data, 0, sizeof(mtx.data));
  mtx.data[0] = 1.0f;
  mtx.data[5] = 1.0f;
  mtx.data[10] = 1.0f;
  mtx.data[15] = 1.0f;
}

void Matrix44::LoadMatrix33(Matrix44& mtx, const Matrix33& m33)
{
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
}

void Matrix44::Set(Matrix44& mtx, const float mtxArray[16])
{
  for (int i = 0; i < 16; ++i)
  {
    mtx.data[i] = mtxArray[i];
  }
}

void Matrix44::Translate(Matrix44& mtx, const float vec[3])
{
  LoadIdentity(mtx);
  mtx.data[3] = vec[0];
  mtx.data[7] = vec[1];
  mtx.data[11] = vec[2];
}

void Matrix44::Shear(Matrix44& mtx, const float a, const float b)
{
  LoadIdentity(mtx);
  mtx.data[2] = a;
  mtx.data[6] = b;
}

void Matrix44::Multiply(const Matrix44& a, const Matrix44& b, Matrix44& result)
{
  MatrixMul(4, a.data, b.data, result.data);
}
