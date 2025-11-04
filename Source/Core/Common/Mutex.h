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
  void lock()
  {
    while (m_lock.exchange(true, std::memory_order_acquire))
    {
      if constexpr (UseAtomicWait)
        m_lock.wait(true, std::memory_order_relaxed);
    }
  }

  bool try_lock()
  {
    bool expected = false;
    return m_lock.compare_exchange_weak(expected, true, std::memory_order_acquire,
                                        std::memory_order_relaxed);
  }

  // Unlike with std::mutex, this call may come from any thread.
  void unlock()
  {
    m_lock.store(false, std::memory_order_release);
    if constexpr (UseAtomicWait)
      m_lock.notify_one();
  }

private:
  std::atomic_bool m_lock{};
};
}  // namespace detail

// Sometimes faster than std::mutex.
using AtomicMutex = detail::AtomicMutexBase<true>;

// Very fast to lock and unlock when uncontested (~3x faster than std::mutex).
using SpinMutex = detail::AtomicMutexBase<false>;

}  // namespace Common
