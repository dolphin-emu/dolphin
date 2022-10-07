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
  EXPECT_EQ((Common::ExtractBitsU<2, 2>(two_hundred_four_signed)), 3u);
  EXPECT_EQ((Common::ExtractBitsU<6, 2>(two_hundred_four_signed)), 51u);
  EXPECT_EQ((Common::ExtractBitsU<4, 3>(two_hundred_four_signed)), 9u);
  EXPECT_EQ((Common::ExtractBitsU<5, 5>(two_hundred_four_signed)), 6u);
  EXPECT_EQ((Common::ExtractBitsS<2, 2>(two_hundred_four_signed)), -1);
  EXPECT_EQ((Common::ExtractBitsS<6, 2>(two_hundred_four_signed)), -13);
  EXPECT_EQ((Common::ExtractBitsS<4, 3>(two_hundred_four_signed)), -7);
  EXPECT_EQ((Common::ExtractBitsS<5, 5>(two_hundred_four_signed)), 6);

  constexpr u32 two_hundred_four_unsigned = 0b0011001100;
  EXPECT_EQ((Common::ExtractBitsU<2, 2>(two_hundred_four_unsigned)), 3u);
  EXPECT_EQ((Common::ExtractBitsU<6, 2>(two_hundred_four_unsigned)), 51u);
  EXPECT_EQ((Common::ExtractBitsU<4, 3>(two_hundred_four_unsigned)), 9u);
  EXPECT_EQ((Common::ExtractBitsU<5, 5>(two_hundred_four_unsigned)), 6u);
  EXPECT_EQ((Common::ExtractBitsS<2, 2>(two_hundred_four_unsigned)), -1);
  EXPECT_EQ((Common::ExtractBitsS<6, 2>(two_hundred_four_unsigned)), -13);
  EXPECT_EQ((Common::ExtractBitsS<4, 3>(two_hundred_four_unsigned)), -7);
  EXPECT_EQ((Common::ExtractBitsS<5, 5>(two_hundred_four_unsigned)), 6);

  // Ensure the identity operation remains sign-independent.
  constexpr s32 negative_one = -1;
  constexpr u32 unsigned_max = 0xFFFFFFFFU;
  EXPECT_EQ((Common::ExtractBitsU<32, 0>(negative_one)), 0xFFFFFFFFU);
  EXPECT_EQ((Common::ExtractBitsU<32, 0>(unsigned_max)), 0xFFFFFFFFU);
  EXPECT_EQ((Common::ExtractBitsS<32, 0>(negative_one)), -1);
  EXPECT_EQ((Common::ExtractBitsS<32, 0>(unsigned_max)), -1);
}

TEST(BitUtils, InsertBit)
{
  u32 host = 0;

  Common::InsertBit<2>(host, true);
  EXPECT_EQ(host, 0b0000000'1'00u);
  Common::InsertBit<4>(host, true);
  EXPECT_EQ(host, 0b00000'1'0100u);
  Common::InsertBit<2>(host, true);  // true over true
  EXPECT_EQ(host, 0b0000010'1'00u);
  Common::InsertBit<2>(host, false);
  EXPECT_EQ(host, 0b0000010'0'00u);
  Common::InsertBit<4>(host, false);
  EXPECT_EQ(host, 0b00000'0'0000u);
  Common::InsertBit<2>(host, false);  // false over false
  EXPECT_EQ(host, 0b0000000'0'00u);
}

