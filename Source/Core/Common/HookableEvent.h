// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

#if defined(_DEBUG)
#include <string>

#include "Common/Logging/Log.h"
#endif

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
//     HookableEvent<std::string, u32> my_lovely_event{"My lovely event"};
//
// Register listeners anywhere you need them as:
//    EventHook my_hook = my_lovely_event.Register([](std::string foo, u32 bar) {
//       fmt::print("I've been triggered with {} and {}", foo, bar)
//   }, "NameOfHook");
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

  explicit HookableEvent(std::string_view event_name)
      : m_storage{std::make_shared<Storage>(event_name)}
  {
  }

  // Returns a handle that will unregister the listener when destroyed.
  // Note: Attempting to add/remove hooks of the event within the callback itself will NOT work.
  [[nodiscard]] EventHook Register(CallbackType callback, std::string_view name)
  {
#if defined(_DEBUG)
    DEBUG_LOG_FMT(COMMON, "Registering {} handler at {} event hook", name, m_storage->event_name);
#endif
    auto handle = std::make_unique<HookImpl>(m_storage, std::move(callback), name);

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
    explicit Storage(std::string_view name [[maybe_unused]])
    {
#if defined(_DEBUG)
      event_name = std::string(name);
#endif
    }

    std::mutex listeners_mutex;
    std::vector<HookImpl*> listeners;
#if defined(_DEBUG)
    std::string event_name;
#endif
  };

  struct HookImpl final : HookBase
  {
    HookImpl(const std::shared_ptr<Storage> storage, CallbackType func,
             std::string_view name [[maybe_unused]])
        : weak_storage{storage}, callback{std::move(func)}
    {
#if defined(_DEBUG)
      hook_name = std::string(name);
#endif
    }

    ~HookImpl() override
    {
      const auto storage = weak_storage.lock();
      if (storage == nullptr)
      {
#if defined(_DEBUG)
        DEBUG_LOG_FMT(COMMON, "Handler {} outlived event hook", hook_name);
#endif
        return;
      }

#if defined(_DEBUG)
      DEBUG_LOG_FMT(COMMON, "Removing handler {} of event hook {}", hook_name, storage->event_name);
#endif
      std::lock_guard lg(storage->listeners_mutex);
      std::erase(storage->listeners, this);
    }

    std::weak_ptr<Storage> weak_storage;
    const CallbackType callback;
#if defined(_DEBUG)
    std::string hook_name;
#endif
  };

  // shared_ptr storage allows hooks to forget their connection if they outlive the event itself.
  std::shared_ptr<Storage> m_storage;
};

}  // namespace Common
