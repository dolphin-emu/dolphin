// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// atomic<shared_ptr<T>> is heavy and has limited compiler availability.
// atomic<unique_ptr<T>> isn't feasible.

// This class provides something similar to a would-be atomic unique_ptr.

#include <atomic>
#include <memory>

namespace Common
{

template <typename T>
class AtomicUniquePtr
{
public:
  using value_type = std::unique_ptr<T>;

  constexpr AtomicUniquePtr() = default;
  constexpr ~AtomicUniquePtr() { store(nullptr); }

  AtomicUniquePtr(const AtomicUniquePtr&) = delete;
  constexpr AtomicUniquePtr(std::nullptr_t) {}
  AtomicUniquePtr(value_type ptr) { store(std::move(ptr)); }

  void operator=(const AtomicUniquePtr&) = delete;
  void operator=(std::nullptr_t) { store(nullptr); }
  void operator=(value_type ptr) { store(std::move(ptr)); }

  void store(value_type desired, std::memory_order order = std::memory_order_seq_cst)
  {
    // A unique_ptr is returned and appropriately destructed.
    exchange(std::move(desired), order);
  }

  // There is notably no "load" function. It wouldn't be semantically possible.
  // exchange(nullptr) can be used to get the pointer.

  value_type exchange(value_type desired, std::memory_order order = std::memory_order_seq_cst)
  {
    // Using fences to ensure the pointed at data is visible even with std::memory_order_relaxed.
    std::atomic_thread_fence(std::memory_order_release);
    value_type result = value_type{m_ptr.exchange(desired.release(), order)};
    std::atomic_thread_fence(std::memory_order_acquire);
    return result;
  }

private:
  std::atomic<T*> m_ptr = nullptr;
};

}  // namespace Common
