// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>

// The Core only supports using a single Host thread.
// If multiple threads want to call host functions then they need to queue
// sequentially for access.
// TODO: The above isn't true anymore, so we should get rid of this class.
struct HostThreadLock
{
  explicit HostThreadLock() : m_lock(s_host_identity_mutex) {}

  ~HostThreadLock() = default;

  HostThreadLock(const HostThreadLock& other) = delete;
  HostThreadLock(HostThreadLock&& other) = delete;
  HostThreadLock& operator=(const HostThreadLock& other) = delete;
  HostThreadLock& operator=(HostThreadLock&& other) = delete;

  void Lock() { m_lock.lock(); }

  void Unlock() { m_lock.unlock(); }

private:
  static std::mutex s_host_identity_mutex;
  std::unique_lock<std::mutex> m_lock;
};
