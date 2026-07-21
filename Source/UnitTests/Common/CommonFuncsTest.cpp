// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/CommonFuncs.h"

#include <gtest/gtest.h>

TEST(CommonFuncs, CrashMacro)
{
  EXPECT_DEATH({ Crash(); }, "");
}
