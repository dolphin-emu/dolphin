// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/Swap.h"

TEST(Swap, SwapByValue)
{
  EXPECT_EQ(0xf0, Common::swap8(0xf0));
  EXPECT_EQ(0x1234, Common::swap16(0x3412));
  EXPECT_EQ(0x12345678u, Common::swap32(0x78563412));
  EXPECT_EQ(0x123456789abcdef0ull, Common::swap64(0xf0debc9a78563412ull));
}
