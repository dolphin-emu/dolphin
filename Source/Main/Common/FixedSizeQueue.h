// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <utility>

// STL-look-a-like interface, but name is mixed case to distinguish it clearly from the
// real STL classes.
//
// Not fully featured, no safety checking yet. Add features as needed.

template <class T, int N>
class FixedSizeQueue
{
public:
  void clear()
  {
    head = 0;
    tail = 0;
    count = 0;
  }

  void push(T t)
  {
    storage[tail] = std::move(t);
    tail++;
    if (tail == N)
      tail = 0;
    count++;
  }

  void pop()
  {
    head++;
    if (head == N)
      head = 0;
    count--;
  }

  T pop_front()
  {
    T& temp = storage[head];
    pop();
    return std::move(temp);
  }

  T& front() { return storage[head]; }
  const T& front() const { return storage[head]; }
  size_t size() const { return count; }

private:
  std::array<T, N> storage;
  int head = 0;
  int tail = 0;
  // Sacrifice 4 bytes for a simpler implementation. may optimize away in the future.
  int count = 0;
};
