// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>

namespace Common
{
namespace detail
{
template <bool UseAtomicWait>
class AtomicMutexBase
{
public:
  void lock() { Lock<true>(); }
  bool try_lock() { return Lock<false>(); }
  void unlock()
  {
    m_lock.store(false, std::memory_order_release);
    if constexpr (UseAtomicWait)
      m_lock.notify_one();
  }

private:
  template <bool Blocking>
  bool Lock()
  {
    while (true)
    {
      bool expected = false;
      if (m_lock.compare_exchange_weak(expected, true, std::memory_order_acquire,
                                       std::memory_order_relaxed))
      {
        return true;
      }

      if (!expected)
        continue;  // Spurious failure.

      if constexpr (Blocking)
      {
        if constexpr (UseAtomicWait)
          m_lock.wait(true, std::memory_order_relaxed);
      }
      else
      {
        return false;
      }
    }
  }

  std::atomic_bool m_lock;
};
}  // namespace detail

// Sometimes faster than std::mutex.
using AtomicMutex = detail::AtomicMutexBase<true>;

// Very fast to lock and unlock when uncontested.
using SpinMutex = detail::AtomicMutexBase<false>;

}  // namespace Common
