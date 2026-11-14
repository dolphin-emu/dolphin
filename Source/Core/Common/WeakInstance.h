// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <shared_mutex>

namespace Common
{

// Holds a weak pointer. Allows atomically getting or creating the instance.
template <typename T>
class WeakInstance final
{
public:
  std::weak_ptr<T> Get()
  {
    std::shared_lock lk{m_mutex};
    return m_ptr;
  }

  std::shared_ptr<T> GetOrCreate()
  {
    std::lock_guard lk{m_mutex};
    auto ptr = m_ptr.lock();
    if (!ptr)
      m_ptr = ptr = std::make_shared<T>();
    return ptr;
  }

private:
  std::shared_mutex m_mutex;
  std::weak_ptr<T> m_ptr;
};

}  // namespace Common
