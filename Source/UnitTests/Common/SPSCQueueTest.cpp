// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <memory>
#include <thread>

#include "Common/CommonTypes.h"
#include "Common/SPSCQueue.h"

TEST(SPSCQueue, Simple)
{
  Common::SPSCQueue<u32> q;

  EXPECT_EQ(0u, q.Size());
  EXPECT_TRUE(q.Empty());

  q.Push(1);
  EXPECT_EQ(1u, q.Size());
  EXPECT_FALSE(q.Empty());

  u32 v;
  q.Pop(v);
  EXPECT_EQ(1u, v);
  EXPECT_EQ(0u, q.Size());
  EXPECT_TRUE(q.Empty());

  // Test the FIFO order.
  for (u32 i = 0; i < 1000; ++i)
    q.Push(i);
  EXPECT_EQ(1000u, q.Size());
  for (u32 i = 0; i < 1000; ++i)
  {
    u32 v2 = 0;
    q.Pop(v2);
    EXPECT_EQ(i, v2);
  }
  EXPECT_TRUE(q.Empty());

  for (u32 i = 0; i < 1000; ++i)
    q.Push(i);
  EXPECT_FALSE(q.Empty());
  q.Clear();
  EXPECT_TRUE(q.Empty());
}

TEST(SPSCQueue, MultiThreaded)
{
  struct Foo
  {
    std::shared_ptr<int> ptr;
    u32 i;
  };

  // A shared_ptr held by every element in the queue.
  auto sptr = std::make_shared<int>(0);

  auto queue_ptr = std::make_unique<Common::WaitableSPSCQueue<Foo>>();
  auto& q = *queue_ptr;

  constexpr u32 reps = 100000;

  auto inserter = [&] {
    for (u32 i = 0; i != reps; ++i)
      q.Push({sptr, i});

    q.WaitForEmpty();
    EXPECT_EQ(sptr.use_count(), 1);
    q.Push({sptr, 0});
    EXPECT_EQ(sptr.use_count(), 2);
  };

  auto popper = [&] {
    for (u32 i = 0; i != reps; ++i)
    {
      q.WaitForData();
      EXPECT_EQ(i, q.Front().i);
      q.Pop();
    }
  };

  std::thread popper_thread(popper);
  std::thread inserter_thread(inserter);

  popper_thread.join();
  inserter_thread.join();

  queue_ptr.reset();
  EXPECT_EQ(sptr.use_count(), 1);
}
