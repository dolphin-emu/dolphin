// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <map>

#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "Common/Logging/Log.h"

namespace Config
{
namespace detail
{
std::string ValueToString(u16 value)
{
  return StringFromFormat("0x%04x", value);
}

std::string ValueToString(u32 value)
{
  return StringFromFormat("0x%08x", value);
}

std::string ValueToString(float value)
{
  return StringFromFormat("%#.9g", value);
}

std::string ValueToString(double value)
{
  return StringFromFormat("%#.17g", value);
}

std::string ValueToString(int value)
{
  return std::to_string(value);
}

std::string ValueToString(bool value)
{
  return StringFromBool(value);
}

std::string ValueToString(const std::string& value)
{
  return value;
}
}

ConfigLayerLoader::ConfigLayerLoader(LayerType layer) : m_layer(layer)
{
}

ConfigLayerLoader::~ConfigLayerLoader() = default;

LayerType ConfigLayerLoader::GetLayer() const
{
  return m_layer;
}

Layer::Layer(LayerType type) : m_layer(type)
{
}

Layer::Layer(std::unique_ptr<ConfigLayerLoader> loader)
    : m_layer(loader->GetLayer()), m_loader(std::move(loader))
{
  Load();
}

Layer::~Layer()
{
  Save();
}

bool Layer::Exists(const ConfigLocation& location) const
{
  const auto iter = m_map.find(location);
  return iter != m_map.end() && iter->second.has_value();
}

bool Layer::DeleteKey(const ConfigLocation& location)
{
  INFO_LOG(CORE, "Delete %s %s.%s.%s", GetLayerName(m_layer).c_str(),
           GetSystemName(location.system).c_str(), location.section.c_str(), location.key.c_str());
  m_is_dirty = true;
  bool had_value = m_map[location].has_value();
  m_map[location].reset();
  return had_value;
}

bool Layer::DeleteSection(System system, const std::string& section_name)
{
  return false;
}

void Layer::DeleteAllKeys()
{
  m_is_dirty = true;
  for (auto& pair : m_map)
  {
    pair.second.reset();
  }
}

Section Layer::GetSection(System system, const std::string& section)
{
  return Section{m_map.lower_bound(ConfigLocation{system, section, ""}),
                 m_map.lower_bound(ConfigLocation{system, section + '\001', ""})};
}

ConstSection Layer::GetSection(System system, const std::string& section) const
{
  return ConstSection{m_map.lower_bound(ConfigLocation{system, section, ""}),
                      m_map.lower_bound(ConfigLocation{system, section + '\001', ""})};
}

void Layer::Load()
{
  if (m_loader)
    m_loader->Load(this);
  m_is_dirty = false;
  InvokeConfigChangedCallbacks();
}

void Layer::Save()
{
  if (!m_loader || !m_is_dirty)
    return;

  m_loader->Save(this);
  m_is_dirty = false;
  InvokeConfigChangedCallbacks();
}

LayerType Layer::GetLayer() const
{
  return m_layer;
}

const LayerMap& Layer::GetLayerMap() const
{
  return m_map;
}
}
