// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Common/Config/ConfigInfo.h"
#include "Common/Config/Enums.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

namespace Config
{
namespace detail
{
std::string ValueToString(u16 value);
std::string ValueToString(u32 value);
std::string ValueToString(float value);
std::string ValueToString(double value);
std::string ValueToString(int value);
std::string ValueToString(bool value);
std::string ValueToString(const std::string& value);

template <typename T>
std::optional<T> TryParse(const std::string& str_value)
{
  T value;
  if (!::TryParse(str_value, &value))
    return std::nullopt;
  return value;
}

template <>
inline std::optional<std::string> TryParse(const std::string& str_value)
{
  return str_value;
}
}

template <typename T>
struct ConfigInfo;

class Layer;
using LayerMap = std::map<ConfigLocation, std::optional<std::string>>;

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

class Section
{
public:
  using iterator = LayerMap::iterator;
  Section(iterator begin_, iterator end_) : m_begin(begin_), m_end(end_) {}
  iterator begin() const { return m_begin; }
  iterator end() const { return m_end; }
private:
  iterator m_begin;
  iterator m_end;
};

class ConstSection
{
public:
  using iterator = LayerMap::const_iterator;
  ConstSection(iterator begin_, iterator end_) : m_begin(begin_), m_end(end_) {}
  iterator begin() const { return m_begin; }
  iterator end() const { return m_end; }
private:
  iterator m_begin;
  iterator m_end;
};

class Layer
{
public:
  explicit Layer(LayerType layer);
  explicit Layer(std::unique_ptr<ConfigLayerLoader> loader);
  virtual ~Layer();

  // Convenience functions
  bool Exists(const ConfigLocation& location) const;
  bool DeleteKey(const ConfigLocation& location);
  void DeleteAllKeys();

  bool DeleteSection(System system, const std::string& section_name);
  template <typename T>
  T Get(const ConfigInfo<T>& config_info)
  {
    return Get<T>(config_info.location).value_or(config_info.default_value);
  }

  template <typename T>
  std::optional<T> Get(const ConfigLocation& location)
  {
    const std::optional<std::string>& str_value = m_map[location];
    if (!str_value)
      return std::nullopt;
    return detail::TryParse<T>(*str_value);
  }

  template <typename T>
  void Set(const ConfigInfo<T>& config_info, const T& value)
  {
    Set<T>(config_info.location, value);
  }

  template <typename T>
  void Set(const ConfigLocation& location, const T& value)
  {
    const std::string new_value = detail::ValueToString(value);
    INFO_LOG(CORE, "%s %s.%s.%s=\"%s\"", GetLayerName(m_layer).c_str(),
             GetSystemName(location.system).c_str(), location.section.c_str(), location.key.c_str(),
             new_value.c_str());
    std::optional<std::string>& current_value = m_map[location];
    if (current_value == new_value)
      return;
    m_is_dirty = true;
    current_value = new_value;
  }

  Section GetSection(System system, const std::string& section);
  ConstSection GetSection(System system, const std::string& section) const;

  // Explicit load and save of layers
  void Load();
  void Save();

  void Clear() { m_map.clear(); }

  LayerType GetLayer() const;
  const LayerMap& GetLayerMap() const;

protected:
  bool m_is_dirty = false;
  LayerMap m_map;
  const LayerType m_layer;
  std::unique_ptr<ConfigLayerLoader> m_loader;
};
}
