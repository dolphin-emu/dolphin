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
  typedef void(*CallbackType)(Args...);
  static constexpr int num_args = sizeof...(Args);

  HookableEvent(std::array<std::string, num_args> &arg_names) : argument_names(arg_names)
  {

  }

  void RegisterCallback(const std::string name, std::function<void(Args...)> callback)
  {
    auto iter = std::find_if(std::begin(callbacks), std::end(callbacks),
                             [&](const auto& x) { return x.first == name; });
    if (iter != std::end(callbacks))
    {
      PanicAlert("%s callback already exists", name.c_str());
      return;
    }
    callbacks.emplace_back(std::move(name), std::move(callback));
  }

  // Deletes a callback, if it exits
  void DeleteCallback(std::string name)
  {
    auto iter = std::find_if(std::begin(callbacks), std::end(callbacks),
                             [&](const auto& x) { return x.first == name; });
    if (iter != std::end(callbacks))
      callbacks.erase(iter);
  }

  void EmitEvent(Args... args)
  {
    for (const auto& callback : callbacks)
      callback.second(args...);
  }

  static_assert(all_const_exportable<Args...>::value, "All Hook argument types must either be fundamental, or pointers to standard layout structs/arrays");

private:
  std::vector<std::pair<std::string, std::function<void(Args...)>>> callbacks;
  std::array<std::string, num_args> &argument_names;
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
  static const bool value = std::is_fundamental<T>::value || (std::is_standard_layout<T>::value && std::is_const<T>::value && !std::is_pointer<T>::value);
};

template <typename Head, typename... Tail>
struct all_const_exportable<Head, Tail...>
{
  static const bool value = all_const_exportable<Head>::value && all_const_exportable<Tail...>::value;
};

