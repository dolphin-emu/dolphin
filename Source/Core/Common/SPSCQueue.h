// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// a simple lockless thread-safe,
// single producer, single consumer queue

#include <algorithm>
#include <atomic>
#include <cstddef>

#include "Common/CommonTypes.h"

namespace Common
{
template <typename T, bool NeedSize = true>
class SPSCQueue
{
public:
  SPSCQueue() : m_size(0) { m_write_ptr = m_read_ptr = new ElementPtr(); }
  ~SPSCQueue()
  {
    // this will empty out the whole queue
    delete m_read_ptr;
  }

  u32 Size() const
  {
    static_assert(NeedSize, "using Size() on SPSCQueue without NeedSize");
    return m_size.load();
  }

  bool Empty() const { return !m_read_ptr->next.load(); }
  T& Front() const { return m_read_ptr->current; }
  template <typename Arg>
  void Push(Arg&& t)
  {
    // create the element, add it to the queue
    m_write_ptr->current = std::forward<Arg>(t);
    // set the next pointer to a new element ptr
    // then advance the write pointer
    ElementPtr* new_ptr = new ElementPtr();
    m_write_ptr->next.store(new_ptr, std::memory_order_release);
    m_write_ptr = new_ptr;
    if (NeedSize)
      m_size++;
  }

  void Pop()
  {
    if (NeedSize)
      m_size--;
    ElementPtr* tmpptr = m_read_ptr;
    // advance the read pointer
    m_read_ptr = tmpptr->next.load();
    // set the next element to nullptr to stop the recursive deletion
    tmpptr->next.store(nullptr);
    delete tmpptr;  // this also deletes the element
  }

  bool Pop(T& t)
  {
    if (Empty())
      return false;

    if (NeedSize)
      m_size--;

    ElementPtr* tmpptr = m_read_ptr;
    m_read_ptr = tmpptr->next.load(std::memory_order_acquire);
    t = std::move(tmpptr->current);
    tmpptr->next.store(nullptr);
    delete tmpptr;
    return true;
  }

  // not thread-safe
  void Clear()
  {
    m_size.store(0);
    delete m_read_ptr;
    m_write_ptr = m_read_ptr = new ElementPtr();
  }

private:
  // stores a pointer to element
  // and a pointer to the next ElementPtr
  class ElementPtr
  {
  public:
    ElementPtr() : next(nullptr) {}
    ~ElementPtr()
    {
      if (ElementPtr* next_ptr = next.load())
        delete next_ptr;
    }

    T current{};
    std::atomic<ElementPtr*> next;
  };

  ElementPtr* m_write_ptr;
  ElementPtr* m_read_ptr;
  std::atomic<u32> m_size;
};
}  // namespace Common
