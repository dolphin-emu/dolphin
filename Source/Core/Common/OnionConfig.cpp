// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <map>

#include "Common/Assert.h"
#include "Common/OnionConfig.h"
#include "Common/StringUtil.h"

namespace OnionConfig
{
const std::string& Section::NULL_STRING = "";
OnionBloom m_layers;
std::list<std::pair<CallbackFunction, void*>> m_callbacks;

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
  auto layers_it = m_layers.find(LayerType::LAYER_META);
  do
  {
    Section* layer_petal = layers_it->second->GetSection(m_system, m_name);
    if (layer_petal && layer_petal->Exists(key))
    {
      return true;
    }
  } while (--layers_it != m_layers.end());

  return false;
}

bool RecursiveSection::Get(const std::string& key, std::string* value,
                           const std::string& default_value) const
{
  std::array<LayerType, 7> search_order = {
      // Skip the meta layer
      LayerType::LAYER_CURRENTRUN, LayerType::LAYER_COMMANDLINE, LayerType::LAYER_MOVIE,
      LayerType::LAYER_NETPLAY,    LayerType::LAYER_LOCALGAME,   LayerType::LAYER_GLOBALGAME,
      LayerType::LAYER_BASE,
  };

  for (auto layer_id : search_order)
  {
    auto layers_it = m_layers.find(layer_id);
    if (layers_it == m_layers.end())
      continue;

    Section* layer_petal = layers_it->second->GetSection(m_system, m_name);
    if (layer_petal && layer_petal->Exists(key))
    {
      return layer_petal->Get(key, value, default_value);
    }
  }

  return Section::Get(key, value, default_value);
}

void RecursiveSection::Set(const std::string& key, const std::string& value)
{
  // The RecursiveSection can't set since it is used to recursively get values
  // from the layer map.
  // It is only a part of the meta layer, and the meta layer isn't allowed to
  // set any values.
  _assert_msg_(COMMON, false, "Don't try to set values here!");
}

class RecursiveLayer final : public Layer
{
public:
  RecursiveLayer() : Layer(LayerType::LAYER_META) {}
  Section* GetSection(System system, const std::string& petal_name) override;
  Section* GetOrCreateSection(System system, const std::string& petal_name) override;
};

Section* RecursiveLayer::GetSection(System system, const std::string& petal_name)
{
  // Always queries backwards recursively, so it doesn't matter if it exists or
  // not on this layer
  return GetOrCreateSection(system, petal_name);
}

Section* RecursiveLayer::GetOrCreateSection(System system, const std::string& petal_name)
{
  Section* petal = Layer::GetSection(system, petal_name);
  if (!petal)
  {
    m_petals[system].emplace_back(new RecursiveSection(m_layer, system, petal_name));
    petal = m_petals[system].back();
  }
  return petal;
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

// Return a list of all lines in a petal
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

bool Layer::Exists(System system, const std::string& petal_name, const std::string& key)
{
  Section* petal = GetSection(system, petal_name);
  if (!petal)
    return false;
  return petal->Exists(key);
}

bool Layer::DeleteKey(System system, const std::string& petal_name, const std::string& key)
{
  Section* petal = GetSection(system, petal_name);
  if (!petal)
    return false;
  return petal->Delete(key);
}

Section* Layer::GetSection(System system, const std::string& petal_name)
{
  for (Section* petal : m_petals[system])
    if (!strcasecmp(petal->m_name.c_str(), petal_name.c_str()))
      return petal;
  return nullptr;
}

Section* Layer::GetOrCreateSection(System system, const std::string& petal_name)
{
  Section* petal = GetSection(system, petal_name);
  if (!petal)
  {
    if (m_layer == LayerType::LAYER_META)
      m_petals[system].emplace_back(new RecursiveSection(m_layer, system, petal_name));
    else
      m_petals[system].emplace_back(new Section(m_layer, system, petal_name));
    petal = m_petals[system].back();
  }
  return petal;
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

Section* GetOrCreateSection(System system, const std::string& petal_name)
{
  return m_layers[LayerType::LAYER_META]->GetOrCreateSection(system, petal_name);
}

OnionBloom* GetFullBloom()
{
  return &m_layers;
}

void AddLayer(Layer* layer)
{
  m_layers[layer->GetLayer()] = layer;
  CallbackSystems();
}

void AddLayer(ConfigLayerLoader* loader)
{
  Layer* layer = new Layer(std::unique_ptr<ConfigLayerLoader>(loader));
  AddLayer(layer);
}

void AddLoadLayer(Layer* layer)
{
  layer->Load();
  AddLayer(layer);
}

void AddLoadLayer(ConfigLayerLoader* loader)
{
  Layer* layer = new Layer(std::unique_ptr<ConfigLayerLoader>(loader));
  layer->Load();
  AddLayer(loader);
}

Layer* GetLayer(LayerType layer)
{
  if (!LayerExists(layer))
    return nullptr;
  return m_layers[layer];
}

void RemoveLayer(LayerType layer)
{
  m_layers.erase(layer);
  CallbackSystems();
}
bool LayerExists(LayerType layer)
{
  return m_layers.find(layer) != m_layers.end();
}

void AddConfigChangedCallback(CallbackFunction func, void* user_data)
{
  m_callbacks.emplace_back(std::make_pair(func, user_data));
}

void CallbackSystems()
{
  for (auto& callback : m_callbacks)
    callback.first(callback.second);
}

// Explicit load and save of layers
void Load()
{
  for (auto& layer : m_layers)
    layer.second->Load();

  CallbackSystems();
}

void Save()
{
  for (auto& layer : m_layers)
    layer.second->Save();
}

void Init()
{
  // This layer always has to exist
  m_layers[LayerType::LAYER_META] = new RecursiveLayer();
}

void Shutdown()
{
  auto it = m_layers.begin();
  while (it != m_layers.end())
  {
    delete it->second;
    it = m_layers.erase(it);
  }

  m_callbacks.clear();
}

static std::map<System, std::string> system_to_name = {
    {System::SYSTEM_MAIN, "Dolphin"},      {System::SYSTEM_GCPAD, "GCPad"},
    {System::SYSTEM_WIIPAD, "Wiimote"},    {System::SYSTEM_GCKEYBOARD, "GCKeyboard"},
    {System::SYSTEM_GFX, "Graphics"},      {System::SYSTEM_LOGGER, "Logger"},
    {System::SYSTEM_DEBUGGER, "Debugger"}, {System::SYSTEM_UI, "UI"},
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
  return System::SYSTEM_MAIN;
}

const std::string& GetLayerName(LayerType layer)
{
  static std::map<LayerType, std::string> layer_to_name = {
      {LayerType::LAYER_BASE, "Base"},
      {LayerType::LAYER_GLOBALGAME, "Global GameINI"},
      {LayerType::LAYER_LOCALGAME, "Local GameINI"},
      {LayerType::LAYER_NETPLAY, "Netplay"},
      {LayerType::LAYER_MOVIE, "Movie"},
      {LayerType::LAYER_COMMANDLINE, "Command Line"},
      {LayerType::LAYER_CURRENTRUN, "Current Run"},
      {LayerType::LAYER_META, "Top"},
  };
  return layer_to_name[layer];
}
}
