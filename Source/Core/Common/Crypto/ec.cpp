// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Copyright 2007,2008  Segher Boessenkool  <segher@kernel.crashing.org>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#include <algorithm>
#include <cstring>

#include "Common/Compiler.h"
#include "Common/Crypto/bn.h"
#include "Common/Crypto/ec.h"
#include "Common/Random.h"
#include "Common/StringUtil.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4505)
#endif

namespace Common::ec
{
static const u8 square[16] = {0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15,
                              0x40, 0x41, 0x44, 0x45, 0x50, 0x51, 0x54, 0x55};

struct Elt;
static Elt operator*(const Elt& a, const Elt& b);

struct Elt
{
  bool IsZero() const
  {
    return std::all_of(data.begin(), data.end(), [](u8 b) { return b == 0; });
  }

  void MulX()
  {
    u8 carry = data[0] & 1;
    u8 x = 0;
    for (std::size_t i = 0; i < data.size() - 1; i++)
    {
      u8 y = data[i + 1];
      data[i] = x ^ (y >> 7);
      x = y << 1;
    }
    data[29] = x ^ carry;
    data[20] ^= carry << 2;
  }

  Elt Square() const
  {
    std::array<u8, 60> wide;
    for (std::size_t i = 0; i < data.size(); i++)
    {
      wide[2 * i] = square[data[i] >> 4];
      wide[2 * i + 1] = square[data[i] & 15];
    }
    for (std::size_t i = 0; i < data.size(); i++)
    {
      u8 x = wide[i];

      wide[i + 19] ^= x >> 7;
      wide[i + 20] ^= x << 1;

      wide[i + 29] ^= x >> 1;
      wide[i + 30] ^= x << 7;
    }

    u8 x = wide[30] & ~1;
    wide[49] ^= x >> 7;
    wide[50] ^= x << 1;
    wide[59] ^= x >> 1;
    wide[30] &= 1;

    Elt result;
    std::copy(wide.cbegin() + 30, wide.cend(), result.data.begin());
    return result;
  }

  Elt ItohTsujii(const Elt& b, std::size_t j) const
  {
    Elt t = *this;
    while (j--)
      t = t.Square();
    return t * b;
  }

  Elt Inv() const
  {
    Elt t = ItohTsujii(*this, 1);
    Elt s = t.ItohTsujii(*this, 1);
    t = s.ItohTsujii(s, 3);
    s = t.ItohTsujii(*this, 1);
    t = s.ItohTsujii(s, 7);
    s = t.ItohTsujii(t, 14);
    t = s.ItohTsujii(*this, 1);
    s = t.ItohTsujii(t, 29);
    t = s.ItohTsujii(s, 58);
    s = t.ItohTsujii(t, 116);
    return s.Square();
  }

  std::array<u8, 30> data{};
};

static Elt operator+(const Elt& a, const Elt& b)
{
  Elt d;
  for (std::size_t i = 0; i < std::tuple_size<decltype(Elt::data)>{}; i++)
    d.data[i] = a.data[i] ^ b.data[i];
  return d;
}

static Elt operator*(const Elt& a, const Elt& b)
{
  Elt d;
  std::size_t i = 0;
  u8 mask = 1;
  for (std::size_t n = 0; n < 233; n++)
  {
    d.MulX();

    if ((a.data[i] & mask) != 0)
      d = d + b;

    mask >>= 1;
    if (mask == 0)
    {
      mask = 0x80;
      i++;
    }
  }
  return d;
}

static Elt operator/(const Elt& dividend, const Elt& divisor)
{
  return dividend * divisor.Inv();
}

struct Point
{
  Point() = default;
  constexpr explicit Point(Elt x, Elt y) : m_data{{std::move(x), std::move(y)}} {}
  explicit Point(const u8* data) { std::copy_n(data, sizeof(m_data), Data()); }

  bool IsZero() const { return X().IsZero() && Y().IsZero(); }
  Elt& X() { return m_data[0]; }
  Elt& Y() { return m_data[1]; }
  u8* Data() { return m_data[0].data.data(); }
  const Elt& X() const { return m_data[0]; }
  const Elt& Y() const { return m_data[1]; }
  const u8* Data() const { return m_data[0].data.data(); }

  Point Double() const
  {
    Point r;
    if (X().IsZero())
      return r;

    const auto s = Y() / X() + X();
    r.X() = s.Square() + s;
    r.X().data[29] ^= 1;
    r.Y() = s * r.X() + r.X() + X().Square();
    return r;
  }

private:
  std::array<Elt, 2> m_data{};
  static_assert(sizeof(decltype(m_data)) == 60, "Wrong size for m_data");
};

// y**2 + x*y = x**3 + x + b
[[maybe_unused]] static const u8 ec_b[30] = {
    0x00, 0x66, 0x64, 0x7e, 0xde, 0x6c, 0x33, 0x2c, 0x7f, 0x8c, 0x09, 0x23, 0xbb, 0x58, 0x21,
    0x3b, 0x33, 0x3b, 0x20, 0xe9, 0xce, 0x42, 0x81, 0xfe, 0x11, 0x5f, 0x7d, 0x8f, 0x90, 0xad};

// order of the addition group of points
static const u8 ec_N[30] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0xe9, 0x74, 0xe7, 0x2f,
                            0x8a, 0x69, 0x22, 0x03, 0x1d, 0x26, 0x03, 0xcf, 0xe0, 0xd7};

