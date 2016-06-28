// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <gtest/gtest.h>
#include <thread>

#include "Common/Event.h"

using Common::Event;

TEST(Event, MultiThreaded)
{
  Event has_sent, can_send;
  int shared_obj;
  const int ITERATIONS_COUNT = 100000;

  auto sender = [&]() {
    for (int i = 0; i < ITERATIONS_COUNT; ++i)
    {
      can_send.Wait();
      shared_obj = i;
      has_sent.Set();
    }
  };

  auto receiver = [&]() {
    for (int i = 0; i < ITERATIONS_COUNT; ++i)
    {
      has_sent.Wait();
      EXPECT_EQ(i, shared_obj);
      can_send.Set();
    }
  };

  std::thread sender_thread(sender);
  std::thread receiver_thread(receiver);

  can_send.Set();

  sender_thread.join();
  receiver_thread.join();
}
