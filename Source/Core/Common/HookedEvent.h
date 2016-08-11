// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#pragma once

#include <functional>
#include <vector>

#include "Common/MsgHandler.h"

template <class... Args>
class HookableEvent
{
public:
  void RegisterCallback(std::string name, std::function<void(Args...)> callback)
  {
    auto iter = std::find_if(std::begin(callbacks), std::end(callbacks),
                             [&](auto& x) { return x.first == name; });
    if (iter != std::end(callbacks))
    {
      PanicAlert("%s callback already exists", name.c_str());
      return;
    }
    callbacks.push_back(std::make_pair(name, callback));
  }

  // Deletes a callback, if it exits
  void DeleteCallback(std::string name)
  {
    auto iter = std::find_if(std::begin(callbacks), std::end(callbacks),
                             [&](auto& x) { return x.first == name; });
    if (iter != std::end(callbacks))
      callbacks.erase(iter);
  }

  void EmitEvent(Args... args)
  {
    for (const auto& callback : callbacks)
      callback.second(args...);
  }

private:
  std::vector<std::pair<std::string, std::function<void(Args...)>>> callbacks;
};
