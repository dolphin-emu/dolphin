// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "Common/Config/ConfigInfo.h"
#include "Common/Config/Enums.h"
#include "Common/StringUtil.h"

namespace Config
{
namespace detail
{
template <typename T, std::enable_if_t<!std::is_enum<T>::value>* = nullptr>
std::optional<T> TryParse(const std::string& str_value)
{
  T value;
  if (!::TryParse(str_value, &value))
    return std::nullopt;
  return value;
}

template <typename T, std::enable_if_t<std::is_enum<T>::value>* = nullptr>
std::optional<T> TryParse(const std::string& str_value)
{
  const auto result = TryParse<std::underlying_type_t<T>>(str_value);
  if (result)
    return static_cast<T>(*result);
  return {};
}

template <>
inline std::optional<std::string> TryParse(const std::string& str_value)
{
  return str_value;
}
}  // namespace detail

template <typename T>
class Info;

class Layer;
using LayerMap = std::map<Location, std::optional<std::string>>;

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
  bool Exists(const Location& location) const;
  bool DeleteKey(const Location& location);
  void DeleteAllKeys();

  template <typename T>
  T Get(const Info<T>& config_info) const
  {
    return Get<T>(config_info.GetLocation()).value_or(config_info.GetDefaultValue());
  }

  template <typename T>
  std::optional<T> Get(const Location& location) const
  {
    const auto iter = m_map.find(location);
    if (iter == m_map.end() || !iter->second.has_value())
      return std::nullopt;
    return detail::TryParse<T>(*iter->second);
  }

  template <typename T>
  bool Set(const Info<T>& config_info, const std::common_type_t<T>& value)
  {
    return Set(config_info.GetLocation(), value);
  }

  template <typename T>
  bool Set(const Location& location, const T& value)
  {
    return Set(location, ValueToString(value));
  }

  bool Set(const Location& location, std::string new_value)
  {
    const auto iter = m_map.find(location);
    if (iter != m_map.end() && iter->second == new_value)
      return false;
    m_is_dirty = true;
    m_map.insert_or_assign(location, std::move(new_value));
    return true;
  }

  void MarkAsDirty() { m_is_dirty = true; }

  Section GetSection(System system, const std::string& section);
  ConstSection GetSection(System system, const std::string& section) const;

  // Explicit load and save of layers
  void Load();
  void Save();

  LayerType GetLayer() const;
  const LayerMap& GetLayerMap() const;

protected:
  bool m_is_dirty = false;
  LayerMap m_map;
  const LayerType m_layer;
  std::unique_ptr<ConfigLayerLoader> m_loader;
};
}  // namespace Config
