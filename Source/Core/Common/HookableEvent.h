// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Logging/Log.h"
#include "Common/StringLiteral.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Common
{
struct HookBase
{
  virtual ~HookBase() = default;

  // This shouldn't be copied. And since we always wrap it in unique_ptr, no need to move it either
  HookBase(const HookBase&) = delete;
  HookBase(HookBase&&) = delete;
  HookBase& operator=(const HookBase&) = delete;
  HookBase& operator=(HookBase&&) = delete;

protected:
  HookBase() = default;
};

// EventHook is a handle a registered listener holds.
// When the handle is destroyed, the HookableEvent will automatically remove the listener.
// If the handle outlives the HookableEvent, the link will be properly disconnected.
using EventHook = std::unique_ptr<HookBase>;

// A hookable event system.
//
// Define Events as:
//
//     HookableEvent<"My lovely event", std::string, u32> my_lovey_event;
//
// Register listeners anywhere you need them as:
//    EventHook myHook = my_lovey_event.Register([](std::string foo, u32 bar) {
//       fmt::print("I've been triggered with {} and {}", foo, bar)
//   }, "NameOfHook");
//
// The hook will be automatically unregistered when the EventHook object goes out of scope.
// Trigger events by calling Trigger as:
//
//   my_lovey_event.Trigger("Hello world", 42);
//

template <StringLiteral EventName, typename... CallbackArgs>
class HookableEvent
{
public:
  using CallbackType = std::function<void(CallbackArgs...)>;

private:
  struct Storage;

  struct HookImpl final : HookBase
  {
    HookImpl(std::weak_ptr<Storage> storage, CallbackType func, std::string name)
        : m_storage{std::move(storage)}, m_function{std::move(func)}, m_name{std::move(name)}
    {
    }

    ~HookImpl() override
    {
      const auto storage = m_storage.lock();
      if (storage == nullptr)
      {
        DEBUG_LOG_FMT(COMMON, "Handler {} outlived event hook {}", m_name, EventName.value);
        return;
      }

      DEBUG_LOG_FMT(COMMON, "Removing {} handler at {} event hook", m_name, EventName.value);
      storage->RemoveHook(this);
    }

    std::weak_ptr<Storage> m_storage;
    const CallbackType m_function;
    const std::string m_name;
  };

  struct Storage
  {
    void RemoveHook(HookImpl* handle)
    {
      std::lock_guard lock(m_mutex);
      std::erase(m_listeners, handle);
    }

    std::mutex m_mutex;
    std::vector<HookImpl*> m_listeners;
  };

public:
  // Returns a handle that will unregister the listener when destroyed.
  // Note: Attempting to add/remove hooks of the event within the callback itself will NOT end well.
  [[nodiscard]] EventHook Register(CallbackType callback, std::string name)
  {
    DEBUG_LOG_FMT(COMMON, "Registering {} handler at {} event hook", name, EventName.value);
    auto handle = std::make_unique<HookImpl>(m_storage, std::move(callback), std::move(name));

    std::lock_guard lock(m_storage->m_mutex);
    m_storage->m_listeners.push_back(handle.get());
    return handle;
  }

  void Trigger(const CallbackArgs&... args)
  {
    std::lock_guard lock(m_storage->m_mutex);
    for (auto* const handle : m_storage->m_listeners)
      handle->m_function(args...);
  }

private:
  // shared_ptr storage allows hooks to forget their connection if they outlive the event itself.
  std::shared_ptr<Storage> m_storage{std::make_shared<Storage>()};
};

}  // namespace Common