// base point
constexpr Point ec_G{
    {{{0x00, 0xfa, 0xc9, 0xdf, 0xcb, 0xac, 0x83, 0x13, 0xbb, 0x21, 0x39, 0xf1, 0xbb, 0x75, 0x5f,
       0xef, 0x65, 0xbc, 0x39, 0x1f, 0x8b, 0x36, 0xf8, 0xf8, 0xeb, 0x73, 0x71, 0xfd, 0x55, 0x8b}}},
    {{{0x01, 0x00, 0x6a, 0x08, 0xa4, 0x19, 0x03, 0x35, 0x06, 0x78, 0xe5, 0x85, 0x28, 0xbe, 0xbf,
       0x8a, 0x0b, 0xef, 0xf8, 0x67, 0xa7, 0xca, 0x36, 0x71, 0x6f, 0x7e, 0x01, 0xf8, 0x10, 0x52}}}};

static Point operator+(const Point& a, const Point& b)
{
  if (a.IsZero())
    return b;
  if (b.IsZero())
    return a;

  Elt u = a.X() + b.X();
  if (u.IsZero())
  {
    u = a.Y() + b.Y();
    if (u.IsZero())
      return a.Double();
    return Point{};
  }

  const Elt s = (a.Y() + b.Y()) / u;
  Elt t = s.Square() + s + b.X();
  t.data[29] ^= 1;

  const Elt rx = t + a.X();
  const Elt ry = s * t + a.Y() + rx;
  return Point{rx, ry};
}

static Point operator*(const u8* a, const Point& b)
{
  Point d;
  for (std::size_t i = 0; i < 30; i++)
  {
    for (u8 mask = 0x80; mask != 0; mask >>= 1)
    {
      d = d.Double();
      if ((a[i] & mask) != 0)
        d = d + b;
    }
  }
  return d;
}

Signature Sign(const u8* key, const u8* hash)
{
  u8 e[30]{};
  memcpy(e + 10, hash, 20);

  u8 m[30];
  do
  {
    // Generate 240 bits and keep 233.
    Common::Random::Generate(m, sizeof(m));
    m[0] &= 1;
  } while (bn_compare(m, ec_N, sizeof(m)) >= 0);

  Elt r = (m * ec_G).X();
  if (bn_compare(r.data.data(), ec_N, 30) >= 0)
    bn_sub_modulus(r.data.data(), ec_N, 30);

  //	S = m**-1*(e + Rk) (mod N)

  u8 kk[30];
  std::copy_n(key, sizeof(kk), kk);
  if (bn_compare(kk, ec_N, sizeof(kk)) >= 0)
    bn_sub_modulus(kk, ec_N, sizeof(kk));
  Elt s;
  bn_mul(s.data.data(), r.data.data(), kk, ec_N, 30);
  bn_add(kk, s.data.data(), e, ec_N, sizeof(kk));
  u8 minv[30];
  bn_inv(minv, m, ec_N, sizeof(minv));
  bn_mul(s.data.data(), minv, kk, ec_N, 30);

  Signature signature;
  std::copy(r.data.cbegin(), r.data.cend(), signature.begin());
  std::copy(s.data.cbegin(), s.data.cend(), signature.begin() + 30);
  return signature;
}

bool VerifySignature(const u8* public_key, const u8* signature, const u8* hash)
{
  const u8* R = signature;
  const u8* S = signature + 30;
  u8 Sinv[30];

  bn_inv(Sinv, S, ec_N, 30);
  u8 e[30]{};
  memcpy(e + 10, hash, 20);

  u8 w1[30], w2[30];
  bn_mul(w1, e, Sinv, ec_N, 30);
  bn_mul(w2, R, Sinv, ec_N, 30);

  Point r1 = w1 * ec_G + w2 * Point{public_key};
  auto& rx = r1.X().data;
  if (bn_compare(rx.data(), ec_N, 30) >= 0)
    bn_sub_modulus(rx.data(), ec_N, 30);

  return (bn_compare(rx.data(), R, 30) == 0);
}

PublicKey PrivToPub(const u8* key)
{
  const Point data = key * ec_G;
  PublicKey result;
  std::copy_n(data.Data(), result.size(), result.begin());
  return result;
}

std::array<u8, 60> ComputeSharedSecret(const u8* private_key, const u8* public_key)
{
  std::array<u8, 60> shared_secret;
  const Point data = private_key * Point{public_key};
  std::copy_n(data.Data(), shared_secret.size(), shared_secret.begin());
  return shared_secret;
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
}  // namespace Common::ec
