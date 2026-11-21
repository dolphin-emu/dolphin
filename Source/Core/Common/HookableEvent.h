// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <forward_list>
#include <list>
#include <memory>
#include <mutex>
#include <utility>

#include "Common/Functional.h"
#include "Common/Logging/Log.h"

namespace Common
{

template <typename... CallbackArgs>
class HookableEvent;
class HookHolder;

class HookBase : public std::enable_shared_from_this<HookBase>
{
  friend HookHolder;

public:
  // Disables a connection.
  virtual void Disconnect() = 0;

  virtual bool IsConnected() const = 0;

  virtual ~HookBase() = default;

private:
  virtual void ForgetHolder() = 0;
};

// EventHook is a handle a registered listener holds.
// When the handle is destroyed, the HookableEvent will automatically remove the listener.
// If the handle outlives the HookableEvent, the link will be properly disconnected.
using EventHook = std::shared_ptr<HookBase>;
using WeakEventHook = std::weak_ptr<HookBase>;

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

// A container of EventHook objects.
// If any held EventHook become disconnected the internal container is appropriately adjusted.
//
// HookableEvent<> event;
// HookHolder holder;
//
// holder.Connect(event, [&] { fmt::print("triggered 1"); });
//
// WeakEventHook hook = holder.Connect(event, [&] { fmt::print("triggered 2"); });
// holder.Disconnect(hook);
//
class HookHolder
{
  template <typename... CallbackArgs>
  friend class HookableEvent;

public:
  // Registers an EventHook that is stored internally.
  // Returns a WeakEventHook that can disconnect or extend the life of the hook.
  template <typename... CallbackArgs>
  WeakEventHook Connect(HookableEvent<CallbackArgs...>& event,
                        HookableEvent<CallbackArgs...>::CallbackType callback)
  {
    return event.AddHook(std::move(callback), this);
  }

  // These are just convenience functions to invoke EventHook::Disconnect()
  void Disconnect(EventHook& hook) { hook->Disconnect(); }
  void Disconnect(WeakEventHook& hook)
  {
    if (auto h = hook.lock())
      Disconnect(h);
  }

  std::size_t GetConnectionCount() const
  {
    std::lock_guard lk{m_mutex};
    return m_hooks.size();
  }

  // Disconnect all held connections regardless of externally held EventHook.
  void DisconnectAll()
  {
    for (std::unique_lock lk{m_mutex}; !m_hooks.empty(); lk.lock())
    {
      const EventHook hook = std::move(m_hooks.back());
      // Avoid deadlock from different lock orders.
      lk.unlock();
      if (hook != nullptr)  // Concurrent disconnect may observe nullptr.
        hook->Disconnect();
    }
  }

  // Destruction only removes internal references.
  // Any externally held EventHook can keep a connection alive.
  ~HookHolder()
  {
    for (std::unique_lock lk{m_mutex}; !m_hooks.empty(); lk.lock())
    {
      const EventHook hook = std::move(m_hooks.back());
      // Avoid deadlock from different lock orders.
      lk.unlock();
      if (hook != nullptr)  // Concurrent disconnect may observe nullptr.
        hook->ForgetHolder();
    }
  }

private:
  mutable std::mutex m_mutex;

  using ConnectedHooks = std::list<EventHook>;
  ConnectedHooks m_hooks;

  ConnectedHooks::iterator AddHook(EventHook hook)
  {
    std::lock_guard lk{m_mutex};
    return m_hooks.emplace(m_hooks.end(), std::move(hook));
  }

  void RemoveHook(ConnectedHooks::iterator it)
  {
    std::lock_guard lk{m_mutex};
    m_hooks.erase(it);
  }
};

template <typename... CallbackArgs>
class HookableEvent
{
  friend HookHolder;

public:
  HookableEvent() = default;

  using CallbackType = Common::MoveOnlyFunction<void(CallbackArgs...)>;

  [[nodiscard]] EventHook Register(HookableEvent::CallbackType callback)
  {
    return AddHook(std::move(callback));
  }

  // Currently *not* allowed to cause destruction of the HookableEvent itself.
  void Trigger(CallbackArgs... args)
  {
    std::lock_guard lk{*m_mutex};

    // Destruction from Trigger might be feasible if all state is moved into a `shared_ptr`.
    TriggerState& trigger_state = m_trigger_states.emplace_front();
    trigger_state.next = m_hooks.begin();
    trigger_state.end = m_hooks.end();

    while (trigger_state.next != trigger_state.end)
    {
      auto& callback = (trigger_state.next++)->callback;
      std::invoke(callback, args...);
    }

    m_trigger_states.pop_front();
  }

