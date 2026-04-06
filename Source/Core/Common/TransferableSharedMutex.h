// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>

namespace Common
{
// Behaves like `std::shared_mutex` but locks and unlocks may come from different threads.
class TransferableSharedMutex
{
public:
  void lock()
  {
    while (true)
    {
      CounterType old_value{};
      if (m_counter.compare_exchange_strong(old_value, EXCLUSIVE_LOCK_VALUE,
                                            std::memory_order_acquire, std::memory_order_relaxed))
      {
        return;
      }

      // lock() or lock_shared() is already held.
      // Wait for an unlock notification and try again.
      m_counter.wait(old_value, std::memory_order_relaxed);
    }
  }

  bool try_lock()
  {
    CounterType old_value{};
    return m_counter.compare_exchange_weak(old_value, EXCLUSIVE_LOCK_VALUE,
                                           std::memory_order_acquire, std::memory_order_relaxed);
  }

  void unlock()
  {
    m_counter.store(0, std::memory_order_release);
    m_counter.notify_all();  // Notify potentially multiple wait()ers in lock_shared().
  }

  void lock_shared()
  {
    while (true)
    {
      auto old_value = m_counter.load(std::memory_order_relaxed);
      while (old_value < LAST_SHARED_LOCK_VALUE)
      {
        if (m_counter.compare_exchange_strong(old_value, old_value + 1, std::memory_order_acquire,
                                              std::memory_order_relaxed))
        {
          return;
        }
      }

      // Something has gone very wrong if m_counter is nearly saturated with shared_lock().
      assert(old_value != LAST_SHARED_LOCK_VALUE);

      // lock() is already held.
      // Wait for an unlock notification and try again.
      m_counter.wait(old_value, std::memory_order_relaxed);
    }
  }

  bool try_lock_shared()
  {
    auto old_value = m_counter.load(std::memory_order_relaxed);
    return (old_value < LAST_SHARED_LOCK_VALUE) &&
           m_counter.compare_exchange_weak(old_value, old_value + 1, std::memory_order_acquire,
                                           std::memory_order_relaxed);
  }

  void unlock_shared()
  {
    if (m_counter.fetch_sub(1, std::memory_order_release) == 1)
      m_counter.notify_one();  // Notify one of the wait()ers in lock().
  }

private:
  using CounterType = std::uintptr_t;

  static constexpr auto EXCLUSIVE_LOCK_VALUE = CounterType(-1);
  static constexpr auto LAST_SHARED_LOCK_VALUE = EXCLUSIVE_LOCK_VALUE - 1;

  std::atomic<CounterType> m_counter{};
};

}  // namespace Common
