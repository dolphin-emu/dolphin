// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "Common/Logging/Log.h"

namespace Common
{
struct HookBase
{
  // A pure virtual destructor makes this class abstract to prevent accidental "slicing".
  virtual ~HookBase() = 0;
};
inline HookBase::~HookBase() = default;

// EventHook is a handle a registered listener holds.
// When the handle is destroyed, the HookableEvent will automatically remove the listener.
// If the handle outlives the HookableEvent, the link will be properly disconnected.
using EventHook = std::unique_ptr<HookBase>;

// A hookable event system.
//
// Define Events as:
//
//   HookableEvent<std::string, u32> my_lovely_event;
//
// Register listeners anywhere you need them as:
//
//   EventHook my_hook = my_lovely_event.Register([](std::string foo, u32 bar) {
//       fmt::print("I've been triggered with {} and {}", foo, bar)
//   });
//
// The hook will be automatically unregistered when the EventHook object goes out of scope.
// Trigger events by calling Trigger as:
//
//   my_lovely_event.Trigger("Hello world", 42);
//

template <typename... CallbackArgs>
class HookableEvent
{
public:
  using CallbackType = std::function<void(CallbackArgs...)>;

  // Returns a handle that will unregister the listener when destroyed.
  // Note: Attempting to add/remove hooks of the event within the callback itself will NOT work.
  [[nodiscard]] EventHook Register(CallbackType callback)
  {
    DEBUG_LOG_FMT(COMMON, "Registering event hook handler");
    auto handle = std::make_unique<HookImpl>(m_storage, std::move(callback));

    std::lock_guard lg(m_storage->listeners_mutex);
    m_storage->listeners.push_back(handle.get());
    return handle;
  }

  void Trigger(const CallbackArgs&... args)
  {
    std::lock_guard lg(m_storage->listeners_mutex);
    for (auto* const handle : m_storage->listeners)
      std::invoke(handle->callback, args...);
  }

private:
  struct HookImpl;

  struct Storage
  {
    std::mutex listeners_mutex;
    std::vector<HookImpl*> listeners;
  };

  struct HookImpl final : HookBase
  {
    HookImpl(const std::shared_ptr<Storage> storage, CallbackType func)
        : weak_storage{storage}, callback{std::move(func)}
    {
    }

    ~HookImpl() override
    {
      const auto storage = weak_storage.lock();
      if (storage == nullptr)
      {
        DEBUG_LOG_FMT(COMMON, "Handler outlived event hook");
        return;
      }

      DEBUG_LOG_FMT(COMMON, "Removing event hook handler");

      std::lock_guard lg(storage->listeners_mutex);
      std::erase(storage->listeners, this);
    }

    std::weak_ptr<Storage> weak_storage;
    const CallbackType callback;
  };

  // shared_ptr storage allows hooks to forget their connection if they outlive the event itself.
  std::shared_ptr<Storage> m_storage{std::make_shared<Storage>()};
};

}  // namespace Common
