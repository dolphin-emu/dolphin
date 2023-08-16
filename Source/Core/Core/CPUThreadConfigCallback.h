// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Config/Config.h"

// This file lets you register callbacks like in Common/Config/Config.h, with the difference that
// callbacks registered here are guaranteed to run on the CPU thread. Callbacks registered here may
// run with a slight delay compared to regular config callbacks.

namespace CPUThreadConfigCallback
{
// returns an ID that can be passed to RemoveConfigChangedCallback()
size_t AddConfigChangedCallback(Config::ConfigChangedCallback func);

void RemoveConfigChangedCallback(size_t callback_id);

// Should be called regularly from the CPU thread
void CheckForConfigChanges();

};  // namespace CPUThreadConfigCallback
