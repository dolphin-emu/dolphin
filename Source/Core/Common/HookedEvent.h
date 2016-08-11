// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#pragma once

#include <functional>
#include <type_traits>
#include <vector>

#include "Common/MsgHandler.h"

template <typename... Ts>
struct all_const_exportable;

template <class... Args>
class HookableEvent
{
public:
  using CallbackType = void (*)(Args...);
  static constexpr size_t num_args = sizeof...(Args);

  explicit HookableEvent(std::array<std::string, num_args>& arg_names) : m_argument_names(arg_names)
  {
  }

  void RegisterCallback(std::string name, std::function<void(Args...)> callback)
  {
    std::lock_guard<std::mutex> lock(m_callbacks_mutex);

    auto iter = std::find_if(std::begin(m_callbacks), std::end(m_callbacks),
                             [&](const auto& x) { return x.first == name; });
    if (iter != std::end(m_callbacks))
    {
      PanicAlert("%s callback already exists", name.c_str());
      return;
    }
    m_callbacks.emplace_back(std::move(name), std::move(callback));
  }

  // Deletes a callback, if it exits
  void DeleteCallback(std::string name)
  {
    std::lock_guard<std::mutex> lock(m_callbacks_mutex);

    auto iter = std::find_if(std::begin(m_callbacks), std::end(m_callbacks),
                             [&](const auto& x) { return x.first == name; });
    if (iter != std::end(m_callbacks))
      m_callbacks.erase(iter);
  }

  void EmitEvent(Args... args)
  {
    std::lock_guard<std::mutex> lock(m_callbacks_mutex);

    // We use a Vector for m_callbacks because we can iterate over Vector really fast.
    // Note: if this function turns out to be a bottleneck, here are a few optimisation ideas:
    //   * Use two vectors, one with the names, one with the callbacks.
    //     Keep them in sync during add and delete, and only iterate over callbacks in here
    //     to minimize cache usage.
    //   * Create a custom container which allows fast iteration over callbacks without the lock.
    //     Basically, make sure AddCallback and deleteCallback are atomic. Add can append to the
    //     vector then increment the counter. Delete might need to do a series of atomic swaps.

    for (const auto& callback : m_callbacks)
      callback.second(args...);
  }

  static_assert(all_const_exportable<Args...>::value, "All Hook argument types must either be "
                                                      "fundamental, or pointers to standard layout "
                                                      "structs/arrays");

private:
  std::mutex m_callbacks_mutex;
  std::vector<std::pair<std::string, std::function<void(Args...)>>> m_callbacks;
  std::array<std::string, num_args>& m_argument_names;
};

template <>
struct all_const_exportable<>
{
  static const bool value = true;
};

template <typename T>
struct all_const_exportable<T>
{
  // Type is either fundamental, or a const pointer to a standard layout structure
  static const bool value =
      std::is_fundamental<T>::value ||
      (std::is_standard_layout<T>::value && std::is_const<T>::value && std::is_pointer<T>::value);
};

template <typename Head, typename... Tail>
struct all_const_exportable<Head, Tail...>
{
  static const bool value =
      all_const_exportable<Head>::value && all_const_exportable<Tail...>::value;
};
