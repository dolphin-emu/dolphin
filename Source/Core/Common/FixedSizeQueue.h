// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

// STL-look-a-like interface, but name is mixed case to distinguish it clearly from the
// real STL classes.
//
// Not fully featured, no safety checking yet. Add features as needed.

template <class T, unsigned int N>
class FixedSizeQueue
{
public:
  void clear()
  {
    if constexpr (!std::is_trivial_v<T>)
    {
      // The clear of non-trivial objects previously used "storage = {}". However, this causes GCC
      // to take a very long time to compile the file/function, as well as generating huge amounts
      // of debug information (~2GB object file, ~600MB of debug info).
      while (count > 0)
        pop();
    }

    head = 0;
    tail = 0;
    count = 0;
  }

  void push(T t)
  {
    if (count == N)
      head = (head + 1) % N;
    else
      count++;

    storage[tail] = std::move(t);
    tail = (tail + 1) % N;
  }

  template <class... Args>
  void emplace(Args&&... args)
  {
    if (count == N)
      head = (head + 1) % N;
    else
      count++;

    storage[tail] = T(std::forward<Args>(args)...);
    tail = (tail + 1) % N;
  }

  void pop()
  {
    assert(count > 0);
    if constexpr (!std::is_trivial_v<T>)
      storage[head] = {};

    head = (head + 1) % N;
    count--;
  }

  T pop_front()
  {
    T temp = std::move(front());
    pop();
    return temp;
  }

  T& operator[](size_t index)
  {
    assert(index < count);
    return storage[(head + index) % N];
  }
  const T& operator[](size_t index) const
  {
    assert(index < count);
    return storage[(head + index) % N];
  }

  T& front() noexcept { return storage[head]; }
  const T& front() const noexcept { return storage[head]; }
  size_t size() const noexcept { return count; }
  size_t max_size() const noexcept { return N; }
  bool empty() const noexcept { return size() == 0; }

private:
  std::array<T, N> storage;
  unsigned int head = 0;
  unsigned int tail = 0;
  // Not necessary but avoids a lot of calculations
  unsigned int count = 0;
};
