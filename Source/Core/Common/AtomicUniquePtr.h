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
  using UniquePtr = std::unique_ptr<T>;
  using RawPtr = T*;

  AtomicUniquePtr() = default;
  ~AtomicUniquePtr() { Store(nullptr); }

  AtomicUniquePtr(const AtomicUniquePtr&) = delete;
  void operator=(const AtomicUniquePtr&) = delete;

  explicit AtomicUniquePtr(std::nullptr_t) {}
  explicit AtomicUniquePtr(UniquePtr ptr) { Store(std::move(ptr)); }

  void operator=(std::nullptr_t) { Store(nullptr); }
  void operator=(UniquePtr ptr) { Store(std::move(ptr)); }

  void Store(UniquePtr desired)
  {
    // A unique_ptr is returned and appropriately destructed here.
    Exchange(std::move(desired));
  }

  UniquePtr Exchange(UniquePtr desired)
  {
    return UniquePtr{m_ptr.exchange(desired.release(), std::memory_order_acq_rel)};
  }

private:
  std::atomic<RawPtr> m_ptr = nullptr;
};

}  // namespace Common
