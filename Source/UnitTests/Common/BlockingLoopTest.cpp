// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <atomic>
#include <thread>

#include <gtest/gtest.h>

#include "Common/BlockingLoop.h"

TEST(BlockingLoop, MultiThreaded)
{
  Common::BlockingLoop loop;
  std::atomic<int> signaled_a(0);
  std::atomic<int> received_a(0);
  std::atomic<int> signaled_b(0);
  std::atomic<int> received_b(0);
  for (int i = 0; i < 100; i++)
  {
    // Invalidate the current state.
    received_a.store(signaled_a.load() + 1);
    received_b.store(signaled_b.load() + 123);

    // Must not block as the loop is stopped.
    loop.Wait();

    std::thread loop_thread([&]() {
      loop.Run([&]() {
        received_a.store(signaled_a.load());
        received_b.store(signaled_b.load());
      });
    });

    // Now Wait must block.
    loop.Prepare();

    // The payload must run at least once on startup.
    loop.Wait();
    EXPECT_EQ(signaled_a.load(), received_a.load());
    EXPECT_EQ(signaled_b.load(), received_b.load());

    std::thread run_a_thread([&]() {
      for (int j = 0; j < 100; j++)
      {
        for (int k = 0; k < 100; k++)
        {
          signaled_a++;
          loop.Wakeup();
        }

        loop.Wait();
        EXPECT_EQ(signaled_a.load(), received_a.load());
      }
    });
    std::thread run_b_thread([&]() {
      for (int j = 0; j < 100; j++)
      {
        for (int k = 0; k < 100; k++)
        {
          signaled_b++;
          loop.Wakeup();
        }

        loop.Wait();
        EXPECT_EQ(signaled_b.load(), received_b.load());
      }
    });

    run_a_thread.join();
    run_b_thread.join();

    loop.Stop();

    // Must not block
    loop.Wait();

    loop_thread.join();
  }
}
