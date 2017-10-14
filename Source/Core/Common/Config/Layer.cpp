// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <map>

#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "Common/Config/Section.h"

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

bool Layer::Exists(System system, const std::string& section_name, const std::string& key)
{
  Section* section = GetSection(system, section_name);
  if (!section)
    return false;
  return section->Exists(key);
}

bool Layer::DeleteKey(System system, const std::string& section_name, const std::string& key)
{
  Section* section = GetSection(system, section_name);
  if (!section)
    return false;
  return section->Delete(key);
}

Section* Layer::GetSection(System system, const std::string& section_name)
{
  for (auto& section : m_sections[system])
    if (!strcasecmp(section->m_name.c_str(), section_name.c_str()))
      return section.get();
  return nullptr;
}

Section* Layer::GetOrCreateSection(System system, const std::string& section_name)
{
  Section* section = GetSection(system, section_name);
  if (!section)
  {
    if (m_layer == LayerType::Meta)
    {
      m_sections[system].emplace_back(
          std::make_unique<RecursiveSection>(m_layer, system, section_name));
    }
    else
    {
      m_sections[system].emplace_back(std::make_unique<Section>(m_layer, system, section_name));
    }
    section = m_sections[system].back().get();
  }
  return section;
}

void Layer::Load()
{
  if (m_loader)
    m_loader->Load(this);
  ClearDirty();
  InvokeConfigChangedCallbacks();
}

void Layer::Save()
{
  if (!m_loader || !IsDirty())
    return;

  m_loader->Save(this);
  ClearDirty();
  InvokeConfigChangedCallbacks();
}

LayerType Layer::GetLayer() const
{
  return m_layer;
}

const LayerMap& Layer::GetLayerMap() const
{
  return m_sections;
}

bool Layer::IsDirty() const
{
  return std::any_of(m_sections.begin(), m_sections.end(), [](const auto& system) {
    return std::any_of(system.second.begin(), system.second.end(),
                       [](const auto& section) { return section->IsDirty(); });
  });
}

void Layer::ClearDirty()
{
  std::for_each(m_sections.begin(), m_sections.end(), [](auto& system) {
    std::for_each(system.second.begin(), system.second.end(),
                  [](auto& section) { section->ClearDirty(); });
  });
}

RecursiveLayer::RecursiveLayer() : Layer(LayerType::Meta)
{
}

Section* RecursiveLayer::GetSection(System system, const std::string& section_name)
{
  // Always queries backwards recursively, so it doesn't matter if it exists or not on this layer
  return GetOrCreateSection(system, section_name);
}

Section* RecursiveLayer::GetOrCreateSection(System system, const std::string& section_name)
{
  Section* section = Layer::GetSection(system, section_name);
  if (!section)
  {
    m_sections[system].emplace_back(
        std::make_unique<RecursiveSection>(m_layer, system, section_name));
    section = m_sections[system].back().get();
  }
  return section;
}
}
