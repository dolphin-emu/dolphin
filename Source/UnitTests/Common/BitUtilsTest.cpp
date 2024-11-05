// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"

TEST(BitUtils, BitSize)
{
  EXPECT_EQ(Common::BitSize<s8>(), 8u);
  EXPECT_EQ(Common::BitSize<s16>(), 16u);
  EXPECT_EQ(Common::BitSize<s32>(), 32u);
  EXPECT_EQ(Common::BitSize<s64>(), 64u);

  EXPECT_EQ(Common::BitSize<u8>(), 8u);
  EXPECT_EQ(Common::BitSize<u16>(), 16u);
  EXPECT_EQ(Common::BitSize<u32>(), 32u);
  EXPECT_EQ(Common::BitSize<u64>(), 64u);
}

TEST(BitUtils, ExtractBit)
{
  constexpr s32 zero = 0;
  EXPECT_EQ(Common::ExtractBit<0>(zero), 0);

  constexpr s32 one = 1;
  EXPECT_EQ(Common::ExtractBit<0>(one), 1);

  constexpr s32 negative_one = -1;
  EXPECT_EQ(Common::ExtractBit<31>(negative_one), 1);

  constexpr s32 one_hundred_twenty_eight = 0b10000000;
  EXPECT_EQ(Common::ExtractBit<7>(one_hundred_twenty_eight), 1);
}

TEST(BitUtils, ExtractBits)
{
  // Note: Parenthesizing is necessary to prevent the macros from
  //       mangling the template function usages.

  constexpr s32 two_hundred_four_signed = 0b0011001100;
  EXPECT_EQ((Common::ExtractBits<2, 3>(two_hundred_four_signed)), 3u);
  EXPECT_EQ((Common::ExtractBits<2, 7>(two_hundred_four_signed)), 51u);
  EXPECT_EQ((Common::ExtractBits<3, 6>(two_hundred_four_signed)), 9u);

  constexpr u32 two_hundred_four_unsigned = 0b0011001100;
  EXPECT_EQ((Common::ExtractBits<2, 3>(two_hundred_four_unsigned)), 3u);
  EXPECT_EQ((Common::ExtractBits<2, 7>(two_hundred_four_unsigned)), 51u);
  EXPECT_EQ((Common::ExtractBits<3, 6>(two_hundred_four_unsigned)), 9u);

  // Ensure bit extraction remains sign-independent even when signed types are used.
  constexpr s32 negative_one = -1;
  EXPECT_EQ((Common::ExtractBits<0, 31>(negative_one)), 0xFFFFFFFFU);

  // Ensure bit extraction with type overriding works as expected
  EXPECT_EQ((Common::ExtractBits<0, 31, s32, s32>(negative_one)), -1);
}

TEST(BitUtils, IsValidLowMask)
{
  EXPECT_TRUE(Common::IsValidLowMask(0b0u));
  EXPECT_TRUE(Common::IsValidLowMask(0b1u));
  EXPECT_FALSE(Common::IsValidLowMask(0b10u));
  EXPECT_TRUE(Common::IsValidLowMask(0b11u));
  EXPECT_FALSE(Common::IsValidLowMask(0b1110u));
  EXPECT_TRUE(Common::IsValidLowMask(0b1111u));
  EXPECT_FALSE(Common::IsValidLowMask(0b10000u));
  EXPECT_FALSE(Common::IsValidLowMask(0b101111u));

  EXPECT_TRUE(Common::IsValidLowMask((u8)~0b0));
  EXPECT_FALSE(Common::IsValidLowMask((u8)(~0b0 - 1)));
  EXPECT_FALSE(Common::IsValidLowMask((u8) ~(0b10000)));
  EXPECT_FALSE(Common::IsValidLowMask((u8)(~((u8)(~0b0) >> 1) | 0b1111)));

  EXPECT_TRUE(Common::IsValidLowMask((u16)~0b0));
  EXPECT_FALSE(Common::IsValidLowMask((u16)(~0b0 - 1)));
  EXPECT_FALSE(Common::IsValidLowMask((u16) ~(0b10000)));
  EXPECT_FALSE(Common::IsValidLowMask((u16)(~((u16)(~0b0) >> 1) | 0b1111)));

  EXPECT_TRUE(Common::IsValidLowMask((u32)~0b0));
  EXPECT_FALSE(Common::IsValidLowMask((u32)(~0b0 - 1)));
  EXPECT_FALSE(Common::IsValidLowMask((u32) ~(0b10000)));
  EXPECT_FALSE(Common::IsValidLowMask((u32)(~((u32)(~0b0) >> 1) | 0b1111)));

  EXPECT_TRUE(Common::IsValidLowMask((u64)~0b0));
  EXPECT_FALSE(Common::IsValidLowMask((u64)(~0b0 - 1)));
  EXPECT_FALSE(Common::IsValidLowMask((u64) ~(0b10000)));
  EXPECT_FALSE(Common::IsValidLowMask((u64)(~((u64)(~0b0) >> 1) | 0b1111)));
}
