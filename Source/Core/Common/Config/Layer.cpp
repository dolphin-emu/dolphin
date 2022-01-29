// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Config/Layer.h"

#include <algorithm>
#include <cstring>
#include <map>

#include "Common/Config/Config.h"

namespace Config
{
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

bool Layer::Exists(const Location& location) const
{
  const auto iter = m_map.find(location);
  return iter != m_map.end() && iter->second.has_value();
}

bool Layer::DeleteKey(const Location& location)
{
  m_is_dirty = true;
  bool had_value = false;
  const auto iter = m_map.find(location);
  if (iter != m_map.end() && iter->second.has_value())
  {
    iter->second.reset();
    had_value = true;
  }

  return had_value;
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
  return Section{m_map.lower_bound(Location{system, section, ""}),
                 m_map.lower_bound(Location{system, section + '\001', ""})};
}

ConstSection Layer::GetSection(System system, const std::string& section) const
{
  return ConstSection{m_map.lower_bound(Location{system, section, ""}),
                      m_map.lower_bound(Location{system, section + '\001', ""})};
}

void Layer::Load()
{
  if (m_loader)
    m_loader->Load(this);
  m_is_dirty = false;
}

void Layer::Save()
{
  if (!m_loader || !m_is_dirty)
    return;

  m_loader->Save(this);
  m_is_dirty = false;
}

LayerType Layer::GetLayer() const
{
  return m_layer;
}

const LayerMap& Layer::GetLayerMap() const
{
  return m_map;
}
}  // namespace Config
