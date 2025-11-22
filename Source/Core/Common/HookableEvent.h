// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
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
  [[nodiscard]] EventHook Register(CallbackType callback)
  {
    DEBUG_LOG_FMT(COMMON, "Registering event hook handler");
    std::lock_guard lg(m_storage->listeners_mutex);

    auto& new_listener =
        m_storage->listeners.emplace_back(std::make_unique<Listener>(std::move(callback)));

    return std::make_unique<HookImpl>(m_storage, new_listener.get());
  }

  // Invokes all registered callbacks.
  // Hooks added from within a callback will be invoked.
  // Hooks removed from within a callback will be skipped,
  //  but destruction of the hook's callback will be delayed until Trigger() completes.
  void Trigger(const CallbackArgs&... args)
  {
    std::lock_guard lg(m_storage->listeners_mutex);
    m_storage->is_triggering = true;

    // Avoiding an actual iterator because the container may be modified.
    for (std::size_t i = 0; i != m_storage->listeners.size(); ++i)
    {
      auto& listener = m_storage->listeners[i];

      if (listener->is_pending_removal)
        continue;

      std::invoke(listener->callback, args...);
    }

    m_storage->is_triggering = false;
    std::erase_if(m_storage->listeners, std::mem_fn(&Listener::is_pending_removal));
  }

private:
  struct Listener
  {
    const CallbackType callback;
    bool is_pending_removal{};
  };

  struct Storage
  {
    std::recursive_mutex listeners_mutex;
    std::vector<std::unique_ptr<Listener>> listeners;
    bool is_triggering{};
  };

  struct HookImpl final : HookBase
  {
    HookImpl(std::weak_ptr<Storage> storage, Listener* listener)
        : weak_storage{std::move(storage)}, listener_ptr{listener}
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

      if (storage->is_triggering)
      {
        // Just mark our listener for removal.
        // Trigger() will erase it for us.
        listener_ptr->is_pending_removal = true;
      }
      else
      {
        // Remove our listener.
        storage->listeners.erase(std::ranges::find_if(
            storage->listeners, [&](auto& ptr) { return ptr.get() == listener_ptr; }));
      }
    }

    const std::weak_ptr<Storage> weak_storage;
    Listener* const listener_ptr;  // "owned" by the above Storage.
  };

  // shared_ptr storage allows hooks to forget their connection if they outlive the event itself.
  std::shared_ptr<Storage> m_storage{std::make_shared<Storage>()};
};

}  // namespace Common
