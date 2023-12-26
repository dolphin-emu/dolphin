// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/CPUThreadConfigCallback.h"

#include <atomic>

#include "Common/Assert.h"
#include "Common/Config/Config.h"
#include "Core/Core.h"

namespace
{
std::atomic<bool> s_should_run_callbacks = false;

static std::vector<
    std::pair<CPUThreadConfigCallback::ConfigChangedCallbackID, Config::ConfigChangedCallback>>
    s_callbacks;

static size_t s_next_callback_id = 0;

void RunCallbacks()
{
  for (const auto& callback : s_callbacks)
    callback.second();
}

void OnConfigChanged()
{
  if (Core::IsCPUThread())
  {
    s_should_run_callbacks.store(false, std::memory_order_relaxed);
    RunCallbacks();
  }
  else
  {
    s_should_run_callbacks.store(true, std::memory_order_relaxed);
  }
}

};  // namespace

namespace CPUThreadConfigCallback
{
ConfigChangedCallbackID AddConfigChangedCallback(Config::ConfigChangedCallback func)
{
  [[maybe_unused]] static auto s_config_changed_callback_id =
      Config::AddConfigChangedCallback(&OnConfigChanged);

  const ConfigChangedCallbackID callback_id{s_next_callback_id};
  ++s_next_callback_id;
  s_callbacks.emplace_back(std::make_pair(callback_id, std::move(func)));
  return callback_id;
}

void RemoveConfigChangedCallback(ConfigChangedCallbackID callback_id)
{
  for (auto it = s_callbacks.begin(); it != s_callbacks.end(); ++it)
  {
    if (it->first == callback_id)
    {
      s_callbacks.erase(it);
      return;
    }
  }
}

void CheckForConfigChanges()
{
  DEBUG_ASSERT(Core::IsCPUThread());

  if (s_should_run_callbacks.exchange(false, std::memory_order_relaxed))
    RunCallbacks();
}

};  // namespace CPUThreadConfigCallback
