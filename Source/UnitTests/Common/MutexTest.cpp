// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "Common/TransferableSharedMutex.h"

TEST(Mutex, TransferableSharedMutex)
{
  Common::TransferableSharedMutex work_mtx;

  bool worker_done = false;

  static constexpr auto SLEEP_TIME = std::chrono::microseconds{1};

  // lock() on main thread, unlock() on worker thread.
  std::thread thread{[&, lk = std::unique_lock{work_mtx}] {
    std::this_thread::sleep_for(SLEEP_TIME);
    worker_done = true;
  }};

  // lock() waits for the thread to unlock().
  if (std::lock_guard lk{work_mtx}; true)
    EXPECT_TRUE(worker_done);

  thread.join();

  // Prevent below workers from incrementing `done_count`.
  Common::TransferableSharedMutex done_mtx;
  std::unique_lock done_lk{done_mtx, std::try_to_lock};
  EXPECT_TRUE(done_lk.owns_lock());

  // try_*() fails when holding an exclusive lock.
  EXPECT_FALSE(done_mtx.try_lock());
  EXPECT_FALSE(done_mtx.try_lock_shared());

  static constexpr int THREAD_COUNT = 4;
  static constexpr int REPEAT_COUNT = 100;

  std::atomic<int> work_count = 0;
  std::atomic<int> done_count = 0;
  std::vector<std::thread> threads(THREAD_COUNT);
  for (auto& t : threads)
  {
    // lock_shared() multiple times on main thread.
    t = std::thread{[&, work_lk = std::shared_lock{work_mtx}]() mutable {
      std::this_thread::sleep_for(SLEEP_TIME);
      ++work_count;

      // try_lock() fails after lock_shared().
      EXPECT_FALSE(work_mtx.try_lock());

      // Main thread already holds done_mtx.
      EXPECT_FALSE(done_mtx.try_lock());
      EXPECT_FALSE(done_mtx.try_lock_shared());

      // Signal work is done.
      work_lk.unlock();

      // lock_shared() blocks until main thread unlock()s.
      {
        std::shared_lock lk{done_mtx};
        ++done_count;
      }

      // Contesting try_lock_shared() doesn't spontaneously fail.
      for (int i = 0; i != REPEAT_COUNT; ++i)
      {
        std::shared_lock lk{done_mtx, std::try_to_lock};
        EXPECT_TRUE(lk.owns_lock());
      }

      // Contesting lock() and lock_shared() doesn't explode.
      for (int i = 0; i != REPEAT_COUNT; ++i)
      {
        {
          std::lock_guard lk{work_mtx};
        }
        std::shared_lock lk{work_mtx};
      }
    }};
  }

  // lock() waits for threads to unlock_shared().
  if (std::lock_guard lk{work_mtx}; true)
    EXPECT_EQ(work_count.load(std::memory_order_relaxed), THREAD_COUNT);

  std::this_thread::sleep_for(SLEEP_TIME);

  // The threads are still blocking on done_mtx.
  EXPECT_EQ(done_count, 0);

  done_lk.unlock();
  std::ranges::for_each(threads, &std::thread::join);

  // The threads finished.
  EXPECT_EQ(done_count, THREAD_COUNT);

  // Things are still sane after contesting in worker threads.
  done_lk.lock();
  std::lock_guard lk{work_mtx};
}
