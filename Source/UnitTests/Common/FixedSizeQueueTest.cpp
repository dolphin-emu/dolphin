// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <gtest/gtest.h>

#include "Common/FixedSizeQueue.h"

TEST(FixedSizeQueue, Simple)
{
  FixedSizeQueue<int, 5> q;

  EXPECT_EQ(0u, q.size());

  q.push(0);
  q.push(1);
  q.push(2);
  q.push(3);
  q.push(4);
  for (int i = 0; i < 1000; ++i)
  {
    EXPECT_EQ(i, q.front());
    EXPECT_EQ(i, q.pop_front());
    q.push(i + 5);
  }
  EXPECT_EQ(1000, q.pop_front());
  EXPECT_EQ(1001, q.pop_front());
  EXPECT_EQ(1002, q.pop_front());
  EXPECT_EQ(1003, q.pop_front());
  EXPECT_EQ(1004, q.pop_front());

  EXPECT_EQ(0u, q.size());
}
