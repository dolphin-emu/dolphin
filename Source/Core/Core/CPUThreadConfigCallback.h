// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <limits>

#include "Common/Config/Config.h"

// This file lets you register callbacks like in Common/Config/Config.h, with the difference that
// callbacks registered here are guaranteed to run on the CPU thread. Callbacks registered here may
// run with a slight delay compared to regular config callbacks.

namespace CPUThreadConfigCallback
{
struct ConfigChangedCallbackID
{
  size_t id = std::numeric_limits<size_t>::max();

  bool operator==(const ConfigChangedCallbackID&) const = default;
};

// Returns an ID that should be passed to RemoveConfigChangedCallback() when the callback is no
// longer needed.
[[nodiscard]] ConfigChangedCallbackID AddConfigChangedCallback(Config::ConfigChangedCallback func);

void RemoveConfigChangedCallback(ConfigChangedCallbackID callback_id);

// Should be called regularly from the CPU thread
void CheckForConfigChanges();

}  // namespace CPUThreadConfigCallback
