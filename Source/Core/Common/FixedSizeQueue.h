// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

// STL-look-a-like interface, but name is mixed case to distinguish it clearly from the
// real STL classes.
//
// Not fully featured, no safety checking yet. Add features as needed.

namespace Common
{
template <class T, int N>
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

  T& front() noexcept { return storage[head]; }
  const T& front() const noexcept { return storage[head]; }
  size_t size() const noexcept { return count; }
  bool empty() const noexcept { return size() == 0; }

private:
  std::array<T, N> storage{};
  int head = 0;
  int tail = 0;
  // Sacrifice 4 bytes for a simpler implementation. may optimize away in the future.
  int count = 0;
};
}  // namespace Common
