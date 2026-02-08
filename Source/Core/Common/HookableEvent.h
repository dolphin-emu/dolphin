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

protected:
  HookBase() = default;

  // This shouldn't be copied. And since we always wrap it in unique_ptr, no need to move it either
  HookBase(const HookBase&) = delete;
  HookBase(HookBase&&) = delete;
  HookBase& operator=(const HookBase&) = delete;
  HookBase& operator=(HookBase&&) = delete;
};

// EventHook is a handle a registered listener holds.
// When the handle is destroyed, the HookableEvent will automatically remove the listener.
using EventHook = std::unique_ptr<HookBase>;

// A hookable event system.
//
// Define Events in a header as:
//
//     using MyLoveyEvent = HookableEvent<"My lovely event", std::string, u32>;
//
// Register listeners anywhere you need them as:
//    EventHook myHook = MyLoveyEvent::Register([](std::string foo, u32 bar) {
//       fmt::print("I've been triggered with {} and {}", foo, bar)
//   }, "NameOfHook");
//
// The hook will be automatically unregistered when the EventHook object goes out of scope.
// Trigger events by calling Trigger as:
//
//   MyLoveyEvent::Trigger("Hello world", 42);
//
template <StringLiteral EventName, typename... CallbackArgs>
class HookableEvent
{
public:
  using CallbackType = std::function<void(CallbackArgs...)>;

private:
  struct HookImpl final : public HookBase
  {
    ~HookImpl() override { HookableEvent::Remove(this); }
    HookImpl(CallbackType callback, std::string name)
        : m_fn(std::move(callback)), m_name(std::move(name))
    {
    }
    CallbackType m_fn;
    std::string m_name;
  };

  struct Storage
  {
    std::recursive_mutex m_mutex;
    std::vector<HookImpl*> m_listeners;
  };

  // We use the "Construct On First Use" idiom to avoid the static initialization order fiasco.
  // https://isocpp.org/wiki/faq/ctors#static-init-order
  static Storage& GetStorage()
  {
    static Storage storage;
    return storage;
  }

  static void Remove(HookImpl* handle)
  {
    auto& storage = GetStorage();
    std::lock_guard lock(storage.m_mutex);

    std::erase(storage.m_listeners, handle);
  }

public:
  // Returns a handle that will unregister the listener when destroyed.
  [[nodiscard]] static EventHook Register(CallbackType callback, std::string name)
  {
    auto& storage = GetStorage();
    std::lock_guard lock(storage.m_mutex);

    DEBUG_LOG_FMT(COMMON, "Registering {} handler at {} event hook", name, EventName.value);
    auto handle = std::make_unique<HookImpl>(std::move(callback), std::move(name));
    storage.m_listeners.push_back(handle.get());
    return handle;
  }

  static void Trigger(const CallbackArgs&... args)
  {
    auto& storage = GetStorage();
    std::lock_guard lock(storage.m_mutex);

    for (const auto& handle : storage.m_listeners)
      handle->m_fn(args...);
  }
};

}  // namespace Common
