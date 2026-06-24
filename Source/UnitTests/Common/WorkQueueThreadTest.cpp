// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <memory>

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
  // Still running after cancellation.
  EXPECT_EQ(x, 2);
}

TEST(WorkQueueThread, AsyncWorkThread)
{
  // AsyncWorkThread is just wrapper around WorkQueueThread.
  // We'll just test some additional fun stuff.
  Common::AsyncWorkThread worker;

  int val = 0;

  auto set_val_from_ptr_on_worker = [&](std::shared_ptr<int> ptr) -> Common::Detached {
    // Suspend and resume execution on the worker thread.
    co_await worker.ScheduleHere();
    val = *ptr;
  };

  const auto shared_int = std::make_shared<int>(1);

  set_val_from_ptr_on_worker(shared_int);

  worker.WaitForCompletion();  // no-op. not running.

  // Still zero because worker isn't running.
  EXPECT_EQ(val, 0);
  // A copy of the shared_ptr is inside the coroutine inside the paused job
  EXPECT_EQ(shared_int.use_count(), 2);

  // Queued work now executes.
  worker.Reset("test worker");
  worker.WaitForCompletion();
  EXPECT_EQ(val, 1);
  EXPECT_EQ(shared_int.use_count(), 1);

  worker.PushBlocking([&] { val = 2; });
  EXPECT_EQ(val, 2);

  worker.Shutdown();

  *shared_int = 3;
  set_val_from_ptr_on_worker(shared_int);

  worker.WaitForCompletion();  // no-op. not running.

  // A copy of the shared_ptr is inside the coroutine inside the paused job
  EXPECT_EQ(shared_int.use_count(), 2);
  // The value is unchanged.
  EXPECT_EQ(val, 2);

  // Remove all work.
  worker.Cancel();

  // The coroutine was destroyed without being invoked.
  EXPECT_EQ(shared_int.use_count(), 1);
  // Value is still unchanged.
  EXPECT_EQ(val, 2);
}
