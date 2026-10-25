// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <cstring>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "Common/CommonTypes.h"
#include "Common/Hash.h"

namespace
{
std::vector<u8> RandomBytes(size_t size)
{
  std::mt19937 rng(1);
  std::vector<u8> bytes(size);
  for (u8& byte : bytes)
    byte = static_cast<u8>(rng());
  return bytes;
}
}  // namespace

TEST(Hash, GetHash64DetectsEachBitFlip)
{
  // Hashing all of the data, any change to it changes the hash.
  for (const u32 len : {1u, 7u, 8u, 15u, 16u, 31u, 64u, 255u, 256u})
  {
    std::vector<u8> data = RandomBytes(len);
    const u64 hash = Common::GetHash64(data.data(), len, 0);
    EXPECT_EQ(hash, Common::GetHash64(data.data(), len, 0));
    for (u32 bit = 0; bit < len * 8; bit++)
    {
      data[bit / 8] ^= 1 << (bit % 8);
      EXPECT_NE(hash, Common::GetHash64(data.data(), len, 0)) << "len " << len << " bit " << bit;
      data[bit / 8] ^= 1 << (bit % 8);
    }
  }
}

#if defined(_M_PPC_64) && defined(__CRYPTO__)

namespace
{
// A portable version of GetHash64_PPC64_VPMSUM.
struct U128
{
  u64 lo;
  u64 hi;
};

U128 CarrylessMultiply(u64 a, u64 b)
{
  U128 result{};
  for (int i = 0; i < 64; i++)
  {
    if ((b >> i) & 1)
    {
      result.lo ^= a << i;
      result.hi ^= i == 0 ? 0 : a >> (64 - i);
    }
  }
  return result;
}

U128 Fold(U128 a, U128 b)
{
  const U128 low = CarrylessMultiply(a.lo, 0x05f5c3c7eb52fab6);
  const U128 high = CarrylessMultiply(a.hi, 0x4eb938a7d257740e);
  return {low.lo ^ high.lo ^ b.lo, low.hi ^ high.hi ^ b.hi};
}

U128 Block(const u8* data)
{
  U128 block;
  std::memcpy(&block.lo, data, 8);
  std::memcpy(&block.hi, data + 8, 8);
  return block;
}

u64 Mix(u64 k)
{
  k ^= k >> 33;
  k *= 0xff51afd7ed558ccd;
  k ^= k >> 33;
  k *= 0xc4ceb9fe1a85ec53;
  k ^= k >> 33;
  return k;
}

u64 ReferenceHash64(const u8* src, u32 len, u32 samples)
{
  const u32 nblocks = len / 16;
  u32 step = len / 8;
  if (samples == 0)
    samples = std::max(step, 1u);
  step = std::max(step / samples, 1u);

  std::array<U128, 4> h{{{len, 0}, {0, 0}, {0, 0}, {0, 0}}};
  u32 i = 0;
  if (nblocks > step * 3)
  {
    for (; i < nblocks - step * 3; i += step * 4)
    {
      for (u32 j = 0; j < 4; j++)
        h[j] = Fold(h[j], Block(src + 16 * (i + step * j)));
    }
  }
  for (; i < nblocks; i += step)
    h[0] = Fold(h[0], Block(src + 16 * i));

  if (len & 15)
  {
    std::array<u8, 16> tail{};
    std::memcpy(tail.data(), src + nblocks * 16, len & 15);
    h[1] = Fold(h[1], Block(tail.data()));
  }

  const U128 result = Fold(Fold(Fold(h[0], h[1]), h[2]), h[3]);
  return Mix(result.lo ^ Mix(result.hi));
}
}  // namespace

TEST(Hash, GetHash64PPC64MatchesReference)
{
  const std::vector<u8> data = RandomBytes(65536 + 16);
  for (const u32 samples : {0u, 1u, 3u, 128u, 512u})
  {
    for (u32 len = 0; len < 600; len++)
    {
      for (u32 offset = 0; offset < 3; offset++)
      {
        EXPECT_EQ(ReferenceHash64(data.data() + offset, len, samples),
                  Common::GetHash64(data.data() + offset, len, samples))
            << "len " << len << " samples " << samples << " offset " << offset;
      }
    }
    for (const u32 len : {4096u, 4100u, 65536u})
    {
      EXPECT_EQ(ReferenceHash64(data.data() + 1, len, samples),
                Common::GetHash64(data.data() + 1, len, samples))
          << "len " << len << " samples " << samples;
    }
  }
}

#endif
