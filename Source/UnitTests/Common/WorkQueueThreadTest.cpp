// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/WorkQueueThread.h"

TEST(WorkQueueThread, Simple)
{
  Common::WorkQueueThreadSP<int> worker;

  constexpr int BIG_VAL = 100;

  int x = 0;
  const auto func = [&](int value) {
    x = value;
    std::this_thread::yield();
  };

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
  EXPECT_EQ(worker.Empty(), true);

  worker.Shutdown();
  worker.Push(0);
  worker.WaitForCompletion();
  // Still 1 because it's no longer running.
  EXPECT_EQ(x, 1);
  EXPECT_EQ(worker.Empty(), false);

  worker.Cancel();
  worker.Reset("test worker", func);
  worker.WaitForCompletion();
  // Still 1 because the work was canceled.
  EXPECT_EQ(x, 1);

  x = 0;
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

  int processed_count = 0;
  worker.Reset("test worker", [&](auto) { ++processed_count; });
  for (int i = 0; i != BIG_VAL; ++i)
    worker.Push(i);

  int gather_count = 0;
  worker.GatherItems([&](auto) { ++gather_count; });
  EXPECT_EQ(worker.Empty(), true);

  // Gathered all the items that weren't processed.
  EXPECT_EQ(processed_count + gather_count, BIG_VAL);
  GTEST_LOG_(INFO) << "Gathered items " << gather_count;
}
