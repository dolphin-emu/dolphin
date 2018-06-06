// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>

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
  u32 value;
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

// an event container manages a single event
template <typename T>
class EventContainer final
{
public:
  void EmitEvent(T evt)
  {
    for (auto& listener_pair : m_listener_pairs)
      listener_pair.second(evt);
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

private:
  std::vector<std::pair<ListenerID<T>, Listener<T>>> m_listener_pairs{};
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
                                 Events::MemoryBreakpoint>;

// global event hub
EventHub& GetEventHub();

}  // namespace API
