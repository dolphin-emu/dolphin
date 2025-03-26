// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// a simple lockless thread-safe,
// single producer, single consumer queue

#include <atomic>
#include <cassert>
#include <memory>

#if defined(__APPLE__)
#include <condition_variable>
// std::atomic<T>::notify_one is "unavailable: introduced in macOS 11.0"
// TODO: Remove this impl when macOS users can use an OS from this decade.
#define WaitableAtomicSize_use_macos_impl
#endif

namespace Common
{
namespace detail
{
class AtomicSize
{
public:
  std::size_t Size() const { return m_size.load(std::memory_order_acquire); }

protected:
  void IncSize() { m_size.fetch_add(1, std::memory_order_release); }
  void DecSize() { m_size.fetch_sub(1, std::memory_order_release); }

  std::atomic<std::size_t> m_size = 0;
};

#if defined(WaitableAtomicSize_use_macos_impl)
class WaitableAtomicSize : public AtomicSize
{
public:
  void WaitForEmpty()
  {
    std::unique_lock lg{m_lock};
    m_cond.wait(lg, [this] { return Size() == 0; });
  }

  void WaitForData()
  {
    std::unique_lock lg{m_lock};
    m_cond.wait(lg, [this] { return Size() != 0; });
  }

protected:
  void IncSize()
  {
    std::unique_lock lg{m_lock};
    AtomicSize::IncSize();
    m_cond.notify_one();
  }

  void DecSize()
  {
    std::unique_lock lg{m_lock};
    AtomicSize::DecSize();
    m_cond.notify_one();
  }

private:
  std::condition_variable m_cond;
  std::mutex m_lock;
};
#else
class WaitableAtomicSize : public AtomicSize
{
public:
  void WaitForEmpty()
  {
    while (auto const old_size = Size())
      m_size.wait(old_size, std::memory_order_acquire);
  }

  void WaitForData() { m_size.wait(0, std::memory_order_acquire); }

protected:
  void IncSize()
  {
    AtomicSize::IncSize();
    m_size.notify_one();
  }

  void DecSize()
  {
    AtomicSize::DecSize();
    m_size.notify_one();
  }
};
#endif
};  // namespace detail

template <typename T, typename SizeBase = detail::AtomicSize>
class SPSCQueue final : public SizeBase
{
public:
  SPSCQueue() = default;
  ~SPSCQueue()
  {
    Clear();
    delete m_read_ptr;
  }

  SPSCQueue(const SPSCQueue&) = delete;
  SPSCQueue& operator=(const SPSCQueue&) = delete;

  bool Empty() const { return SizeBase::Size() == 0; }

  // The following are only safe from the "producer thread":
  void Push(const T& arg) { Emplace(arg); }
  void Push(T&& arg) { Emplace(std::move(arg)); }
  template <typename... Args>
  void Emplace(Args&&... args)
  {
    ::new (&m_write_ptr->value.data) T{std::forward<Args>(args)...};

    Node* const new_ptr = new Node;
    m_write_ptr->next = new_ptr;
    m_write_ptr = new_ptr;

    SizeBase::IncSize();
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

    SizeBase::DecSize();
  }

  bool Pop(T& result)
  {
    if (Empty())
      return false;

    result = std::move(Front());
    Pop();
    return true;
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
};

template <typename T>
using WaitableSPSCQueue = SPSCQueue<T, detail::WaitableAtomicSize>;

}  // namespace Common
