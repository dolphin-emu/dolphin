// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <gtest/gtest.h>

#include "Common/RingBuffer.h"

TEST(RingBuffer, RingBuffer)
{
  //check unitialized ringbuffer
  RingBuffer<u32> rb;

  EXPECT_EQ(0u, rb.LoadHead());
  EXPECT_EQ(0u, rb.LoadTail());

  // check resize behavior for a few numbers (and round to next power of 2 size)
  std::array<size_t, 3> query_sizes{ {1000, 512, 10} };
  std::array<size_t, 3> actual_sizes{ {1024, 512, 16} };
  for (size_t i = 0; i < query_sizes.size(); ++i)
  {
    rb.Resize(query_sizes[i]);
    EXPECT_EQ(actual_sizes[i], rb.MaxSize());
    EXPECT_EQ(actual_sizes[i] - 1, rb.Mask());
  }

  // resize should keep head and tail at 0
  EXPECT_EQ(0u, rb.LoadHead());
  EXPECT_EQ(0u, rb.LoadTail());

  // check random access and circularity
  for (size_t i = 0; i < rb.MaxSize(); ++i) {
    rb[i] = static_cast<u32>(i);
    EXPECT_EQ(static_cast<u32>(i), rb[i]);
    EXPECT_EQ(static_cast<u32>(i), rb[i + rb.MaxSize() * i]);
    rb[i + rb.MaxSize() * i] = static_cast<u32>(i + rb.MaxSize() * i);
    EXPECT_EQ(static_cast<u32>(i + rb.MaxSize() * i), rb[i]);
  }
  // should not affect head and tail
  EXPECT_EQ(0u, rb.LoadHead());
  EXPECT_EQ(0u, rb.LoadTail());

  // 0s the array
  // we don't need to reset head and tail because they haven't been moved
  std::fill_n(&rb[0], rb.MaxSize(), 0);
  // should be zeroes

  std::array<u32, 20> ones;
  std::array<u32, 20> twos;
  std::array<u32, 20> threes;
  std::fill_n(ones.begin(), ones.size(), 1);
  std::fill_n(twos.begin(), ones.size(), 2);
  std::fill_n(threes.begin(), ones.size(), 3);

  // write 5 items, check
  // should be: [1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
  //             ^tail          ^ head
  rb.Write(ones.data(), 5);
  EXPECT_EQ(1u, rb[0]);
  EXPECT_EQ(1u, rb[4]);
  EXPECT_EQ(0u, rb[5]);
  EXPECT_EQ(5u, rb.LoadHead());
  EXPECT_EQ(0u, rb.LoadTail());

  // write 20 items, but since tail hasn't moved from 0,
  // should only write 11 items (current head is at 5)
  // should be: [1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2]
  //             ^tail,head
  rb.Write(twos.data(), 20);
  EXPECT_EQ(16u, rb.LoadHead());
  EXPECT_EQ(2u, rb[rb.LoadHead() - 1]);
  EXPECT_EQ(1u, rb[rb.LoadHead()]);

  // test store tail
  rb.StoreTail(5);
  EXPECT_EQ(5u, rb.LoadTail());
  // now: [1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2]
  //       ^head          ^tail

  // test tail fetchadd
  rb.FetchAddTail(5);
  EXPECT_EQ(10u, rb.LoadTail());
  // now: [1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2]
  //       ^head                         ^tail

  // test store head
  rb.StoreHead(26);
  EXPECT_EQ(26, rb.LoadHead());
  // now: [1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2]
  //                                     ^tail,head

  // circle write
  // start head at 26 and tail at 20
  // start: [1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2]
  //                     ^tail             ^head
  // writing 10 elements should wrap around 4 elements
  // head should be at 36
  rb.StoreTail(20);
  rb.Write(threes.data(), 10);
  EXPECT_EQ(3u, rb[0]);
  EXPECT_EQ(3u, rb[3]);
  EXPECT_EQ(3u, rb[10]);
  EXPECT_EQ(3u, rb[rb.MaxSize() - 1]);
  EXPECT_EQ(36u, rb.LoadHead());
  EXPECT_EQ(1u, rb[rb.LoadHead()]);
  EXPECT_EQ(1u, rb[rb.LoadTail()]);
  // end: [3, 3, 3, 3, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3]
  //                   ^head,tail
}