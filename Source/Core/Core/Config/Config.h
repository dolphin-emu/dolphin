// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/Config/Config.h"
#include "Common/Config/Enums.h"

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

template <typename T>
T Get(LayerType layer, const ConfigInfo<T>& info)
{
  return GetLayer(layer)
      ->GetOrCreateSection(info.location.system, info.location.section)
      ->template Get<T>(info.location.key, info.default_value);
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

LayerType GetActiveLayerForConfig(const ConfigLocation&);

template <typename T>
LayerType GetActiveLayerForConfig(const ConfigInfo<T>& info)
{
  return GetActiveLayerForConfig(info.location);
}

template <typename T>
void Set(LayerType layer, const ConfigInfo<T>& info, const T& value)
{
  GetLayer(layer)
      ->GetOrCreateSection(info.location.system, info.location.section)
      ->Set(info.location.key, value);
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

}  // namespace Config
