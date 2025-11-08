// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "Common/Config/ConfigInfo.h"
#include "Common/Config/Enums.h"
#include "Common/Config/Layer.h"

namespace Config
{
struct ConfigChangedCallbackID
{
  size_t id = std::numeric_limits<size_t>::max();

  bool operator==(const ConfigChangedCallbackID&) const = default;
};

using ConfigChangedCallback = std::function<void()>;

// Layer management
void AddLayer(std::unique_ptr<ConfigLayerLoader> loader);
std::shared_ptr<Layer> GetLayer(LayerType layer);
void RemoveLayer(LayerType layer);

// Returns an ID that should be passed to RemoveConfigChangedCallback() when the callback is no
// longer needed. The callback may be called from any thread.
[[nodiscard]] ConfigChangedCallbackID AddConfigChangedCallback(ConfigChangedCallback func);
void RemoveConfigChangedCallback(ConfigChangedCallbackID callback_id);
void OnConfigChanged();

// Returns the number of times the config has changed in the current execution of the program
u64 GetConfigVersion();

// Explicit load and save of layers
void Load();
void Save();

void Init();
void Shutdown();
void ClearCurrentRunLayer();

const std::string& GetSystemName(System system);
std::optional<System> GetSystemFromName(const std::string& system);
const std::string& GetLayerName(LayerType layer);
LayerType GetActiveLayerForConfig(const Location&);

std::optional<std::string> GetAsString(const Location&);

template <typename T>
T Get(LayerType layer, const Info<T>& info)
{
  if (layer == LayerType::Meta)
    return Get(info);
  return GetLayer(layer)->Get(info);
}

template <typename T>
T Get(const Info<T>& info)
{
  CachedValue<T> cached = info.GetCachedValue();
  const u64 config_version = GetConfigVersion();

  if (cached.config_version < config_version)
  {
    cached.value = GetUncached(info);
    cached.config_version = config_version;

    info.TryToSetCachedValue(cached);
  }

  return std::move(cached.value);
}

template <typename T>
T GetUncached(const Info<T>& info)
{
  const std::optional<std::string> str = GetAsString(info.GetLocation());
  if (!str)
    return info.GetDefaultValue();

  return detail::TryParse<T>(*str).value_or(info.GetDefaultValue());
}

template <typename T>
T GetBase(const Info<T>& info)
{
  return Get(LayerType::Base, info);
}

template <typename T>
LayerType GetActiveLayerForConfig(const Info<T>& info)
{
  return GetActiveLayerForConfig(info.GetLocation());
}

template <typename InfoT, typename ValueT>
void Set(LayerType layer, const InfoT& info, const ValueT& value)
{
  if (GetLayer(layer)->Set(info, value))
    OnConfigChanged();
}

template <typename InfoT, typename ValueT>
void SetBase(const Info<InfoT>& info, const ValueT& value)
{
  Set(LayerType::Base, info, value);
}

template <typename InfoT, typename ValueT>
void SetCurrent(const Info<InfoT>& info, const ValueT& value)
{
  Set(LayerType::CurrentRun, info, value);
}

template <typename InfoT, typename ValueT>
void SetBaseOrCurrent(const Info<InfoT>& info, const ValueT& value)
{
  if (GetActiveLayerForConfig(info) == LayerType::Base)
    Set(LayerType::Base, info, value);
  else
    Set(LayerType::CurrentRun, info, value);
}

template <typename T>
void DeleteKey(LayerType layer, const Info<T>& info)
{
  if (GetLayer(layer)->DeleteKey(info.GetLocation()))
    OnConfigChanged();
}

// Used to defer OnConfigChanged until after the completion of many config changes.
class ConfigChangeCallbackGuard
{
public:
  ConfigChangeCallbackGuard();
  ~ConfigChangeCallbackGuard();

  ConfigChangeCallbackGuard(const ConfigChangeCallbackGuard&) = delete;
  ConfigChangeCallbackGuard& operator=(const ConfigChangeCallbackGuard&) = delete;
};
}  // namespace Config
