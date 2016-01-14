// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <map>

#include "Common/Assert.h"
#include "Common/Config.h"
#include "Common/StringUtil.h"

namespace Config
{
const std::string& Section::NULL_STRING = "";
static Layers s_layers;
static std::list<ConfigChangedCallback> s_callbacks;

void CallbackSystems();

// Only to be used with the meta-layer
class RecursiveSection final : public Section
{
public:
  RecursiveSection(LayerType layer, System system, const std::string& name)
      : Section(layer, system, name)
  {
  }

  bool Exists(const std::string& key) const override;

  bool Get(const std::string& key, std::string* value,
           const std::string& default_value = NULL_STRING) const override;

  void Set(const std::string& key, const std::string& value) override;
};

bool RecursiveSection::Exists(const std::string& key) const
{
  auto layers_it = s_layers.find(LayerType::Meta);
  do
  {
    Section* layer_section = layers_it->second->GetSection(m_system, m_name);
    if (layer_section && layer_section->Exists(key))
    {
      return true;
    }
  } while (--layers_it != s_layers.end());

  return false;
}

bool RecursiveSection::Get(const std::string& key, std::string* value,
                           const std::string& default_value) const
{
  static const std::array<LayerType, 7> search_order = {{
      // Skip the meta layer
      LayerType::CurrentRun, LayerType::CommandLine, LayerType::Movie, LayerType::Netplay,
      LayerType::LocalGame, LayerType::GlobalGame, LayerType::Base,
  }};

  for (auto layer_id : search_order)
  {
    auto layers_it = s_layers.find(layer_id);
    if (layers_it == s_layers.end())
      continue;

    Section* layer_section = layers_it->second->GetSection(m_system, m_name);
    if (layer_section && layer_section->Exists(key))
    {
      return layer_section->Get(key, value, default_value);
    }
  }

  return Section::Get(key, value, default_value);
}

void RecursiveSection::Set(const std::string& key, const std::string& value)
{
  // The RecursiveSection can't set since it is used to recursively get values from the layer
  // map.
  // It is only a part of the meta layer, and the meta layer isn't allowed to set any values.
  _assert_msg_(COMMON, false, "Don't try to set values here!");
}

class RecursiveLayer final : public Layer
{
public:
  RecursiveLayer() : Layer(LayerType::Meta) {}
  Section* GetSection(System system, const std::string& section_name) override;
  Section* GetOrCreateSection(System system, const std::string& section_name) override;
};

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
    m_sections[system].emplace_back(new RecursiveSection(m_layer, system, section_name));
    section = m_sections[system].back();
  }
  return section;
}

bool Section::Exists(const std::string& key) const
{
  return m_values.find(key) != m_values.end();
}

bool Section::Delete(const std::string& key)
{
  auto it = m_values.find(key);
  if (it == m_values.end())
    return false;

  m_values.erase(it);
  return true;
}

void Section::Set(const std::string& key, const std::string& value)
{
  auto it = m_values.find(key);
  if (it != m_values.end())
    it->second = value;
  else
    m_values[key] = value;
}

void Section::Set(const std::string& key, u32 newValue)
{
  Section::Set(key, StringFromFormat("0x%08x", newValue));
}

void Section::Set(const std::string& key, float newValue)
{
  Section::Set(key, StringFromFormat("%#.9g", newValue));
}

void Section::Set(const std::string& key, double newValue)
{
  Section::Set(key, StringFromFormat("%#.17g", newValue));
}

void Section::Set(const std::string& key, int newValue)
{
  Section::Set(key, StringFromInt(newValue));
}

void Section::Set(const std::string& key, bool newValue)
{
  Section::Set(key, StringFromBool(newValue));
}

void Section::Set(const std::string& key, const std::string& newValue,
                  const std::string& defaultValue)
{
  if (newValue != defaultValue)
    Set(key, newValue);
  else
    Delete(key);
}

bool Section::Get(const std::string& key, std::string* value,
                  const std::string& default_value) const
{
  const auto& it = m_values.find(key);
  if (it != m_values.end())
  {
    *value = it->second;
    return true;
  }
  else if (&default_value != &NULL_STRING)
  {
    *value = default_value;
    return true;
  }

  return false;
}

bool Section::Get(const std::string& key, int* value, int defaultValue) const
{
  std::string temp;
  bool retval = Get(key, &temp);

  if (retval && TryParse(temp, value))
    return true;

  *value = defaultValue;
  return false;
}

bool Section::Get(const std::string& key, u32* value, u32 defaultValue) const
{
  std::string temp;
  bool retval = Get(key, &temp);

  if (retval && TryParse(temp, value))
    return true;

  *value = defaultValue;
  return false;
}

bool Section::Get(const std::string& key, bool* value, bool defaultValue) const
{
  std::string temp;
  bool retval = Get(key, &temp);

  if (retval && TryParse(temp, value))
    return true;

  *value = defaultValue;
  return false;
}

