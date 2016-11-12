// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <atomic>
#include <cstring>
#include <vector>

#include "Common/MathUtil.h"

// Simple Ring Buffer class.
// Since we use bitwise & for circularity for speed, m_max_size will be adjusted to power of 2.
// Only implemented bulk data writing.
// Add functions as necessary.
template <class T>
class RingBuffer
{
public:
  RingBuffer() = default;
  explicit RingBuffer(size_t max_size) : m_max_size(NextIntPow2(max_size)), m_mask(m_max_size - 1)
  {
    m_data.resize(m_max_size);
  }
  ~RingBuffer() = default;
  // Resize resets head and tail
  void Resize(size_t max_size)
  {
    m_max_size = NextIntPow2(max_size);
    m_mask = m_max_size - 1;
    m_data.resize(m_max_size);
    m_head.store(0);
    m_tail.store(0);
  }

  T& operator[](size_t pos) { return m_data[pos & m_mask]; }
  const T operator[](size_t pos) const { return m_data[pos & m_mask]; }
  // will only write up to tail if write length is too big
  void Write(const T* source, size_t length)
  {
    size_t head = m_head.load();
    size_t adjusted = std::min(length, m_max_size - (head - m_tail.load()));
    // calculate if we need wraparound
    signed long long over = adjusted - (m_max_size - (head & m_mask));
    if (over > 0)
    {
      memcpy(&m_data[head & m_mask], source, (adjusted - over) * sizeof(T));
      memcpy(&m_data[0], source + (adjusted - over), over * sizeof(T));
    }
    else
    {
      memcpy(&m_data[head & m_mask], source, adjusted * sizeof(T));
    }
    m_head.fetch_add(adjusted);
  }

  size_t LoadHead() const { return m_head.load(); }
  size_t LoadTail() const { return m_tail.load(); }
  void StoreHead(const size_t pos) { m_head.store(pos); }
  void FetchAddTail(const size_t count) { m_tail.fetch_add(count); }
  void StoreTail(const size_t pos) { m_tail.store(pos); }
  size_t MaxSize() const { return m_max_size; }
  size_t Mask() const { return m_mask; }
private:
  std::vector<T> m_data;
  std::atomic<size_t> m_head{0};
  std::atomic<size_t> m_tail{0};

  size_t m_max_size;
  size_t m_mask;
};