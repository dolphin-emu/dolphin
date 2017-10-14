// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Common/Config/Enums.h"
#include "Common/Config/Section.h"

namespace Config
{
template <typename T>
struct ConfigInfo;

using LayerMap = std::map<System, std::vector<std::unique_ptr<Section>>>;

class ConfigLayerLoader
{
public:
  explicit ConfigLayerLoader(LayerType layer);
  virtual ~ConfigLayerLoader();
  virtual void Load(Layer* config_layer) = 0;
  virtual void Save(Layer* config_layer) = 0;

  LayerType GetLayer() const;

private:
  const LayerType m_layer;
};

class Layer
{
public:
  explicit Layer(LayerType layer);
  explicit Layer(std::unique_ptr<ConfigLayerLoader> loader);
  virtual ~Layer();

  // Convenience functions
  bool Exists(System system, const std::string& section_name, const std::string& key);
  bool DeleteKey(System system, const std::string& section_name, const std::string& key);
  template <typename T>
  bool GetIfExists(System system, const std::string& section_name, const std::string& key, T* value)
  {
    if (Exists(system, section_name, key))
      return GetOrCreateSection(system, section_name)->Get(key, value);

    return false;
  }

  virtual Section* GetSection(System system, const std::string& section_name);
  virtual Section* GetOrCreateSection(System system, const std::string& section_name);

  template <typename T>
  T Get(const ConfigInfo<T>& config_info)
  {
    return GetOrCreateSection(config_info.location.system, config_info.location.section)
        ->template Get<T>(config_info.location.key, config_info.default_value);
  }

  template <typename T>
  void Set(const ConfigInfo<T>& config_info, const T& value)
  {
    GetOrCreateSection(config_info.location.system, config_info.location.section)
        ->Set(config_info.location.key, value);
  }

  // Explicit load and save of layers
  void Load();
  void Save();

  LayerType GetLayer() const;
  const LayerMap& GetLayerMap() const;

protected:
  bool IsDirty() const;
  void ClearDirty();

  LayerMap m_sections;
  const LayerType m_layer;
  std::unique_ptr<ConfigLayerLoader> m_loader;
};

class RecursiveLayer final : public Layer
{
public:
  RecursiveLayer();
  Section* GetSection(System system, const std::string& section_name) override;
  Section* GetOrCreateSection(System system, const std::string& section_name) override;
};
}
