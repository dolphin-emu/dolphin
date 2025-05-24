// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <atomic>

#include "Common/CommonTypes.h"
#include "Common/FlushThread.h"

TEST(FlushThread, Simple)
{
  std::atomic<int> result = 0;
  std::atomic<int> desired = 1;
  const auto flush_func = [&] {
    result.store(desired.load(std::memory_order_relaxed), std::memory_order_relaxed);
  };

  Common::FlushThread ft;
  ft.Reset("flush", flush_func);

  // No flush on start.
  EXPECT_EQ(result.load(), 0);

  for (int rep = 0; rep != 10; ++rep)
  {
    constexpr int expected = 5;
    for (int i = 0; i <= expected; ++i)
    {
      desired.store(i, std::memory_order_relaxed);
      ft.SetDirty();
    }

    // Make sure we always get the latest value after multiple dirty calls.
    ft.WaitForCompletion();
    EXPECT_EQ(result.load(std::memory_order_relaxed), expected);
  }

  result = 0;
  desired = 1;

  // Big delay.
  ft.SetFlushDelay(std::chrono::seconds{9999});
  ft.SetDirty();

  // WaitForCompletion should flush immediately, ignoring the configured delay.
  {
    const auto start = std::chrono::steady_clock::now();
    ft.WaitForCompletion();
    const auto end = std::chrono::steady_clock::now();
    GTEST_LOG_(INFO) << "Ideally 0: "
                     << duration_cast<std::chrono::milliseconds>(end - start).count();
  }
  EXPECT_EQ(result.load(), 1);

  desired = 2;

  ft.SetDirty();
  ft.Reset("noop", [] {});
  // Reset causes a flush before the noop function is established.
  EXPECT_EQ(result.load(), 2);

  desired = 3;

  ft.SetDirty();
  ft.WaitForCompletion();
  // No change because noop func.
  EXPECT_EQ(result.load(), 2);

  ft.Reset("flush", flush_func);
  ft.WaitForCompletion();
  // No change because not dirty.
  EXPECT_EQ(result.load(), 2);

  ft.Shutdown();
  ft.SetDirty();
  ft.WaitForCompletion();
  // No change because Shutdown.
  EXPECT_EQ(result.load(), 2);

  ft.Reset("flush", flush_func);
  ft.WaitForCompletion();
  // Dirty state persits on Reset.
  EXPECT_EQ(result.load(), 3);

  result = 0;

  ft.Reset("inc", [&] {
    ++result;
    result.notify_one();
  });

  static constexpr auto flush_delay = std::chrono::milliseconds{1};
  ft.SetFlushDelay(flush_delay);
  ft.SetDirty();
  ft.SetDirty();
  ft.SetDirty();

  // Probably no flush yet, because of the delay.
  GTEST_LOG_(INFO) << "Ideally 0: " << result.load();

  // The flush happens after the configured delay.
  {
    const auto start = std::chrono::steady_clock::now();
    result.wait(0);
    const auto end = std::chrono::steady_clock::now();
    GTEST_LOG_(INFO) << "Ideally " << flush_delay.count() << ": "
                     << duration_cast<std::chrono::milliseconds>(end - start).count();
  }

  ft.Shutdown();
  // At least one flush happened. Probably exactly one.
  GTEST_LOG_(INFO) << "Ideally 1: " << result.load();
  EXPECT_GT(result.load(), 0);
}
