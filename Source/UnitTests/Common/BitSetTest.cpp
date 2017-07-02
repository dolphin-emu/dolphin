// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <gtest/gtest.h>

#include "Common/BitSet.h"

TEST(BitSet, Basics)
{
  BitSet32 bs;
  BitSet64 bs2(1);
  BitSet64 bs3(2);
  EXPECT_TRUE(!!bs2);
  EXPECT_FALSE(!!bs);
  EXPECT_EQ(bs2, bs2);
  EXPECT_NE(bs2, bs3);
  EXPECT_EQ(BitSet32(0xfff), BitSet32::AllTrue(12));
  EXPECT_EQ(BitSet64(0xffffffffffffffff), BitSet64::AllTrue(64));
}

TEST(BitSet, BitGetSet)
{
  BitSet32 bs;
  bs[3] = bs[8] = bs[11] = true;
  EXPECT_TRUE(bs[3]);
  EXPECT_FALSE(bs[4]);
  EXPECT_EQ((u32)((1 << 3) | (1 << 8) | (1 << 11)), bs.m_val);
}

TEST(BitSet, Count)
{
  u32 random_numbers[] = {0x2cb0b5f3, 0x81ab32a6, 0xd9030dc5, 0x325ffe26, 0xb2fcaee3,
                          0x4ccf188a, 0xf8be36dc, 0xb2fcecd5, 0xb750c2e5, 0x31d19074,
                          0xf267644a, 0xac00a719, 0x6d45f19b, 0xf7e91c5b, 0xf687e694,
                          0x9057c24e, 0x5eb65c39, 0x85d3038b, 0x101f4e66, 0xc202d136};
  u32 counts[] = {17, 14, 14, 19, 20, 14, 20, 20, 16, 13, 16, 12, 18, 20, 18, 14, 18, 14, 14, 12};
  for (size_t i = 0; i < 20; i++)
  {
    EXPECT_EQ(counts[i], BitSet32(random_numbers[i]).Count());
  }

  u64 random_numbers_64[] = {0xf86cd6f6ef09d7d4ULL, 0x6f2d8533255ead3cULL, 0x9da7941e0e52b345ULL,
                             0x06e4189be67d2b17ULL, 0x3eb0681f65cb6d25ULL, 0xccab8a7c74a51203ULL,
                             0x09d470516694c64bULL, 0x38cd077e075c778fULL, 0xd69ebfa6355ebfdeULL};
  u32 counts_64[] = {39, 34, 31, 32, 33, 29, 27, 35, 43};
  for (size_t i = 0; i < 9; i++)
  {
    EXPECT_EQ(counts_64[i], BitSet64(random_numbers_64[i]).Count());
  }
}

TEST(BitSet, BitOps)
{
  BitSet32 a(3), b(5), c;
  EXPECT_EQ(BitSet32(7), a | b);
  EXPECT_EQ(BitSet32(6), a ^ b);
  EXPECT_EQ(BitSet32(1), a & b);
  EXPECT_EQ(BitSet32(0xfffffffc), ~a);
  c = a;
  c |= b;
  EXPECT_EQ(BitSet32(7), c);
  c = a;
  c ^= b;
  EXPECT_EQ(BitSet32(6), c);
  c = a;
  c &= b;
  EXPECT_EQ(BitSet32(1), c);
}

TEST(BitSet, InitializerListsAndIteration)
{
  std::vector<int> bits{1, 10, 15, 17, 20, 30};
  BitSet32 bs{1, 10, 15, 17, 20, 30};
  auto vit = bits.begin();
  for (auto i : bs)
  {
    EXPECT_NE(vit, bits.end());
    EXPECT_EQ(i, *vit++);
  }
  EXPECT_EQ(vit, bits.end());
}
