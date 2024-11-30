// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <utility>

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
  constexpr std::array<std::pair<u32, u32>, 20> random_32bit_number_bitcount_pairs = {
      {{0x2cb0b5f3, 17}, {0x81ab32a6, 14}, {0xd9030dc5, 14}, {0x325ffe26, 19}, {0xb2fcaee3, 20},
       {0x4ccf188a, 14}, {0xf8be36dc, 20}, {0xb2fcecd5, 20}, {0xb750c2e5, 16}, {0x31d19074, 13},
       {0xf267644a, 16}, {0xac00a719, 12}, {0x6d45f19b, 18}, {0xf7e91c5b, 20}, {0xf687e694, 18},
       {0x9057c24e, 14}, {0x5eb65c39, 18}, {0x85d3038b, 14}, {0x101f4e66, 14}, {0xc202d136, 12}}};
  for (const auto& [number, bitcount] : random_32bit_number_bitcount_pairs)
  {
    const auto bitset = BitSet32(number);
    EXPECT_EQ(bitset.Count(), bitcount);
    u32 iterating_count = 0;
    for (auto iter = bitset.begin(); iter != bitset.end(); ++iter)
      ++iterating_count;
    EXPECT_EQ(iterating_count, bitcount);
  }

  constexpr std::array<std::pair<u64, u32>, 9> random_64bit_number_bitcount_pairs = {
      {{0xf86cd6f6ef09d7d4ULL, 39},
       {0x6f2d8533255ead3cULL, 34},
       {0x9da7941e0e52b345ULL, 31},
       {0x06e4189be67d2b17ULL, 32},
       {0x3eb0681f65cb6d25ULL, 33},
       {0xccab8a7c74a51203ULL, 29},
       {0x09d470516694c64bULL, 27},
       {0x38cd077e075c778fULL, 35},
       {0xd69ebfa6355ebfdeULL, 43}}};
  for (const auto& [number, bitcount] : random_64bit_number_bitcount_pairs)
  {
    const auto bitset = BitSet64(number);
    EXPECT_EQ(bitset.Count(), bitcount);
    u32 iterating_count = 0;
    for (auto iter = bitset.begin(); iter != bitset.end(); ++iter)
      ++iterating_count;
    EXPECT_EQ(iterating_count, bitcount);
  }
}

TEST(BitSet, BitOps)
{
  BitSet32 a(3), b(5);
  EXPECT_EQ(BitSet32(7), a | b);
  EXPECT_EQ(BitSet32(6), a ^ b);
  EXPECT_EQ(BitSet32(1), a & b);
  EXPECT_EQ(BitSet32(0xfffffffc), ~a);
  BitSet32 c = a;
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
