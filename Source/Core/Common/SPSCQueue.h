// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// a simple lockless thread-safe,
// single producer, single consumer queue

#include <atomic>
#include <cassert>
#include <memory>

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
    std::construct_at(&m_write_ptr->value.data, std::forward<Args>(args)...);

    Node* const new_ptr = new Node;
    m_write_ptr->next = new_ptr;
    m_write_ptr = new_ptr;

    AdjustSize(1);
  }

  void WaitForEmpty() requires(IncludeWaitFunctionality)
  {
    while (const std::size_t old_size = Size())
      m_size.wait(old_size, std::memory_order_acquire);
  }

  // The following are only safe from the "consumer thread":
  T& Front() { return m_read_ptr->value.data; }
  const T& Front() const { return m_read_ptr->value.data; }

  void Pop()
  {
    assert(!Empty());

    std::destroy_at(&Front());

    Node* const old_node = m_read_ptr;
    m_read_ptr = old_node->next;
    delete old_node;

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

  void WaitForData() requires(IncludeWaitFunctionality)
  {
    m_size.wait(0, std::memory_order_acquire);
  }

  void Clear()
  {
    while (!Empty())
      Pop();
  }

private:
  struct Node
  {
    // union allows value construction to be deferred until Push.
    union Value
    {
      T data;
      Value() {}
      ~Value() {}
    } value;

    Node* next;
  };

  Node* m_write_ptr = new Node;
  Node* m_read_ptr = m_write_ptr;

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
