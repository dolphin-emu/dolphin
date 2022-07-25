// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>

namespace Common
{
// Reference Counting smart pointer
// This is a more lightweight version of std::shared_ptr, that doesn't support weak_ptr.
//
// Supporting weak_ptr actually adds a lot of cost to the smart_ptr destructor, as it
// needs to avoid the race conditions where one thread manages to lock a weak_ptr while
// another thread has deleted the last smart_ptr and is running the destructor.
//
// By not supporting weak_pointers, our destructor can be much simpler.
// Code is expected to use raw pointers instead, and manually control lifetime.
//
// Other changes and simplifications over std::shared_ptr:
//  - can't construct from raw pointer, must use make_rc<T>()
//    This allows the reference count and object to always share an allocation,
//    and the destructor doesn't need to dynamically calculate how many
//    allocations to delete.
//  - No constructing from unique_ptr or auto_pointer
//  - New static ::make() method, equivalent to make_rc<T>()
//
//
// There are also many feature that are simply not implemented (like custom deleters, arrays, swap)
//
// Memory Ordering:
//  - Guarantees the object's destructor will see all stores before the
//    reset()/destruction on other threads.
//
template <typename T>
class rc_ptr
{
  using element_type = std::remove_extent_t<T>;
  struct Storage
  {
    std::atomic_uint32_t m_count;
    T m_data;

    template <typename... Args>
    Storage(Args&&... args) : m_count(1), m_data(std::forward<Args>(args)...)
    {
    }
  };
  Storage* m_storage;

  constexpr rc_ptr(Storage* storage) : m_storage(storage) {}

  // template <typename _T, typename... Args>
  // friend constexpr rc_ptr<_T> make_rc(Args&&... args) noexcept;

  constexpr void _inc()
  {
    // Using relaxed memory ordering gives the compilers some freedom to optimize around
    // these atomic adds. Future compilers might even optimize them away, combining them
    // with other atomics: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4455.html
    m_storage->m_count.fetch_add(1, std::memory_order_relaxed);
  }
  constexpr void _reset()
  {
    if (m_storage)
    {
      // we release, so all stores on this thread will be visible to the thread that does the delete
      if (m_storage->m_count.fetch_sub(1, std::memory_order_release) == 1)
      {
        // this acquire fence ensures that any stores released by other threads will be visible.
        // Compiles to nothing on x86, and dmb ishld on ARMv8
        std::atomic_thread_fence(std::memory_order_acquire);

        // Unlike std::shared_ptr, we don't need this ordering guarantees for our own control block,
        // a relaxed fetch_sub would be enough. But std::shared_ptr implicitly passes this guarantee
        // on to the destructor, so better safe than sorry.

        // TODO: Should we switch to a more relaxed ordering when T only has a trivial destructor?
        delete m_storage;
      }
    }
  }

public:
  template <typename... Args>
  static rc_ptr<T> make(Args&&... args) noexcept
  {  // static make method
    return rc_ptr<T>(new Storage(std::forward<Args>(args)...));
  }

  constexpr rc_ptr(rc_ptr&& other) noexcept : m_storage(other.m_storage)  // Move Constructor
  {
    other.m_storage = nullptr;
  }
  constexpr rc_ptr(const rc_ptr& other) noexcept : m_storage(other.m_storage)  // Copy Constructor
  {
    _inc();
  }
  constexpr void operator=(rc_ptr&& other) noexcept  // Move Assign
  {
    _reset();
    m_storage = std::move(other.m_storage);
    other.m_storage = nullptr;
  }
  constexpr void operator=(const rc_ptr& other) noexcept  // Assign
  {
    _reset();
    m_storage = other.m_storage;
    _inc();
  }
  constexpr rc_ptr() noexcept : m_storage(nullptr) {}

  // TODO: can be constexpr in C++20
  ~rc_ptr()
  {  // Destructor
    _reset();
    // Don't bother nulling m_storage, some compilers don't optimize it out
  }

  constexpr element_type* get() const noexcept
  {
    if (!m_storage)
      return nullptr;
    return &m_storage->m_data;
  }

  // "The behavior is undefined if the stored pointer is null"
  constexpr element_type& operator*() const noexcept { return m_storage->m_data; }
  constexpr element_type* operator->() const noexcept { return &m_storage->m_data; }

  // We can't store a nullptr, so this is simpler than std::shared_ptr
  constexpr operator bool() const { return m_storage != nullptr; }

  constexpr bool operator==(const rc_ptr& other) const { return m_storage == other.m_storage; }
  constexpr bool operator!=(const rc_ptr& other) const { return m_storage != other.m_storage; }

  constexpr void reset() noexcept
  {
    _reset();
    m_storage = nullptr;
  }

  constexpr long use_count() const
  {
    // because increment uses relaxed memory ordering, this count isn't 100% accurate
    // std::shared_ptr, has the same deficiency.
    return m_storage ? m_storage->m_count.load(std::memory_order_relaxed) : 0;
  }
};

template <typename T, typename... Args>
constexpr rc_ptr<T> make_rc(Args&&... args) noexcept
{
  return rc_ptr<T>::make(std::forward<Args>(args)...);
}

}  // namespace Common
