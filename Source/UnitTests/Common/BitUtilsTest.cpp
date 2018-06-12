// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

TEST(BitUtils, RotateLeft)
{
  EXPECT_EQ(Common::RotateLeft(0xF0F0F0F0, 0), 0xF0F0F0F0U);
  EXPECT_EQ(Common::RotateLeft(0xF0F0F0F0, 4), 0x0F0F0F0FU);
  EXPECT_EQ(Common::RotateLeft(0xF0F0F0F0, 8), 0xF0F0F0F0U);
  EXPECT_EQ(Common::RotateLeft(0xF0F0F0F0F0F0F0F0, 0), 0xF0F0F0F0F0F0F0F0U);
  EXPECT_EQ(Common::RotateLeft(0xF0F0F0F0F0F0F0F0, 4), 0x0F0F0F0F0F0F0F0FU);
  EXPECT_EQ(Common::RotateLeft(0xF0F0F0F0F0F0F0F0, 8), 0xF0F0F0F0F0F0F0F0U);

  EXPECT_EQ(Common::RotateLeft(0xF1F1F1F1, 1), 0xE3E3E3E3U);
  EXPECT_EQ(Common::RotateLeft(0xF1F1F1F1, 2), 0xC7C7C7C7U);
  EXPECT_EQ(Common::RotateLeft(0xF1F1F1F1, 3), 0x8F8F8F8FU);
  EXPECT_EQ(Common::RotateLeft(0xF1F1F1F1, 4), 0x1F1F1F1FU);
  EXPECT_EQ(Common::RotateLeft(0xF1F1F1F1F1F1F1F1, 1), 0xE3E3E3E3E3E3E3E3U);
  EXPECT_EQ(Common::RotateLeft(0xF1F1F1F1F1F1F1F1, 2), 0xC7C7C7C7C7C7C7C7U);
  EXPECT_EQ(Common::RotateLeft(0xF1F1F1F1F1F1F1F1, 3), 0x8F8F8F8F8F8F8F8FU);
  EXPECT_EQ(Common::RotateLeft(0xF1F1F1F1F1F1F1F1, 4), 0x1F1F1F1F1F1F1F1FU);
}

TEST(BitUtils, RotateRight)
{
  EXPECT_EQ(Common::RotateRight(0xF0F0F0F0, 0), 0xF0F0F0F0U);
  EXPECT_EQ(Common::RotateRight(0xF0F0F0F0, 4), 0x0F0F0F0FU);
  EXPECT_EQ(Common::RotateRight(0xF0F0F0F0, 8), 0xF0F0F0F0U);
  EXPECT_EQ(Common::RotateRight(0xF0F0F0F0F0F0F0F0, 0), 0xF0F0F0F0F0F0F0F0U);
  EXPECT_EQ(Common::RotateRight(0xF0F0F0F0F0F0F0F0, 4), 0x0F0F0F0F0F0F0F0FU);
  EXPECT_EQ(Common::RotateRight(0xF0F0F0F0F0F0F0F0, 8), 0xF0F0F0F0F0F0F0F0U);

  EXPECT_EQ(Common::RotateRight(0xF1F1F1F1, 1), 0xF8F8F8F8U);
  EXPECT_EQ(Common::RotateRight(0xF1F1F1F1, 2), 0x7C7C7C7CU);
  EXPECT_EQ(Common::RotateRight(0xF1F1F1F1, 3), 0x3E3E3E3EU);
  EXPECT_EQ(Common::RotateRight(0xF1F1F1F1, 4), 0x1F1F1F1FU);
  EXPECT_EQ(Common::RotateRight(0xF1F1F1F1F1F1F1F1, 1), 0xF8F8F8F8F8F8F8F8U);
  EXPECT_EQ(Common::RotateRight(0xF1F1F1F1F1F1F1F1, 2), 0x7C7C7C7C7C7C7C7CU);
  EXPECT_EQ(Common::RotateRight(0xF1F1F1F1F1F1F1F1, 3), 0x3E3E3E3E3E3E3E3EU);
  EXPECT_EQ(Common::RotateRight(0xF1F1F1F1F1F1F1F1, 4), 0x1F1F1F1F1F1F1F1FU);
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

TEST(BitUtils, BitCast)
{
  EXPECT_EQ(0x00000000U, Common::BitCast<u32>(0.0f));
  EXPECT_EQ(0x80000000U, Common::BitCast<u32>(-0.0f));
  EXPECT_EQ(0x3F800000U, Common::BitCast<u32>(1.0f));
  EXPECT_EQ(0xBF800000U, Common::BitCast<u32>(-1.0f));

  EXPECT_EQ(0x0000000000000000ULL, Common::BitCast<u64>(0.0));
  EXPECT_EQ(0x8000000000000000ULL, Common::BitCast<u64>(-0.0));
  EXPECT_EQ(0x3FF0000000000000ULL, Common::BitCast<u64>(1.0));
  EXPECT_EQ(0xBFF0000000000000ULL, Common::BitCast<u64>(-1.0));
}
