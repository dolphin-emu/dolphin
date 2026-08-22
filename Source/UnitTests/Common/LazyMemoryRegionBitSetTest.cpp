// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <fmt/format.h>
#include <gtest/gtest.h>

#include "Common/LazyMemoryRegionBitSet.h"
#include "Common/MemArena.h"

// Intentionally not divisible by 8
static constexpr size_t BIT_COUNT = 1024 + 512 + 1;

// Two blocks for the test to write to, and one block that remains mapped to the default page so we
// can check that we didn't accidentally write to the default page
static constexpr size_t WINDOWS_BOUNDARY_BIT_COUNT =
    Common::LazyMemoryRegion::WINDOWS_BLOCK_SIZE * 8 * 3;

static u32 ExpectedContentAllZeroes(size_t)
{
  return 0;
}

static u32 ExpectedContentAllOnes(size_t)
{
  return 0xFFFFFFFF;
}

static u32 ExpectedContentAfterSetting(size_t index)
{
  switch (index)
  {
  case 32:
    return 0x0000FF00;
  case 34:
    return 0x0FFFFFF0;
  case 35:
    return 0xFFFFFFF0;
  case 36:
    return 0x0FFFFFFF;
  case 37:
    return 0xFFFFFFF0;
  case 39:
    return 0x0FFFFFFF;
  case 40:
    return 0xFFFFFFF0;
  case 47:
    return 0x0FFFFFFF;
  default:
    if (index < 32)
      return 1 << index;
    if (index < 48)
      return 0xFFFFFFFF;
    return 0;
  }
}

static u32 ExpectedContentAfterClearing(size_t index)
{
  return ~ExpectedContentAfterSetting(index);
}

static u32 ExpectedContentForWindowsBoundary(size_t index)
{
  switch (index)
  {
  case Common::LazyMemoryRegion::WINDOWS_BLOCK_SIZE / sizeof(u32) - 1:
    return 0x80000000;
  case Common::LazyMemoryRegion::WINDOWS_BLOCK_SIZE / sizeof(u32):
    return 0x00000001;
  default:
    return 0;
  }
}

template <typename F>
static void VerifyContent(const u32* memory, size_t bits, F f)
{
  const size_t end_index = bits / 32;
  for (size_t i = 0; i < end_index; ++i, ++memory)
  {
    const u32 expected = f(i);
    ASSERT_EQ(*memory, expected) << "i=" << i;
  }
  if (bits % 32 != 0)
  {
    const u32 expected = f(end_index) & ~(0xFFFFFFFF << bits % 32);
    ASSERT_EQ(*memory, expected) << "i=" << end_index;
  }
}

TEST(LazyMemoryRegionBitSet, Unwritten)
{
  Common::LazyMemoryRegionBitSet bit_set(BIT_COUNT);
  VerifyContent(bit_set.GetRawMemory(), BIT_COUNT, ExpectedContentAllZeroes);
}

TEST(LazyMemoryRegionBitSet, Set)
{
  Common::LazyMemoryRegionBitSet bit_set(BIT_COUNT);

  // Set single bits in the range 0-1023
  for (size_t i = 0; i < 32; ++i)
    bit_set.SetBit(i * 33);

  // Set ranges of bits in bits 1024 and higher
  bit_set.SetBits(1024 + 8, 1024 + 16);
  bit_set.SetBits(1024 + 32, 1024 + 64);
  bit_set.SetBits(1024 + 68, 1024 + 92);
  bit_set.SetBits(1024 + 100, 1024 + 128);
  bit_set.SetBits(1024 + 128, 1024 + 156);
  bit_set.SetBits(1024 + 164, 1024 + 252);
  bit_set.SetBits(1024 + 260, 1024 + 384);
  bit_set.SetBits(1024 + 384, 1024 + 508);

  VerifyContent(bit_set.GetRawMemory(), BIT_COUNT, ExpectedContentAfterSetting);

  bit_set.Clear();
  VerifyContent(bit_set.GetRawMemory(), BIT_COUNT, ExpectedContentAllZeroes);
}

TEST(LazyMemoryRegionBitSet, Clear)
{
  Common::LazyMemoryRegionBitSet bit_set(BIT_COUNT);
  bit_set.SetBits(0, BIT_COUNT);
  VerifyContent(bit_set.GetRawMemory(), BIT_COUNT, ExpectedContentAllOnes);

  // Clear single bits in the range 0-1023
  for (size_t i = 0; i < 32; ++i)
    bit_set.ClearBit(i * 33);

  // Clear ranges of bits in bits 1024 and higher
  bit_set.ClearBits(1024 + 8, 1024 + 16);
  bit_set.ClearBits(1024 + 32, 1024 + 64);
  bit_set.ClearBits(1024 + 68, 1024 + 92);
  bit_set.ClearBits(1024 + 100, 1024 + 128);
  bit_set.ClearBits(1024 + 128, 1024 + 156);
  bit_set.ClearBits(1024 + 164, 1024 + 252);
  bit_set.ClearBits(1024 + 260, 1024 + 384);
  bit_set.ClearBits(1024 + 384, 1024 + 508);

  VerifyContent(bit_set.GetRawMemory(), BIT_COUNT, ExpectedContentAfterClearing);

  bit_set.Clear();
  VerifyContent(bit_set.GetRawMemory(), BIT_COUNT, ExpectedContentAllZeroes);
}

TEST(LazyMemoryRegionBitSet, WindowsBoundary)
{
  // Unlike on other operating systems, LazyMemoryRegion's Windows implementation allocates one page
  // containing only zeroes and maps it to every part of the region by default. If
  // LazyMemoryRegionBitSet wants to write to any part of the region, it first needs to call
  // LazyMemoryRegion::EnsureMemoryPageWritable. This test is checking that
  // LazyMemoryRegion::EnsureMemoryPageWritable is being called with the correct parameters.

  Common::LazyMemoryRegionBitSet bit_set(WINDOWS_BOUNDARY_BIT_COUNT);

  bit_set.SetBits(Common::LazyMemoryRegion::WINDOWS_BLOCK_SIZE * 8 - 1,
                  Common::LazyMemoryRegion::WINDOWS_BLOCK_SIZE * 8 + 1);

  VerifyContent(bit_set.GetRawMemory(), WINDOWS_BOUNDARY_BIT_COUNT,
                ExpectedContentForWindowsBoundary);
}