bool Section::Get(const std::string& key, float* value, float defaultValue) const
{
  std::string temp;
  bool retval = Get(key, &temp);

  if (retval && TryParse(temp, value))
    return true;

  *value = defaultValue;
  return false;
}

bool Section::Get(const std::string& key, double* value, double defaultValue) const
{
  std::string temp;
  bool retval = Get(key, &temp);

  if (retval && TryParse(temp, value))
    return true;

  *value = defaultValue;
  return false;
}

// Return a list of all lines in a section
bool Section::GetLines(std::vector<std::string>* lines, const bool remove_comments) const
{
  lines->clear();

  for (std::string line : m_lines)
  {
    line = StripSpaces(line);

    if (remove_comments)
    {
      size_t commentPos = line.find('#');
      if (commentPos == 0)
      {
        continue;
      }

      if (commentPos != std::string::npos)
      {
        line = StripSpaces(line.substr(0, commentPos));
      }
    }

    lines->push_back(line);
  }

  return true;
}

// Onion layers
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
  for (Section* section : m_sections[system])
    if (!strcasecmp(section->m_name.c_str(), section_name.c_str()))
      return section;
  return nullptr;
}

Section* Layer::GetOrCreateSection(System system, const std::string& section_name)
{
  Section* section = GetSection(system, section_name);
  if (!section)
  {
    if (m_layer == LayerType::Meta)
      m_sections[system].emplace_back(new RecursiveSection(m_layer, system, section_name));
    else
      m_sections[system].emplace_back(new Section(m_layer, system, section_name));
    section = m_sections[system].back();
  }
  return section;
}

void Layer::Load()
{
  if (m_loader)
    m_loader->Load(this);
  CallbackSystems();
}

void Layer::Save()
{
  if (m_loader)
    m_loader->Save(this);
}

Section* GetOrCreateSection(System system, const std::string& section_name)
{
  return s_layers[LayerType::Meta]->GetOrCreateSection(system, section_name);
}

Layers* GetLayers()
{
  return &s_layers;
}

void AddLayer(std::unique_ptr<Layer> layer)
{
  s_layers[layer->GetLayer()] = std::move(layer);
  CallbackSystems();
}

void AddLayer(std::unique_ptr<ConfigLayerLoader> loader)
{
  AddLayer(std::make_unique<Layer>(std::move(loader)));
}

void AddLoadLayer(std::unique_ptr<Layer> layer)
{
  layer->Load();
  AddLayer(std::move(layer));
}

void AddLoadLayer(std::unique_ptr<ConfigLayerLoader> loader)
{
  AddLoadLayer(std::make_unique<Layer>(std::move(loader)));
}

Layer* GetLayer(LayerType layer)
{
  if (!LayerExists(layer))
    return nullptr;
  return s_layers[layer].get();
}

void RemoveLayer(LayerType layer)
{
  s_layers.erase(layer);
  CallbackSystems();
}
bool LayerExists(LayerType layer)
{
  return s_layers.find(layer) != s_layers.end();
}

void AddConfigChangedCallback(ConfigChangedCallback func)
{
  s_callbacks.emplace_back(func);
}

void CallbackSystems()
{
  for (const auto& callback : s_callbacks)
    callback();
}

// Explicit load and save of layers
void Load()
{
  for (auto& layer : s_layers)
    layer.second->Load();

  CallbackSystems();
}

void Save()
{
  for (auto& layer : s_layers)
    layer.second->Save();
}

void Init()
{
  // This layer always has to exist
  s_layers[LayerType::Meta] = std::make_unique<RecursiveLayer>();
}

void Shutdown()
{
  s_layers.clear();
  s_callbacks.clear();
}

static std::map<System, std::string> system_to_name = {
    {System::Main, "Dolphin"},          {System::GCPad, "GCPad"},  {System::WiiPad, "Wiimote"},
    {System::GCKeyboard, "GCKeyboard"}, {System::GFX, "Graphics"}, {System::Logger, "Logger"},
    {System::Debugger, "Debugger"},     {System::UI, "UI"},
};

const std::string& GetSystemName(System system)
{
  return system_to_name[system];
}

System GetSystemFromName(const std::string& system)
{
  for (auto& val : system_to_name)
    if (val.second == system)
      return val.first;

  _assert_msg_(COMMON, false, "Programming error! Couldn't convert '%s' to system!",
               system.c_str());
  return System::Main;
}

const std::string& GetLayerName(LayerType layer)
{
  static std::map<LayerType, std::string> layer_to_name = {
      {LayerType::Base, "Base"},
      {LayerType::GlobalGame, "Global GameINI"},
      {LayerType::LocalGame, "Local GameINI"},
      {LayerType::Netplay, "Netplay"},
      {LayerType::Movie, "Movie"},
      {LayerType::CommandLine, "Command Line"},
      {LayerType::CurrentRun, "Current Run"},
      {LayerType::Meta, "Top"},
  };
  return layer_to_name[layer];
}
}
