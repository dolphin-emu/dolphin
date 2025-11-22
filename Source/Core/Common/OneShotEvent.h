// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <memory>
#include <semaphore>

#include "Common/Functional.h"

namespace Common
{

// A one-time-use single-producer single-consumer thread synchronization class.
// Safe when `Set` will cause a `Wait`ing thread to immediately destruct the event itself.
class OneShotEvent
{
public:
  // One thread should call Set() exactly once.
  void Set() { m_semaphore.release(1); }

  // One thread may call Wait() once or WaitFor() until it returns true.

  void Wait() { m_semaphore.acquire(); }

  template <typename Rep, typename Period>
  bool WaitFor(const std::chrono::duration<Rep, Period>& rel_time)
  {
    return m_semaphore.try_acquire_for(rel_time);
  }

private:
  std::binary_semaphore m_semaphore{0};
};

// Invokes Set() on the given object upon destruction.
template <typename EventType>
class ScopedSetter
{
public:
  ScopedSetter() = default;
  explicit ScopedSetter(EventType* ptr) : m_ptr{ptr} {}

  // Forgets the object without invoking Set().
  void Dismiss() { m_ptr.release(); }

private:
  // Leveraging unique_ptr conveniently makes this class move-only.
  // It does no actual deletion, just calls Set().
  using NonOwningSetOnDeletePtr = std::unique_ptr<EventType, InvokerOf<&EventType::Set>>;
  NonOwningSetOnDeletePtr m_ptr;
};

}  // namespace Common
