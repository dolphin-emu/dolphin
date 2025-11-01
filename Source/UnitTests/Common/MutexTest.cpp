// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <algorithm>
#include <mutex>
#include <thread>

#include "Common/Mutex.h"

template <typename MutexType>
static void DoAtomicMutexTest()
{
  MutexType work_mtx;

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
  MutexType done_mtx;
  std::unique_lock done_lk{done_mtx, std::try_to_lock};
  EXPECT_TRUE(done_lk.owns_lock());

  // try_lock() fails when holding a lock.
  EXPECT_FALSE(done_mtx.try_lock());

  static constexpr int THREAD_COUNT = 4;
  static constexpr int REPEAT_COUNT = 100;

  int done_count = 0;
  std::vector<std::thread> threads(THREAD_COUNT);
  for (auto& t : threads)
  {
    t = std::thread{[&] {
      // lock() blocks until main thread unlock()s.
      {
        std::lock_guard lk{done_mtx};
        ++done_count;
      }

      // Contesting lock() and try_lock() doesn't explode.
      for (int i = 0; i != REPEAT_COUNT; ++i)
      {
        {
          std::lock_guard lk{work_mtx};
        }
        std::unique_lock lk{work_mtx, std::try_to_lock};
      }
    }};
  }

  std::this_thread::sleep_for(SLEEP_TIME);

  // The threads are still blocking on done_mtx.
  EXPECT_EQ(done_count, 0);

  done_lk.unlock();
  std::ranges::for_each(threads, &std::thread::join);

  // The threads finished.
  EXPECT_EQ(done_count, THREAD_COUNT);

  // Things are still sane after contesting in worker threads.
  std::lock_guard lk{work_mtx};
}

TEST(Mutex, AtomicMutex)
{
  DoAtomicMutexTest<Common::AtomicMutex>();
  DoAtomicMutexTest<Common::SpinMutex>();
}
