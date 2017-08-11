// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "Common/Config/Enums.h"
#include "Common/Config/Layer.h"
#include "Common/Config/Section.h"

namespace Config
{
struct ConfigLocation
{
  System system;
  std::string section;
  std::string key;

  bool operator==(const ConfigLocation& other) const;
  bool operator!=(const ConfigLocation& other) const;
  bool operator<(const ConfigLocation& other) const;
};

template <typename T>
struct ConfigInfo
{
  ConfigLocation location;
  T default_value;
};

using Layers = std::map<LayerType, std::unique_ptr<Layer>>;
using ConfigChangedCallback = std::function<void()>;

// Common function used for getting configuration
Section* GetOrCreateSection(System system, const std::string& section_name);

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

const std::string& GetSystemName(System system);
System GetSystemFromName(const std::string& system);
const std::string& GetLayerName(LayerType layer);
LayerType GetActiveLayerForConfig(const ConfigLocation&);

template <typename T>
T Get(LayerType layer, const ConfigInfo<T>& info)
{
  return GetLayer(layer)->Get(info);
}

template <typename T>
T Get(const ConfigInfo<T>& info)
{
  return Get(LayerType::Meta, info);
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
}
}