  std::size_t GetConnectionCount() const
  {
    std::lock_guard lk{*m_mutex};
    return m_hooks.size();
  }

  // Currently *not* allowed to cause destruction of the HookableEvent itself.
  void DisconnectAll()
  {
    std::lock_guard lk{*m_mutex};

    while (!m_hooks.empty())
      m_hooks.back().hook->Disconnect();
  }

  HookableEvent(HookableEvent&&) = delete;
  HookableEvent(const HookableEvent&) = delete;
  HookableEvent& operator=(HookableEvent&&) = delete;
  HookableEvent& operator=(const HookableEvent&) = delete;

  ~HookableEvent() { DisconnectAll(); }

private:
  class Hook;

  struct ConnectedHook
  {
    CallbackType callback;
    Hook* const hook;
  };

  using ConnectedHooks = std::list<ConnectedHook>;
  ConnectedHooks m_hooks;

  // Member variable iterators so Add/Remove inside a Trigger call can adjust them.
  struct TriggerState
  {
    ConnectedHooks::iterator next;
    ConnectedHooks::iterator end;
  };

  // Multiple trigger states to handle recursive Trigger calls.
  std::forward_list<TriggerState> m_trigger_states;

  // The mutex is shared with all connected hooks.
  std::shared_ptr<std::recursive_mutex> m_mutex{std::make_shared<std::recursive_mutex>()};

  class Hook final : public HookBase
  {
    friend HookableEvent;

  public:
    void Disconnect() override
    {
      // Removal from either the holder or the event may take us out of scope.
      // Delay destruction until this function is over.
      const auto self = shared_from_this();

      std::lock_guard lk{*m_mutex};

      if (auto* holder = std::exchange(m_holder, nullptr))
        holder->RemoveHook(m_holder_it);

      if (auto* event = std::exchange(m_event, nullptr))
        event->RemoveHook(m_event_it);
    }

    bool IsConnected() const override
    {
      std::lock_guard lk{*m_mutex};
      return m_event != nullptr;
    }

    ~Hook() override
    {
      std::lock_guard lk{*m_mutex};

      // Reaching the destructor is only possible if a potential Holder has already forgotten us.
      // We only need to inform the Event.
      if (auto* event = std::exchange(m_event, nullptr))
        event->RemoveHook(m_event_it);
    }

  private:
    void ForgetHolder() override
    {
      std::lock_guard lk{*m_mutex};

      if (auto* holder = std::exchange(m_holder, nullptr))
        holder->RemoveHook(m_holder_it);
    }

    HookableEvent* m_event{};
    ConnectedHooks::iterator m_event_it;

    HookHolder* m_holder{};
    HookHolder::ConnectedHooks::iterator m_holder_it;

    // Shared with the event and all other connected hooks.
    std::shared_ptr<std::recursive_mutex> m_mutex;
  };

  EventHook AddHook(CallbackType callback, HookHolder* holder = nullptr)
  {
    DEBUG_LOG_FMT(COMMON, "Registering event hook handler");

    std::lock_guard lk{*m_mutex};

    auto hook = std::make_shared<Hook>();
    auto it = m_hooks.emplace(m_hooks.end(), std::move(callback), hook.get());

    hook->m_event = this;
    hook->m_event_it = it;

    hook->m_mutex = m_mutex;

    if (holder)
    {
      hook->m_holder = holder;
      hook->m_holder_it = holder->AddHook(hook);
    }

    // Trigger states are adjusted to not include the new item.
    for (auto& trigger_state : m_trigger_states)
    {
      if (trigger_state.end == m_hooks.end())
      {
        if (trigger_state.next == trigger_state.end)
          trigger_state.next = it;
        trigger_state.end = it;
      }
    }

    return hook;
  }

  void RemoveHook(ConnectedHooks::iterator it)
  {
    DEBUG_LOG_FMT(COMMON, "Removing event hook handler");

    // Note: The mutex is already held from the only caller.

    // Trigger states referencing the removed item are adjusted to the very next item.
    const auto next = std::next(it);
    for (auto& trigger_state : m_trigger_states)
    {
      if (trigger_state.end == it)
        trigger_state.end = next;
      if (trigger_state.next == it)
        trigger_state.next = next;
    }
    m_hooks.erase(it);
  }
};

}  // namespace Common