TEST(BitUtils, InsertBits)
{
  u32 host;

  host = 0;
  // A typical field, a field larger than the value, and overwriting bits with 1s
  Common::InsertBits<2, 2>(host, 3u);
  EXPECT_EQ(host, 0b000000'11'00u);
  Common::InsertBits<2 + 4, 2>(host, 3u);
  EXPECT_EQ(host, 0b00'000011'00u);
  Common::InsertBits<8, 1>(host, 255u);
  EXPECT_EQ(host, 0b0'11111111'0u);

  host = 0;
  // Overflow by 1 (same as writing 0s)
  Common::InsertBits<8, 1>(host, 256u);
  EXPECT_EQ(host, 0b0'00000000'0u);

  enum class TestEnum
  {
    A = 5,
    B = 12,
    C = -7,
  };

  host = 0;
  // Negative value and enum values
  Common::InsertBits<6, 1>(host, -7);
  EXPECT_EQ(host, 0b000'111001'0u);
  Common::InsertBits<6, 1>(host, TestEnum::A);
  EXPECT_EQ(host, 0b000'000101'0u);
  Common::InsertBits<6, 1>(host, TestEnum::B);
  EXPECT_EQ(host, 0b000'001100'0u);
  Common::InsertBits<6, 1>(host, TestEnum::C);
  EXPECT_EQ(host, 0b000'111001'0u);
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

TEST(BitUtils, ShortCharLowMask)
{
  // Logical NOT takes place after integral promotion, but I didn't know that when I replaced
  // std::numeric_limits<T>::max with ~T{0} (for unsigned types, of course).  To be absolutely
  // certain low masks for short and char are never broken again, we shall check them all.

  EXPECT_EQ(0b00000000, Common::CalcLowMaskSafe<u8>(0));
  EXPECT_EQ(0b00000001, Common::CalcLowMaskSafe<u8>(1));
  EXPECT_EQ(0b00000011, Common::CalcLowMaskSafe<u8>(2));
  EXPECT_EQ(0b00000111, Common::CalcLowMaskSafe<u8>(3));
  EXPECT_EQ(0b00001111, Common::CalcLowMaskSafe<u8>(4));
  EXPECT_EQ(0b00011111, Common::CalcLowMaskSafe<u8>(5));
  EXPECT_EQ(0b00111111, Common::CalcLowMaskSafe<u8>(6));
  EXPECT_EQ(0b01111111, Common::CalcLowMaskSafe<u8>(7));
  EXPECT_EQ(0b11111111, Common::CalcLowMaskSafe<u8>(8));

  EXPECT_EQ(0b00000000'00000000, Common::CalcLowMaskSafe<u16>(0));
  EXPECT_EQ(0b00000000'00000001, Common::CalcLowMaskSafe<u16>(1));
  EXPECT_EQ(0b00000000'00000011, Common::CalcLowMaskSafe<u16>(2));
  EXPECT_EQ(0b00000000'00000111, Common::CalcLowMaskSafe<u16>(3));
  EXPECT_EQ(0b00000000'00001111, Common::CalcLowMaskSafe<u16>(4));
  EXPECT_EQ(0b00000000'00011111, Common::CalcLowMaskSafe<u16>(5));
  EXPECT_EQ(0b00000000'00111111, Common::CalcLowMaskSafe<u16>(6));
  EXPECT_EQ(0b00000000'01111111, Common::CalcLowMaskSafe<u16>(7));
  EXPECT_EQ(0b00000000'11111111, Common::CalcLowMaskSafe<u16>(8));

  EXPECT_EQ(0b00000001'11111111, Common::CalcLowMaskSafe<u16>(9));
  EXPECT_EQ(0b00000011'11111111, Common::CalcLowMaskSafe<u16>(10));
  EXPECT_EQ(0b00000111'11111111, Common::CalcLowMaskSafe<u16>(11));
  EXPECT_EQ(0b00001111'11111111, Common::CalcLowMaskSafe<u16>(12));
  EXPECT_EQ(0b00011111'11111111, Common::CalcLowMaskSafe<u16>(13));
  EXPECT_EQ(0b00111111'11111111, Common::CalcLowMaskSafe<u16>(14));
  EXPECT_EQ(0b01111111'11111111, Common::CalcLowMaskSafe<u16>(15));
  EXPECT_EQ(0b11111111'11111111, Common::CalcLowMaskSafe<u16>(16));
}
