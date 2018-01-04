// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <gtest/gtest.h>

#include "Common/CommonFuncs.h"

TEST(CommonFuncs, ArraySizeFunction)
{
  char test[4];
  u32 test2[42];

  EXPECT_EQ(4u, ArraySize(test));
  EXPECT_EQ(42u, ArraySize(test2));

  (void)test;
  (void)test2;
}

TEST(CommonFuncs, RoundUpPow2Macro)
{
  EXPECT_EQ(4, ROUND_UP_POW2(3));
  EXPECT_EQ(4, ROUND_UP_POW2(4));
  EXPECT_EQ(8, ROUND_UP_POW2(6));
  EXPECT_EQ(0x40000000, ROUND_UP_POW2(0x23456789));
}

TEST(CommonFuncs, CrashMacro)
{
  EXPECT_DEATH({ Crash(); }, "");
}
