// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/WorkQueueThread.h"

TEST(WorkQueueThread, Simple)
{
  Common::WorkQueueThreadSP<int> worker;

  constexpr int BIG_VAL = 1000;

  int x = 0;
  const auto func = [&](int value) { x = value; };

  worker.Push(1);
  worker.WaitForCompletion();
  // Still zero because it's not running.
  EXPECT_EQ(x, 0);

  // Does nothing if not running.
  worker.Shutdown();

  worker.Reset("test worker", func);
  worker.WaitForCompletion();
  // Items pushed before Reset are processed.
  EXPECT_EQ(x, 1);

  worker.Shutdown();
  worker.Push(0);
  worker.WaitForCompletion();
  // Still 1 because it's no longer running.
  EXPECT_EQ(x, 1);

  worker.Cancel();
  worker.Reset("test worker", func);
  worker.WaitForCompletion();
  // Still 1 because the work was canceled.
  EXPECT_EQ(x, 1);

  for (int i = 0; i != BIG_VAL; ++i)
    worker.Push(i);
  worker.Cancel();
  // Could be any one of the pushed values.
  EXPECT_LT(x, BIG_VAL);
  GTEST_LOG_(INFO) << "Canceled work after item " << x;

  worker.Push(2);
  worker.WaitForCompletion();
  // Still running after cancelation.
  EXPECT_EQ(x, 2);
}
