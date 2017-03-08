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
using Layers = std::map<LayerType, std::unique_ptr<Layer>>;
using ConfigChangedCallback = std::function<void()>;

// Common function used for getting configuration
Section* GetOrCreateSection(System system, const std::string& section_name);

// Layer management
Layers* GetLayers();
void AddLayer(std::unique_ptr<Layer> layer);
void AddLayer(std::unique_ptr<ConfigLayerLoader> loader);
void AddLoadLayer(std::unique_ptr<Layer> layer);
void AddLoadLayer(std::unique_ptr<ConfigLayerLoader> loader);
Layer* GetLayer(LayerType layer);
void RemoveLayer(LayerType layer);
bool LayerExists(LayerType layer);

void AddConfigChangedCallback(ConfigChangedCallback func);
void InvokeConfigChangedCallbacks();

// Explicit load and save of layers
void Load();
void Save();

// Often used functions for getting or setting configuration on the base layer for the main system
template <typename T>
T Get(const std::string& section_name, const std::string& key, const T& default_value)
{
  auto base_layer = GetLayer(Config::LayerType::Base);
  return base_layer->GetOrCreateSection(Config::System::Main, section_name)
      ->Get(key, default_value);
}

template <typename T>
void Set(const std::string& section_name, const std::string& key, const T& value)
{
  auto base_layer = GetLayer(Config::LayerType::Base);
  base_layer->GetOrCreateSection(Config::System::Main, section_name)->Set(key, value);
}

void Init();
void Shutdown();

const std::string& GetSystemName(System system);
System GetSystemFromName(const std::string& system);
const std::string& GetLayerName(LayerType layer);
}
