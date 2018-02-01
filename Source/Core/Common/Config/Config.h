// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "Common/Config/ConfigInfo.h"
#include "Common/Config/Enums.h"
#include "Common/Config/Layer.h"

namespace Config
{
using Layers = std::map<LayerType, std::unique_ptr<Layer>>;
using ConfigChangedCallback = std::function<void()>;

// Layer management
Layers* GetLayers();
void AddLayer(std::unique_ptr<Layer> layer);
void AddLayer(std::unique_ptr<ConfigLayerLoader> loader);
Layer* GetLayer(LayerType layer);
void RemoveLayer(LayerType layer);
bool LayerExists(LayerType layer);

void AddConfigChangedCallback(ConfigChangedCallback func);
void InvokeConfigChangedCallbacks();

// Explicit load and save of layers
void Load();
void Save();

void Init();
void Shutdown();
void ClearCurrentRunLayer();
void CreateVRGameLayer();
bool OverrideSectionWithSection(const std::string& sectionName, const std::string& sectionName2);

const std::string& GetSystemName(System system);
System GetSystemFromName(const std::string& system);
const std::string& GetLayerName(LayerType layer);
LayerType GetActiveLayerForConfig(const ConfigLocation&);

template <typename T>
T Get(LayerType layer, const ConfigInfo<T>& info)
{
  if (layer == LayerType::Meta)
    return Get(info);
  if (!LayerExists(layer))
    return info.default_value;
  return GetLayer(layer)->Get(info);
}

template <typename T>
T Get(const ConfigInfo<T>& info)
{
  return GetLayer(GetActiveLayerForConfig(info.location))->Get(info);
}

template <typename T>
T GetBase(const ConfigInfo<T>& info)
{
  return Get(LayerType::Base, info);
}

template <typename T>
LayerType GetActiveLayerForConfig(const ConfigInfo<T>& info)
{
  return GetActiveLayerForConfig(info.location);
}

template <typename T>
void Set(LayerType layer, const ConfigInfo<T>& info, const T& value)
{
  GetLayer(layer)->Set(info, value);
  InvokeConfigChangedCallbacks();
}

template <typename T>
void SetBase(const ConfigInfo<T>& info, const T& value)
{
  Set<T>(LayerType::Base, info, value);
}

template <typename T>
void SetCurrent(const ConfigInfo<T>& info, const T& value)
{
  Set<T>(LayerType::CurrentRun, info, value);
}

template <typename T>
void SetBaseOrCurrent(const ConfigInfo<T>& info, const T& value)
{
  if (GetActiveLayerForConfig(info) == LayerType::Base)
    Set<T>(LayerType::Base, info, value);
  else
    Set<T>(LayerType::CurrentRun, info, value);
  // Carl: TODO: pass in active layer's value as the default?
}

template <typename T>
void ResetToGameDefault(const ConfigInfo<T>& info)
{
  Config::Layer* base = GetLayer(Config::LayerType::Base);
  Config::Layer* run = GetLayer(Config::LayerType::CurrentRun);
  Config::Layer* local = GetLayer(Config::LayerType::LocalGame);
  if (base)
    base->DeleteKey(info.location);
  if (run)
    run->DeleteKey(info.location);
  if (local)
    local->DeleteKey(info.location);
}

template <typename T>
void SaveIfNotDefault(const ConfigInfo<T>& info)
{
  T default = Config::Get(Config::LayerType::GlobalGame, info);
  T value = Config::Get(info);
  if (value != default)
  {
    Config::Set(Config::LayerType::LocalGame, info, value);
    Config::Layer* layer = Config::GetLayer(Config::LayerType::Base);
    if (layer)
      layer->DeleteKey(info.location);
    layer = Config::GetLayer(Config::LayerType::CurrentRun);
    if (layer)
      layer->DeleteKey(info.location);
  }
  else
  {
    Config::Layer* layer = Config::GetLayer(Config::LayerType::LocalGame);
    if (layer)
      layer->DeleteKey(info.location);
  }
}

}
