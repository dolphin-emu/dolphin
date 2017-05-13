// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <list>
#include <map>

#include "Common/Assert.h"
#include "Common/Config/Config.h"

namespace Config
{
static Layers s_layers;
static std::list<ConfigChangedCallback> s_callbacks;

void InvokeConfigChangedCallbacks();

Section* GetOrCreateSection(System system, const std::string& section_name)
{
  return s_layers[LayerType::Meta]->GetOrCreateSection(system, section_name);
}

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

void AddLoadLayer(std::unique_ptr<Layer> layer)
{
  layer->Load();
  AddLayer(std::move(layer));
}

void AddLoadLayer(std::unique_ptr<ConfigLayerLoader> loader)
{
  AddLoadLayer(std::make_unique<Layer>(std::move(loader)));
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
  for (const auto& callback : s_callbacks)
    callback();
}

// Explicit load and save of layers
void Load()
{
  for (auto& layer : s_layers)
    layer.second->Load();
}

void Save()
{
  for (auto& layer : s_layers)
    layer.second->Save();
}

void Init()
{
  // These layers contain temporary values
  s_layers[LayerType::CommandLine] = std::make_unique<Layer>(LayerType::CommandLine);
  ClearCurrentRunLayer();
  // This layer always has to exist
  s_layers[LayerType::Meta] = std::make_unique<RecursiveLayer>();
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
    {System::Main, "Dolphin"},          {System::GCPad, "GCPad"},  {System::WiiPad, "Wiimote"},
    {System::GCKeyboard, "GCKeyboard"}, {System::GFX, "Graphics"}, {System::Logger, "Logger"},
    {System::Debugger, "Debugger"},     {System::UI, "UI"},
};

const std::string& GetSystemName(System system)
{
  return system_to_name.at(system);
}

System GetSystemFromName(const std::string& name)
{
  const auto system = std::find_if(system_to_name.begin(), system_to_name.end(),
                                   [&name](const auto& entry) { return entry.second == name; });
  if (system != system_to_name.end())
    return system->first;

  _assert_msg_(COMMON, false, "Programming error! Couldn't convert '%s' to system!", name.c_str());
  return System::Main;
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
      {LayerType::Meta, "Top"},
  };
  return layer_to_name.at(layer);
}
}
