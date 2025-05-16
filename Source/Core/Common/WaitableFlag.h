// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// class that allows threads to wait for a bool to take on a value.

#pragma once

#include <atomic>

namespace Common
{
class WaitableFlag final
{
public:
  void Set(bool value = true)
  {
    m_flag.store(value, std::memory_order_release);
    m_flag.notify_all();
  }

  void Wait(bool expected_value) { m_flag.wait(!expected_value, std::memory_order_acquire); }

  // Note that this does not awake Wait'ing threads. Use Set(false) if that's needed.
  void Reset() { m_flag.store(false, std::memory_order_relaxed); }

private:
  std::atomic_bool m_flag{};
};

}  // namespace Common
