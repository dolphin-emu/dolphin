// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <atomic>
#include <vector>
#include "Common/CommonTypes.h"

// Simple Ring Buffer class.
// Only implemented bulk data writing.
// Add functions as necessary.

template <class T>
class RingBuffer
{
public:
  RingBuffer() : m_size(0), m_mask(0) {}

  RingBuffer(u32 size) : m_size(size), m_mask(size - 1)
  {
    m_data.resize(size);
  }

  ~RingBuffer() {}

  // Resize resets head and tail
  void Resize(u32 size)
  {
    m_size = size;
    m_mask = size - 1;
    m_data.resize(size);
    m_head.store(0);
    m_tail.store(0);
  }

  T& operator[](u32 pos)
  {
    return m_data[pos & m_mask];
  }

  constexpr const T& operator[](u32 pos) const
  {
    return m_data[pos & m_mask];
  }

  void Write(const T* source, u32 length)
  {
    u32 head = m_head.load();
    if (length + ((head - m_tail.load()) & m_mask) >= m_size)
      return;       // if writing data will cause head to overtake tail, exit
                    // don't know if this is best default behavior...
                    // maybe write up to tail would be better?

    // calculate if we need wraparound
    s32 over = length - (m_size - (head & m_mask));

    if (over > 0)
    {
      memcpy(&m_data[head & m_mask], source, (length - over) * sizeof(T));
      memcpy(&m_data[0], source + (length - over), over * sizeof(T));
    }
    else
    {
      memcpy(&m_data[head & m_mask], source, length * sizeof(T));
    }

    m_head.fetch_add(length);
  }

  u32 LoadHead() const
  {
    return m_head.load();
  }

  u32 LoadTail() const
  {
    return m_tail.load();
  }

  void StoreHead(const u32 pos)
  {
    m_head.store(pos);
  }

  void FetchAddTail(const u32 count)
  {
    m_tail.fetch_add(count);
  }

  void StoreTail(const u32 pos)
  {
    m_tail.store(pos);
  }

  u32 m_size;
  u32 m_mask;

private:
  std::vector<T> m_data;
  std::atomic<u32> m_head{ 0 };
  std::atomic<u32> m_tail{ 0 };
};