// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>

namespace Common
{
// Behaves like std::shared_mutex but locks and unlocks may come from different threads.
class TransferableSharedMutex
{
public:
  void lock() { Lock<true>(); }
  bool try_lock() { return Lock<false>(); }
  void unlock()
  {
    m_counter.store(0, std::memory_order_release);
    m_counter.notify_all();
  }

  void lock_shared() { LockShared<true>(); }
  bool try_lock_shared() { return LockShared<false>(); }
  void unlock_shared()
  {
    if (m_counter.fetch_sub(1, std::memory_order_release) == 1)
      m_counter.notify_all();
  }

private:
  using CounterType = std::uintptr_t;
  static constexpr auto EXCLUSIVE_LOCK_VALUE = CounterType(-1);

  std::atomic<CounterType> m_counter{};

  template <bool Blocking>
  bool Lock()
  {
    while (true)
    {
      auto old_value = CounterType{};
      if (m_counter.compare_exchange_strong(old_value, EXCLUSIVE_LOCK_VALUE,
                                            std::memory_order_acquire, std::memory_order_relaxed))
      {
        return true;
      }

      // lock() or lock_shared() is already held.
      // If blocking, wait for an unlock notification and try again, else fail.
      if constexpr (Blocking)
        m_counter.wait(old_value, std::memory_order_relaxed);
      else
        return false;
    }
  }

  template <bool Blocking>
  bool LockShared()
  {
    while (true)
    {
      auto old_value = m_counter.load(std::memory_order_relaxed);
      while (old_value != EXCLUSIVE_LOCK_VALUE)
      {
        // Only (max() - 1) shared locks can be held.
        assert(old_value != EXCLUSIVE_LOCK_VALUE - 1);

        if (m_counter.compare_exchange_weak(old_value, old_value + 1, std::memory_order_acquire,
                                            std::memory_order_relaxed))
        {
          return true;
        }
      }

      // lock() is already held.
      // If blocking, wait for an unlock notification and try again, else fail.
      if constexpr (Blocking)
        m_counter.wait(old_value, std::memory_order_relaxed);
      else
        return false;
    }
  }
};

}  // namespace Common
