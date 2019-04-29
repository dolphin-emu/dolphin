// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <list>
#include <map>

#include "Common/Config/Config.h"

namespace Config
{
static Layers s_layers;
static std::list<ConfigChangedCallback> s_callbacks;
static u32 s_callback_guards = 0;

Layers* GetLayers()
{
  return &s_layers;
}

void AddLayer(std::unique_ptr<Layer> layer)
{
  s_layers[layer->GetLayer()] = std::move(layer);
  InvokeConfigChangedCallbacks();
}

void AddLayer(std::unique_ptr<ConfigLayerLoader> loader)
{
  AddLayer(std::make_unique<Layer>(std::move(loader)));
}

Layer* GetLayer(LayerType layer)
{
  if (!LayerExists(layer))
    return nullptr;
  return s_layers[layer].get();
}

void RemoveLayer(LayerType layer)
{
  s_layers.erase(layer);
  InvokeConfigChangedCallbacks();
}
bool LayerExists(LayerType layer)
{
  return s_layers.find(layer) != s_layers.end();
}

void AddConfigChangedCallback(ConfigChangedCallback func)
{
  s_callbacks.emplace_back(func);
}

void InvokeConfigChangedCallbacks()
{
  if (s_callback_guards)
    return;

  for (const auto& callback : s_callbacks)
    callback();
}

// Explicit load and save of layers
void Load()
{
  for (auto& layer : s_layers)
    layer.second->Load();
  InvokeConfigChangedCallbacks();
}

void Save()
{
  for (auto& layer : s_layers)
    layer.second->Save();
  InvokeConfigChangedCallbacks();
}

void Init()
{
  // These layers contain temporary values
  ClearCurrentRunLayer();
}

void Shutdown()
{
  s_layers.clear();
  s_callbacks.clear();
}

void ClearCurrentRunLayer()
{
  s_layers[LayerType::CurrentRun] = std::make_unique<Layer>(LayerType::CurrentRun);
}

static const std::map<System, std::string> system_to_name = {
    {System::Main, "Dolphin"},          {System::GCPad, "GCPad"},    {System::WiiPad, "Wiimote"},
    {System::GCKeyboard, "GCKeyboard"}, {System::GFX, "Graphics"},   {System::Logger, "Logger"},
    {System::Debugger, "Debugger"},     {System::SYSCONF, "SYSCONF"}};

const std::string& GetSystemName(System system)
{
  return system_to_name.at(system);
}

std::optional<System> GetSystemFromName(const std::string& name)
{
  const auto system = std::find_if(system_to_name.begin(), system_to_name.end(),
                                   [&name](const auto& entry) { return entry.second == name; });
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

LayerType GetActiveLayerForConfig(const ConfigLocation& config)
{
  for (auto layer : SEARCH_ORDER)
  {
    if (!LayerExists(layer))
      continue;

    if (GetLayer(layer)->Exists(config))
      return layer;
  }

  // If config is not present in any layer, base layer is considered active.
  return LayerType::Base;
}

ConfigChangeCallbackGuard::ConfigChangeCallbackGuard()
{
  ++s_callback_guards;
}

ConfigChangeCallbackGuard::~ConfigChangeCallbackGuard()
{
  if (--s_callback_guards)
    return;

  InvokeConfigChangedCallbacks();
}

}  // namespace Config
