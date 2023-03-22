// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/FixedSizeQueue.h"

TEST(FixedSizeQueue, Simple)
{
  Common::FixedSizeQueue<int, 5> q;

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

TEST(FixedSizeQueue, RingBuffer)
{
  // Testing if queue works when used as a ring buffer
  Common::FixedSizeQueue<int, 5> q;

  EXPECT_EQ(0u, q.size());

  q.push(0);
  q.push(1);
  q.push(2);
  q.push(3);
  q.push(4);
  q.push(5);

  EXPECT_EQ(5u, q.size());
  EXPECT_EQ(1, q.pop_front());

  EXPECT_EQ(4u, q.size());
}

// Local classes cannot have static fields,
// therefore this has to be declared in global scope.
class NonTrivialTypeTestData
{
public:
  static inline int num_objects = 0;
  static inline int total_constructed = 0;
  static inline int default_constructed = 0;
  static inline int total_destructed = 0;

  NonTrivialTypeTestData()
  {
    num_objects++;
    total_constructed++;
    default_constructed++;
  }

  NonTrivialTypeTestData(int /*val*/)
  {
    num_objects++;
    total_constructed++;
  }

  ~NonTrivialTypeTestData()
  {
    num_objects--;
    total_destructed++;
  }
};

TEST(FixedSizeQueue, NonTrivialTypes)
{
  // Testing if construction/destruction of non-trivial types happens as expected
  Common::FixedSizeQueue<NonTrivialTypeTestData, 2> q;

  EXPECT_EQ(0u, q.size());

  EXPECT_EQ(2, NonTrivialTypeTestData::num_objects);
  EXPECT_EQ(2, NonTrivialTypeTestData::total_constructed);
  EXPECT_EQ(2, NonTrivialTypeTestData::default_constructed);
  EXPECT_EQ(0, NonTrivialTypeTestData::total_destructed);

  q.emplace(4);
  q.emplace(6);
  q.emplace(8);

  EXPECT_EQ(2, NonTrivialTypeTestData::num_objects);
  EXPECT_EQ(2 + 3, NonTrivialTypeTestData::total_constructed);
  EXPECT_EQ(2, NonTrivialTypeTestData::default_constructed);
  EXPECT_EQ(3, NonTrivialTypeTestData::total_destructed);
  EXPECT_EQ(2u, q.size());

  q.pop();
  q.pop();

  EXPECT_EQ(2, NonTrivialTypeTestData::num_objects);
  EXPECT_EQ(2 + 3 + 2, NonTrivialTypeTestData::total_constructed);
  EXPECT_EQ(2 + 2, NonTrivialTypeTestData::default_constructed);
  EXPECT_EQ(3 + 2, NonTrivialTypeTestData::total_destructed);
  EXPECT_EQ(0u, q.size());
}
