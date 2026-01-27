// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <thread>

#include "Common/Mutex.h"
#include "Common/TransferableSharedMutex.h"

template <typename MutexType>
static void DoAtomicMutexTests(const char mutex_name[])
{
  MutexType work_mutex;

  bool worker_done = false;

  static constexpr auto SLEEP_TIME = std::chrono::microseconds{1};

  // lock() on main thread, unlock() on worker thread.
  std::thread thread{[&, lk = std::unique_lock{work_mutex}] {
    std::this_thread::sleep_for(SLEEP_TIME);
    worker_done = true;
  }};

  // lock() waits for the thread to unlock().
  {
    std::lock_guard lk{work_mutex};
    EXPECT_TRUE(worker_done);
  }

  thread.join();

  // Prevent below workers from incrementing `done_count`.
  MutexType done_mutex;
  std::unique_lock done_lk{done_mutex};

  // try_lock() fails when holding a lock.
  EXPECT_FALSE(done_mutex.try_lock());

  static constexpr int THREAD_COUNT = 4;
  static constexpr int REPEAT_COUNT = 100;
  static constexpr int TOTAL_ITERATIONS = THREAD_COUNT * REPEAT_COUNT;

  int done_count = 0;
  int work_count = 0;
  std::atomic<int> try_lock_fail_count{};
  std::vector<std::thread> threads(THREAD_COUNT);
  for (auto& t : threads)
  {
    t = std::thread{[&] {
      // lock() blocks until main thread unlock()s.
      {
        std::lock_guard lk{done_mutex};
        ++done_count;
      }

      // Contesting lock() and try_lock() doesn't explode.
      for (int i = 0; i != REPEAT_COUNT; ++i)
      {
        {
          std::lock_guard lk{work_mutex};
          ++work_count;
        }

        // Try lock in a loop.
        while (!work_mutex.try_lock())
        {
          try_lock_fail_count.fetch_add(1, std::memory_order_relaxed);
        }
        std::lock_guard lk{work_mutex, std::adopt_lock};
        ++work_count;
      }
    }};
  }

  std::this_thread::sleep_for(SLEEP_TIME);

  // The threads are still blocking on done_mutex.
  EXPECT_EQ(done_count, 0);

  done_lk.unlock();
  std::ranges::for_each(threads, &std::thread::join);

  // The threads finished.
  EXPECT_EQ(done_count, THREAD_COUNT);
  EXPECT_EQ(work_count, TOTAL_ITERATIONS * 2);

  GTEST_LOG_(INFO) << mutex_name << "::try_lock() failure %: "
                   << (try_lock_fail_count * 100.0 / (TOTAL_ITERATIONS + try_lock_fail_count));

  // Things are still sane after contesting in worker threads.
  std::lock_guard lk{work_mutex};
}

TEST(Mutex, AtomicMutex)
{
  DoAtomicMutexTests<Common::AtomicMutex>("AtomicMutex");
  DoAtomicMutexTests<Common::SpinMutex>("SpinMutex");
}

TEST(Mutex, TransferableSharedMutex)
{
  Common::TransferableSharedMutex work_mutex;

  bool worker_done = false;

  static constexpr auto SLEEP_TIME = std::chrono::microseconds{1};

  // lock() on main thread, unlock() on worker thread.
  std::thread thread{[&, lk = std::unique_lock{work_mutex}] {
    std::this_thread::sleep_for(SLEEP_TIME);
    worker_done = true;
  }};

  // lock() waits for the thread to unlock().
  {
    std::lock_guard lk{work_mutex};
    EXPECT_TRUE(worker_done);
  }

  thread.join();

  // Prevent below workers from incrementing `done_count`.
  Common::TransferableSharedMutex done_mutex;
  std::unique_lock done_lk{done_mutex};

  // try_*() fails when holding an exclusive lock.
  EXPECT_FALSE(done_mutex.try_lock());
  EXPECT_FALSE(done_mutex.try_lock_shared());

  static constexpr int THREAD_COUNT = 4;
  static constexpr int REPEAT_COUNT = 100;
  static constexpr int TOTAL_ITERATIONS = THREAD_COUNT * REPEAT_COUNT;

  std::atomic<int> work_count = 0;
  std::atomic<int> done_count = 0;
  int additional_work_count = 0;

  std::atomic<int> try_lock_fail_count = 0;
  std::atomic<int> try_lock_shared_fail_count = 0;

  std::vector<std::thread> threads(THREAD_COUNT);
  for (auto& t : threads)
  {
    // lock_shared() multiple times on main thread.
    t = std::thread{[&, work_lk = std::shared_lock{work_mutex}]() mutable {
      std::this_thread::sleep_for(SLEEP_TIME);

      // try_lock() fails after lock_shared().
      EXPECT_FALSE(work_mutex.try_lock());

      // Main thread already holds done_mutex.
      EXPECT_FALSE(done_mutex.try_lock());
      EXPECT_FALSE(done_mutex.try_lock_shared());

      ++work_count;

      // Signal work is done.
      work_lk.unlock();

      // lock_shared() blocks until main thread unlock()s.
      {
        std::shared_lock lk{done_mutex};
        ++done_count;
      }

      // Contesting all of [try_]lock[_shared] doesn't explode.
      for (int i = 0; i != REPEAT_COUNT; ++i)
      {
        while (!work_mutex.try_lock())
        {
          try_lock_fail_count.fetch_add(1, std::memory_order_relaxed);
        }
        work_mutex.unlock();

        while (!work_mutex.try_lock_shared())
        {
          try_lock_shared_fail_count.fetch_add(1, std::memory_order_relaxed);
        }
        work_mutex.unlock_shared();
        {
          std::lock_guard lk{work_mutex};
          ++additional_work_count;
        }
        std::shared_lock lk{work_mutex};
      }
    }};
  }

  // lock() waits for threads to unlock_shared().
  {
    std::lock_guard lk{work_mutex};
    EXPECT_EQ(work_count.load(std::memory_order_relaxed), THREAD_COUNT);
  }

  std::this_thread::sleep_for(SLEEP_TIME);

  // The threads are still blocking on done_mutex.
  EXPECT_EQ(done_count, 0);

  done_lk.unlock();
  std::ranges::for_each(threads, &std::thread::join);

  // The threads finished.
  EXPECT_EQ(done_count, THREAD_COUNT);

  EXPECT_EQ(additional_work_count, TOTAL_ITERATIONS);

  GTEST_LOG_(INFO) << "try_lock() failure %: "
                   << (try_lock_fail_count * 100.0 / (TOTAL_ITERATIONS + try_lock_fail_count));

  GTEST_LOG_(INFO) << "try_lock_shared() failure %: "
                   << (try_lock_shared_fail_count * 100.0 /
                       (TOTAL_ITERATIONS + try_lock_shared_fail_count));

  // Things are still sane after contesting in worker threads.
  done_lk.lock();
  std::lock_guard lk{work_mutex};
}
