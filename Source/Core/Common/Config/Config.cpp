// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <list>
#include <map>
#include <tuple>

#include "Common/Assert.h"
#include "Common/Config/Config.h"
#include "Common/Logging/Log.h"

namespace Config
{
static Layers s_layers;
static std::list<ConfigChangedCallback> s_callbacks;

void InvokeConfigChangedCallbacks();

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

void CreateVRGameLayer()
{
  s_layers[LayerType::VRGame] = std::make_unique<Layer>(LayerType::VRGame);
}

bool OverrideSectionWithSection(const std::string& sectionName, const std::string& sectionName2)
{
  if (!s_layers[LayerType::GlobalGame] && !s_layers[LayerType::LocalGame])
    return false;
  Section section_values_default = s_layers[LayerType::GlobalGame] ? (s_layers[LayerType::GlobalGame]->GetSection(Config::System::GFX, sectionName2)) : (s_layers[LayerType::LocalGame]->GetSection(Config::System::GFX, sectionName2));
  Section section_values_local = s_layers[LayerType::LocalGame] ? (s_layers[LayerType::LocalGame]->GetSection(Config::System::GFX, sectionName2)) : (s_layers[LayerType::GlobalGame]->GetSection(Config::System::GFX, sectionName2));
  if (section_values_default.begin() == section_values_default.end() && section_values_local.begin() == section_values_local.end())
    return false;

  //Section* section =
  //    s_layers[LayerType::VRGame]->GetOrCreateSection(Config::System::GFX, sectionName);
  for (const auto& value : section_values_default)
  {
	if (value.second)
      NOTICE_LOG(VR, "Override [%s] %s with game VR default [%s] %s =\"%s\"", sectionName.c_str(),
               value.first.key.c_str(), sectionName2.c_str(), value.first.key.c_str(),
               value.second->c_str());
	else
		NOTICE_LOG(VR, "Override [%s] %s with game VR default [%s] %s = (deleted)", sectionName.c_str(),
			value.first.key.c_str(), sectionName2.c_str(), value.first.key.c_str());

	if (value.second)
	  s_layers[LayerType::VRGame]->Set(value.first, *value.second);
	else
		s_layers[LayerType::VRGame]->DeleteKey(value.first);
  }
  for (const auto& value : section_values_local)
  {
	  if (value.second)
		  NOTICE_LOG(VR, "Override [%s] %s with game VR user setting [%s] %s =\"%s\"", sectionName.c_str(),
			  value.first.key.c_str(), sectionName2.c_str(), value.first.key.c_str(),
			  value.second->c_str());
	  else
		  NOTICE_LOG(VR, "Override [%s] %s with game VR user setting [%s] %s = (deleted)", sectionName.c_str(),
			  value.first.key.c_str(), sectionName2.c_str(), value.first.key.c_str());

	  if (value.second)
		  s_layers[LayerType::VRGame]->Set(value.first, *value.second);
	  else
		  s_layers[LayerType::VRGame]->DeleteKey(value.first);
  }
  return true;
}

static const std::map<System, std::string> system_to_name = {
    {System::Main, "Dolphin"},          {System::GCPad, "GCPad"},  {System::WiiPad, "Wiimote"},
    {System::GCKeyboard, "GCKeyboard"}, {System::GFX, "Graphics"}, {System::Logger, "Logger"},
    {System::Debugger, "Debugger"},     {System::UI, "UI"},        {System::SYSCONF, "SYSCONF"}};

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
      {LayerType::VRGame, "VR GameINI"},
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
}
