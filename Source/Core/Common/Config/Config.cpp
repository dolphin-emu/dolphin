// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Config/Config.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

namespace Config
{
using Layers = std::map<LayerType, std::shared_ptr<Layer>>;

static Layers s_layers;
static std::vector<std::pair<ConfigChangedCallbackID, ConfigChangedCallback>> s_callbacks;
static size_t s_next_callback_id = 0;
static u32 s_callback_guards = 0;
static std::atomic<u64> s_config_version = 0;

static std::shared_mutex s_layers_rw_lock;

using ReadLock = std::shared_lock<std::shared_mutex>;
using WriteLock = std::unique_lock<std::shared_mutex>;

static void AddLayerInternal(std::shared_ptr<Layer> layer)
{
  {
    WriteLock lock(s_layers_rw_lock);

    const Config::LayerType layer_type = layer->GetLayer();
    s_layers.insert_or_assign(layer_type, std::move(layer));
  }
  OnConfigChanged();
}

void AddLayer(std::unique_ptr<ConfigLayerLoader> loader)
{
  AddLayerInternal(std::make_shared<Layer>(std::move(loader)));
}

std::shared_ptr<Layer> GetLayer(LayerType layer)
{
  ReadLock lock(s_layers_rw_lock);

  std::shared_ptr<Layer> result;
  const auto it = s_layers.find(layer);
  if (it != s_layers.end())
  {
    result = it->second;
  }
  return result;
}

void RemoveLayer(LayerType layer)
{
  {
    WriteLock lock(s_layers_rw_lock);

    s_layers.erase(layer);
  }
  OnConfigChanged();
}

ConfigChangedCallbackID AddConfigChangedCallback(ConfigChangedCallback func)
{
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

void OnConfigChanged()
{
  // Increment the config version to invalidate caches.
  // To ensure that getters do not return stale data, this should always be done
  // even when callbacks are suppressed.
  s_config_version.fetch_add(1, std::memory_order_relaxed);

  if (s_callback_guards)
    return;

  for (const auto& callback : s_callbacks)
    callback.second();
}

u64 GetConfigVersion()
{
  return s_config_version.load(std::memory_order_relaxed);
}

// Explicit load and save of layers
void Load()
{
  {
    ReadLock lock(s_layers_rw_lock);

    for (auto& layer : s_layers)
      layer.second->Load();
  }
  OnConfigChanged();
}

void Save()
{
  {
    ReadLock lock(s_layers_rw_lock);

    for (auto& layer : s_layers)
      layer.second->Save();
  }
  OnConfigChanged();
}

void Init()
{
  // These layers contain temporary values
  ClearCurrentRunLayer();
}

void Shutdown()
{
  WriteLock lock(s_layers_rw_lock);

  s_layers.clear();
}

void ClearCurrentRunLayer()
{
  WriteLock lock(s_layers_rw_lock);

  s_layers.insert_or_assign(LayerType::CurrentRun, std::make_shared<Layer>(LayerType::CurrentRun));
}

static const std::map<System, std::string> system_to_name = {
    {System::Main, "Dolphin"},
    {System::GCPad, "GCPad"},
    {System::WiiPad, "Wiimote"},
    {System::GCKeyboard, "GCKeyboard"},
    {System::GFX, "Graphics"},
    {System::Logger, "Logger"},
    {System::SYSCONF, "SYSCONF"},
    {System::DualShockUDPClient, "DualShockUDPClient"},
    {System::FreeLook, "FreeLook"},
    {System::Session, "Session"},
    {System::GameSettingsOnly, "GameSettingsOnly"},
    {System::Achievements, "Achievements"}};

const std::string& GetSystemName(System system)
{
  return system_to_name.at(system);
}

std::optional<System> GetSystemFromName(const std::string& name)
{
  const auto system = std::ranges::find_if(
      system_to_name, [&name](const auto& entry) { return entry.second == name; });
  if (system != system_to_name.end())
    return system->first;

  return {};
}

const std::string& GetLayerName(LayerType layer)
{
  static const std::map<LayerType, std::string> layer_to_name = {
      {LayerType::Base, "Base"},
      {LayerType::GlobalGame, "Global GameINI"},
      {LayerType::LocalGame, "Local GameINI"},
      {LayerType::Netplay, "Netplay"},
      {LayerType::Movie, "Movie"},
      {LayerType::CommandLine, "Command Line"},
      {LayerType::CurrentRun, "Current Run"},
  };
  return layer_to_name.at(layer);
}

LayerType GetActiveLayerForConfig(const Location& config)
{
  ReadLock lock(s_layers_rw_lock);

  for (auto layer : SEARCH_ORDER)
  {
    const auto it = s_layers.find(layer);
    if (it != s_layers.end())
    {
      if (it->second->Exists(config))
        return layer;
    }
  }

  // If config is not present in any layer, base layer is considered active.
  return LayerType::Base;
}

std::optional<std::string> GetAsString(const Location& config)
{
  std::optional<std::string> result;
  ReadLock lock(s_layers_rw_lock);

  for (auto layer : SEARCH_ORDER)
  {
    const auto it = s_layers.find(layer);
    if (it != s_layers.end())
    {
      result = it->second->Get<std::string>(config);
      if (result.has_value())
        break;
    }
  }

  return result;
}

ConfigChangeCallbackGuard::ConfigChangeCallbackGuard()
{
  ++s_callback_guards;
}

ConfigChangeCallbackGuard::~ConfigChangeCallbackGuard()
{
  if (--s_callback_guards)
    return;

  OnConfigChanged();
}

}  // namespace Config
