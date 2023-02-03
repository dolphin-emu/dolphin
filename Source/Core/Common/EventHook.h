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
// A hookable event system.

// Define Events in a header as:
//
//     using MyLoveyEvent = Event<"My lovely event", std::string>;
//
// Register listeners anywhere you need them as:
//    EventHook myHook = MyLoveyEvent::Register([](std::string foo) {
//       // Do something
//   }, "Name of the hook");
//
// The hook will be automatically unregistered when the EventHook object goes out of scope.
// Trigger events by doing:
//
//   MyLoveyEvent::Trigger("Hello world");
//

struct HookBase
{
  virtual ~HookBase() = default;
};

using EventHook = std::unique_ptr<HookBase>;

template <StringLiteral EventName, typename... CallbackArgs>
class Event
{
public:
  using CallbackType = std::function<void(CallbackArgs...)>;

private:
  struct HookImpl final : public HookBase
  {
    ~HookImpl() override { Event::Remove(this); }
    HookImpl(CallbackType callback, std::string name)
        : m_fn(std::move(callback)), m_name(std::move(name))
    {
    }
    CallbackType m_fn;
    std::string m_name;
  };

public:
  // Returns a handle that will unregister the listener when destroyed.
  static EventHook Register(CallbackType callback, std::string name)
  {
    std::lock_guard lock(m_mutex);
    DEBUG_LOG_FMT(COMMON, "Registering {} handler at {} event hook", name, EventName.value);
    auto handle = std::make_unique<HookImpl>(callback, std::move(name));
    m_listeners.push_back(handle.get());
    return handle;
  }

  static void Trigger(const CallbackArgs&... args)
  {
    std::lock_guard lock(m_mutex);
    for (auto& handle : m_listeners)
      handle->m_fn(args...);
  }

private:
  static void Remove(HookImpl* handle)
  {
    std::lock_guard lock(m_mutex);
    std::erase(m_listeners, handle);
  }

  inline static std::vector<HookImpl*> m_listeners = {};
  inline static std::mutex m_mutex;
};

}  // namespace Common
