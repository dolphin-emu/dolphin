// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Based on gtest_main.cc

#include <cstdio>
#include <fmt/format.h>

#include "Common/MsgHandler.h"

#include "gtest/gtest.h"

namespace
{
bool TestMsgHandler(const char* caption, const char* text, bool yes_no, Common::MsgType style)
{
  fmt::print(stderr, "{}\n", text);
  ADD_FAILURE();
  // Return yes to any question (we don't need Dolphin to break on asserts)
  return true;
}
}  // namespace

int main(int argc, char** argv)
{
  fmt::print(stderr, "Running main() from UnitTestsMain.cpp\n");
  Common::RegisterMsgAlertHandler(TestMsgHandler);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
