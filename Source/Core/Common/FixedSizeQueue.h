// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>

// STL-look-a-like interface, but name is mixed case to distinguish it clearly from the
// real STL classes.
//
// Not fully featured. Add features as needed.

template <class T, size_t N>
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

  // Copies over an array, loops over of num is greater than max_size
  void push_array(const T* t, size_t num)
  {
    size_t back_writable_num = std::min(tail_to_end(), num);
    memcpy(&back(), t, sizeof(T) * back_writable_num);
    t += back_writable_num;
    size_t readable_num = num - back_writable_num;
    while (readable_num > 0)
    {
      size_t beginning_writable_num = std::min(readable_num, N);
      memcpy(&beginning(), t, sizeof(T) * beginning_writable_num);
      t += beginning_writable_num;
      readable_num -= beginning_writable_num;
    }

    while (num > 0)
    {
      if (count == N)
        head = (head + 1) % N;
      else
        count++;
      tail = (tail + 1) % N;
      --num;
    }
  }

  // Takes a max num to copy (from head to tail), returns the actual copied number.
  // Doesn't loop over if num is greater than our size
  size_t copy_to_array(T* t, size_t num)
  {
    num = std::min(num, count);
    const size_t front_readable_num = std::min(head_to_end(), num);
    const size_t beginning_readable_num = num - front_readable_num;

    memcpy(&t[0], &front(), sizeof(T) * front_readable_num);
    if (beginning_readable_num > 0)
      memcpy(&t[front_readable_num], &beginning(), sizeof(T) * beginning_readable_num);
    return num;
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

  // From front
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

  // From front
  void erase(size_t num)
  {
    assert(count >= num);
    if constexpr (!std::is_trivial_v<T>)
    {
      while (num > 0)
      {
        storage[head] = {};
        head = (head + 1) % N;
        count--;
        num--;
      }
    }
    else
    {
      head = (head + num) % N;
      count -= num;
    }
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
  T& back() noexcept { return storage[tail]; }
  const T& back() const noexcept { return storage[tail]; }
  // Only use if head_to_end() > size()
  T& beginning() noexcept { return storage[0]; }
  const T& beginning() const noexcept { return storage[0]; }
  size_t size() const noexcept { return count; }
  size_t max_size() const noexcept { return N; }
  // Helper to know how many more samples we could read before needing to loop over
  size_t head_to_end() const noexcept { return N - head; }
  // Helper to know how many more samples we could write before needing to loop over
  size_t tail_to_end() const noexcept { return N - tail; }
  bool empty() const noexcept { return size() == 0; }

private:
  std::array<T, N> storage;
  size_t head = 0;
  size_t tail = 0;
  // Not necessary but avoids a lot of calculations
  size_t count = 0;
};
