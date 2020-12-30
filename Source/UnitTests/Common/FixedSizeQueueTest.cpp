// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <gtest/gtest.h>

#include "Common/FixedSizeQueue.h"

TEST(FixedSizeQueue, PushEmplacePop)
{
  FixedSizeQueue<int, 5> q;

  EXPECT_EQ(0u, q.size());
  EXPECT_EQ(5u, q.max_size());

  q.push(0);
  q.push(1);
  q.push(2);
  q.push(3);
  q.emplace(4);
  for (int i = 0; i < 1000; ++i)
  {
    EXPECT_EQ(i, q[0]);
    EXPECT_EQ(i, q.pop_front());
    q.push(i + 5);
  }
  EXPECT_EQ(1000, q.pop_front());
  EXPECT_EQ(1001, q.pop_front());
  EXPECT_EQ(1002, q.pop_front());
  EXPECT_EQ(1003, q.pop_front());
  EXPECT_EQ(1004, q.pop_front());

  EXPECT_EQ(0u, q.size());
  EXPECT_EQ(5u, q.max_size());
}

TEST(FixedSizeQueue, EraseClear)
{
  FixedSizeQueue<int, 2> q;

  q.push(0);
  q.push(0);
  q.erase(2);
  EXPECT_EQ(0u, q.size());
  q.push(0);
  q.push(0);
  q.clear();
  EXPECT_EQ(0u, q.size());
}

TEST(FixedSizeQueue, RingBuffer)
{
  // Testing if queue works when used as a ring buffer
  FixedSizeQueue<int, 5> q;

  EXPECT_EQ(0u, q.size());

  q.push(0);
  EXPECT_EQ(0, q[0]);
  q.push(1);
  EXPECT_EQ(1, q[1]);
  q.push(2);
  q.push(3);
  q.push(4);
  EXPECT_EQ(4, q[4]);  // q is 0 1 2 3 4. External index 0 is 0 internally
  q.push(5);  // Loops to internal index 0
  EXPECT_EQ(5, q[4]);  // q is 5 1 2 3 4. External index 0 is 1 internally
  EXPECT_EQ(1, q[0]);

  EXPECT_EQ(5u, q.size());

  EXPECT_EQ(1, q.pop_front());
  EXPECT_EQ(4u, q.size());
  q.erase(2);
  EXPECT_EQ(2u, q.size());
}

TEST(FixedSizeQueue, Copy)
{
  // Testing copy from and to array, which can only be used on trivial types
  FixedSizeQueue<int, 5> q;

  // Copy over an array bigger than us, which means the copy will loop over
  std::array<int, 6> other_array = {1, 2, 3, 4, 5, 6};
  q.push_array(other_array.data(), other_array.size());
  // q is 6 2 3 4 5. External index 0 is 1 internally
  EXPECT_EQ(6, q[4]);
  EXPECT_EQ(2, q[0]);
  EXPECT_EQ(5, q[3]);

  // Copy it back (up to 5, our size). It will start from its head/front
  for (size_t i = 0; i < other_array.size(); ++i)
  {
    other_array[i] = 0;
  }
  q.copy_to_array(other_array.data(), 5);
  EXPECT_EQ(2, other_array[0]);
  EXPECT_EQ(6, other_array[4]);
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
  FixedSizeQueue<NonTrivialTypeTestData, 2> q;

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
