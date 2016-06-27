// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <atomic>
#include <thread>

#include <gtest/gtest.h>

#include "Common/BlockingLoop.h"
#include "Common/Thread.h"

TEST(BusyLoopTest, MultiThreaded)
{
  Common::BlockingLoop loop;
  Common::Event e;
  for (int i = 0; i < 10; i++)
  {
    loop.Prepare();
    std::thread loop_thread([&]() { loop.Run([&]() { e.Set(); }); });

    // Ping - Pong
    for (int j = 0; j < 10; j++)
    {
      loop.Wakeup();
      e.Wait();

      // Just waste some time. So the main loop did fall back to the sleep state much more likely.
      Common::SleepCurrentThread(1);
    }

    for (int j = 0; j < 100; j++)
    {
      // We normally have to call Wakeup to assure the Event is triggered.
      // But this check is for an internal feature of the BlockingLoop.
      // It's implemented to fall back to a busy loop regulary.
      // If we're in the busy loop, the payload (and so the Event) is called all the time.
      // loop.Wakeup();
      e.Wait();
    }

    loop.Stop();
    loop_thread.join();
  }
}
