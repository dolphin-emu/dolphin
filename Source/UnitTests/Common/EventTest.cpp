// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <thread>

#include "Common/Event.h"

using Common::Event;
using Common::TimedEvent;

template <typename EventType>
static void RunBackAndForthTest()
{
  EventType has_sent;
  EventType can_send;
  int shared_obj{};

  constexpr int ITERATIONS_COUNT = 100000;

  auto sender = [&] {
    for (int i = 0; i < ITERATIONS_COUNT; ++i)
    {
      can_send.Wait();
      shared_obj = i;
      has_sent.Set();
    }
  };

  auto receiver = [&] {
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

template <typename EventType>
static void RunAsyncSetTest()
{
  int wake_count = 0;

  EventType event;

  constexpr int ITERATIONS_COUNT = 1000;

  for (int i = 0; i < ITERATIONS_COUNT; ++i)
  {
    const int set_count = 432 + i;

    // main thread quickly adjusts some data and invokes Set() a bunch of times.
    // waiter_thread is awoken from Wait() a non-specific amount of times
    //  but ultimately does see the final data value and doesn't deadlock.
    // We use a pattern like this in Dolphin a few times.

    std::atomic<int> data_in{};
    int data_out{};

    std::thread waiter_thread{[&] {
      while (data_out != set_count)
      {
        event.Wait();
        ++wake_count;
        data_out = data_in.load(std::memory_order_relaxed);
      }
    }};

    for (int s = 0; s != set_count; ++s)
    {
      data_in.fetch_add(1, std::memory_order_relaxed);
      event.Set();
    }

    waiter_thread.join();

    EXPECT_EQ(data_in, data_out);
  }

  GTEST_LOG_(INFO) << "wake count: " << wake_count;
}

TEST(TimedEvent, BackAndForth)
{
  RunBackAndForthTest<TimedEvent>();
}

TEST(Event, BackAndForth)
{
  RunBackAndForthTest<Event>();
}

TEST(TimedEvent, AsyncSet)
{
  RunAsyncSetTest<TimedEvent>();
}

TEST(Event, AsyncSet)
{
  RunAsyncSetTest<Event>();
}
