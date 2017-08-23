// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <gtest/gtest.h>
#include <thread>

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
    u32 v2;
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
  Common::SPSCQueue<u32> q;

  auto inserter = [&q]() {
    for (u32 i = 0; i < 100000; ++i)
      q.Push(i);
  };

  auto popper = [&q]() {
    for (u32 i = 0; i < 100000; ++i)
    {
      while (q.Empty())
        ;
      u32 v;
      q.Pop(v);
      EXPECT_EQ(i, v);
    }
  };

  std::thread popper_thread(popper);
  std::thread inserter_thread(inserter);

  popper_thread.join();
  inserter_thread.join();
}
