// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <thread>

#include "Common/CircularQueue.h"

TEST(CircularBuffer, Simple)
{
  Common::CircularArray<int, 333> buffer;

  EXPECT_EQ(buffer.Empty(), true);

  {
    Common::CircularQueue<int, std::vector<int>> vec_buffer{444};
    EXPECT_EQ(vec_buffer.Size(), 0);
    EXPECT_EQ(vec_buffer.Capacity(), 444);
  }

  constexpr int reps = 13;
  constexpr int range_size = 11;
  constexpr int pushpop_count = 2;

  constexpr int magic_value = 7;

  EXPECT_EQ(buffer.Writable(), buffer.Capacity());

  for (int i = 0; i != int(buffer.Capacity()); ++i)
    buffer.Push(0);

  EXPECT_EQ(buffer.Writable(), 0);

  // Pushing beyond capacity wraps around.
  for (int i = 0; i != int(buffer.Capacity()); ++i)
    buffer.Push(magic_value);

  EXPECT_EQ(buffer.Writable(), 0);

  for (int i = 0; i != 31; ++i)
    buffer.RewindReadHead(11);
  for (int i = 0; i != 31; ++i)
    buffer.AdvanceReadHead(11);

  EXPECT_EQ(buffer.Readable(), buffer.Capacity() * 2);

  for (int i = 0; i != int(buffer.Capacity()) * 2; ++i)
  {
    EXPECT_EQ(buffer.GetReadHead(), magic_value);
    buffer.AdvanceReadHead(1);
  }

  EXPECT_EQ(buffer.Empty(), true);
  EXPECT_EQ(buffer.Readable(), 0);

  std::array<int, range_size> fixed_data{};
  std::ranges::copy_n(std::views::iota(0).begin(), range_size, fixed_data.begin());

  for (int r = 0; r != reps; ++r)
  {
    for (int i = 0; i != pushpop_count; ++i)
    {
      buffer.PushRange(fixed_data);
      EXPECT_EQ(buffer.Size(), (i + 1) * range_size);
    }

    for (int i = 0; i != pushpop_count; ++i)
    {
      std::array<int, range_size> data{};
      buffer.PopRange(&data);
      for (int c = 0; c != range_size; ++c)
        EXPECT_EQ(data[c], fixed_data[c]);
    }

    EXPECT_EQ(buffer.Size(), 0);
  }
}

TEST(CircularBuffer, MultiThreaded)
{
  Common::WaitableSPSCCircularArray<int, 29> buffer;

  constexpr int reps = 13;
  constexpr int pushpop_count = 97;

  auto inserter = [&]() {
    for (int r = 0; r != reps; ++r)
    {
      for (int i = 0; i != pushpop_count; ++i)
      {
        buffer.WaitForWritable(1);
        buffer.GetWriteHead() = i;
        buffer.AdvanceWriteHead(1);

        buffer.WaitForWritable(1);
        buffer.Push(i);

        buffer.WaitForWritable(1);
        buffer.GetWriteSpan().front() = i;
        buffer.AdvanceWriteHead(1);
      }
    }
  };

  auto popper = [&]() {
    for (int r = 0; r != reps; ++r)
    {
      for (int i = 0; i != pushpop_count; ++i)
      {
        buffer.WaitForReadable(3);
        // Pop works
        EXPECT_EQ(buffer.Pop(), i);
        // Manual read head works.
        EXPECT_EQ(buffer.GetReadHead(), i);
        buffer.AdvanceReadHead(1);
        EXPECT_EQ(buffer.GetReadSpan().front(), i);
        buffer.AdvanceReadHead(1);
      }
    }
    EXPECT_EQ(buffer.Empty(), true);
  };

  std::thread popper_thread(popper);
  std::thread inserter_thread(inserter);

  popper_thread.join();
  inserter_thread.join();
}
