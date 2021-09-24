// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/CommonFuncs.h"

TEST(CommonFuncs, CrashMacro)
{
  EXPECT_DEATH({ Crash(); }, "");
}
