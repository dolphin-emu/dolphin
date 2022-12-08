// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <mutex>

#include "Common/CommonTypes.h"

namespace API
{
namespace Events
{
// events are defined as structs.
// each event also has to be added to the EventHub type alias.
struct FrameAdvance
{
};
struct MemoryBreakpoint
{
  bool write;
  u32 addr;
  u64 value;
};
struct CodeBreakpoint
{
  u32 addr;
};
struct SetInterrupt
{
  u32 cause_mask;
};
struct ClearInterrupt
{
  u32 cause_mask;
};

}  // namespace API::Events

// a listener on T is any function that takes a const reference to T as argument
template <typename T>
using Listener = std::function<void(const T&)>;
template <typename T>
using ListenerFuncPtr = void (*)(const T&);

// inside a generic struct to make listener IDs typesafe per event
template <typename T>
struct ListenerID
{
  bool operator==(const ListenerID& other) { return other.value == value; }
  u64 value;
};

// an event container manages a single event type
template <typename T>
class EventContainer final
{
public:
  void EmitEvent(T evt)
  {
    std::lock_guard lock{m_listeners_iterate_mutex};
    // avoid concurrent modification issues by iterating over a copy
    std::vector<std::pair<ListenerID<T>, Listener<T>>> listener_pairs = m_listener_pairs;
    for (auto& listener_pair : listener_pairs)
      listener_pair.second(evt);
    // avoid concurrent modification issues by performing a swap
    // with an fresh empty vector.
    std::vector<Listener<T>> one_time_listeners;
    std::swap(one_time_listeners, m_one_time_listeners);
    for (auto& listener : one_time_listeners)
      listener(evt);
  }

  ListenerID<T> ListenEvent(Listener<T> listener)
  {
    auto id = ListenerID<T>{m_next_listener_id++};
    m_listener_pairs.emplace_back(std::pair<ListenerID<T>, Listener<T>>(id, std::move(listener)));
    return id;
  }

  bool UnlistenEvent(ListenerID<T> listener_id)
  {
    for (auto it = m_listener_pairs.begin(); it != m_listener_pairs.end(); ++it)
    {
      if (it->first == listener_id)
      {
        m_listener_pairs.erase(it);
        return true;
      }
    }
    return false;
  }

  void ListenEventOnce(Listener<T> listener)
  {
    m_one_time_listeners.emplace_back(std::move(listener));
  }

  void TickListeners()
  {
    std::lock_guard lock{m_listeners_iterate_mutex};
  }
private:
  std::mutex m_listeners_iterate_mutex{};
  std::vector<std::pair<ListenerID<T>, Listener<T>>> m_listener_pairs{};
  std::vector<Listener<T>> m_one_time_listeners{};
  u64 m_next_listener_id = 0;
};

// an event hub manages a set of event containers,
// hence being the gateway to a multitude of events
template <typename... Ts>
class GenericEventHub final
{
public:
  template <typename T>
  void EmitEvent(T evt)
  {
    GetEventContainer<T>().EmitEvent(evt);
  }

  template <typename T>
  ListenerID<T> ListenEvent(Listener<T> listener)
  {
    return GetEventContainer<T>().ListenEvent(listener);
  }

  // convenience overload
  template <typename T>
  ListenerID<T> ListenEvent(ListenerFuncPtr<T> listener_func_ptr)
  {
    return GetEventContainer<T>().ListenEvent(listener_func_ptr);
  }

  template <typename T>
  bool UnlistenEvent(ListenerID<T> listener_id)
  {
    return GetEventContainer<T>().UnlistenEvent(listener_id);
  }

  template <typename T>
  void ListenEventOnce(Listener<T> listener)
  {
    GetEventContainer<T>().ListenEventOnce(listener);
  }

  void TickAllListeners()
  {
    std::apply([](auto&&... arg) { (arg.TickListeners(), ...); }, m_event_containers);
  }

private:
  template <typename T>
  EventContainer<T>& GetEventContainer()
  {
    return std::get<EventContainer<T>>(m_event_containers);
  }

  std::tuple<EventContainer<Ts>...> m_event_containers;
};

// all existing events need to be listed here, otherwise there will be spooky templating errors
using EventHub = GenericEventHub<Events::FrameAdvance, Events::SetInterrupt, Events::ClearInterrupt,
                                 Events::MemoryBreakpoint, Events::CodeBreakpoint>;

// global event hub
EventHub& GetEventHub();

}  // namespace API
