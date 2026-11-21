// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// a simple lockless thread-safe,
// single producer, single consumer queue

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>

#include "Common/TypeUtils.h"

namespace Common
{

namespace detail
{
template <typename T, bool IncludeWaitFunctionality>
class SPSCQueueBase final
{
public:
  SPSCQueueBase() = default;
  ~SPSCQueueBase()
  {
    Clear();
    delete m_read_ptr;
  }

  SPSCQueueBase(const SPSCQueueBase&) = delete;
  SPSCQueueBase& operator=(const SPSCQueueBase&) = delete;

  std::size_t Size() const { return m_size.load(std::memory_order_acquire); }
  bool Empty() const { return Size() == 0; }

  // The following are only safe from the "producer thread":
  void Push(const T& arg) { Emplace(arg); }
  void Push(T&& arg) { Emplace(std::move(arg)); }
  template <typename... Args>
  void Emplace(Args&&... args)
  {
    m_write_ptr->values[m_write_pos].Construct(std::forward<Args>(args)...);

    if (++m_write_pos == ITEMS_PER_NODE)
    {
      m_write_pos = 0;
      Node* const new_ptr = new Node;
      m_write_ptr->next = new_ptr;
      m_write_ptr = new_ptr;
    }

    AdjustSize(1);
  }

  void WaitForEmpty() requires IncludeWaitFunctionality
  {
    while (const std::size_t old_size = Size())
      m_size.wait(old_size, std::memory_order_acquire);
  }

  // The following are only safe from the "consumer thread":
  T& Front() { return m_read_ptr->values[m_read_pos].Ref(); }
  const T& Front() const { return m_read_ptr->values[m_read_pos].Ref(); }

  void Pop()
  {
    assert(!Empty());

    m_read_ptr->values[m_read_pos].Destroy();

    if (++m_read_pos == ITEMS_PER_NODE)
    {
      m_read_pos = 0;
      Node* const old_node = m_read_ptr;
      m_read_ptr = old_node->next;
      delete old_node;
    }

    AdjustSize(-1);
  }

  bool Pop(T& result)
  {
    if (Empty())
      return false;

    result = std::move(Front());
    Pop();
    return true;
  }

  void WaitForData() requires IncludeWaitFunctionality
  {
    m_size.wait(0, std::memory_order_acquire);
  }

  void Clear()
  {
    while (!Empty())
      Pop();
  }

private:
  static constexpr std::size_t BYTES_PER_NODE = 1024;
  static constexpr std::size_t ITEMS_PER_NODE = std::max(BYTES_PER_NODE / sizeof(T), 1uz);

  struct Node
  {
    std::array<ManuallyConstructedValue<T>, ITEMS_PER_NODE> values;
    Node* next;
  };

  Node* m_write_ptr = new Node;
  std::size_t m_write_pos = 0;

  Node* m_read_ptr = m_write_ptr;
  std::size_t m_read_pos = 0;

  void AdjustSize(std::size_t value)
  {
    m_size.fetch_add(value, std::memory_order_release);
    if constexpr (IncludeWaitFunctionality)
      m_size.notify_one();
  }

  std::atomic<std::size_t> m_size = 0;
};
}  // namespace detail

template <typename T>
using SPSCQueue = detail::SPSCQueueBase<T, false>;

template <typename T>
using WaitableSPSCQueue = detail::SPSCQueueBase<T, true>;

}  // namespace Common
