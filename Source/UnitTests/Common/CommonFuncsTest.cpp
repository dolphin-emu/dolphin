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

TEST(CommonFuncs, CrashMacro)
{
  EXPECT_DEATH({ Crash(); }, "");
}
