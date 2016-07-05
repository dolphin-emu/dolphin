// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <list>
#include <map>
#include <memory>
#include <string>

// XXX: Purely for case insensitive compare
#include "Common/IniFile.h"

namespace OnionConfig
{
enum class LayerType
{
  LAYER_BASE,
  LAYER_GLOBALGAME,
  LAYER_LOCALGAME,
  LAYER_MOVIE,
  LAYER_NETPLAY,
  LAYER_COMMANDLINE,
  LAYER_CURRENTRUN,
  LAYER_META,
};

enum class System
{
  SYSTEM_MAIN,
  SYSTEM_GCPAD,
  SYSTEM_WIIPAD,
  SYSTEM_GCKEYBOARD,
  SYSTEM_GFX,
  SYSTEM_LOGGER,
  SYSTEM_DEBUGGER,
  SYSTEM_UI,
};

class Section;
class Layer;
class ConfigLayerLoader;

using SectionValueMap = std::map<std::string, std::string, CaseInsensitiveStringCompare>;
using LayerMap = std::map<System, std::list<Section*>>;
using OnionBloom = std::map<LayerType, Layer*>;
using CallbackFunction = std::function<void(void*)>;

class Section
{
  friend Layer;
  friend ConfigLayerLoader;

public:
  Section(LayerType layer, System system, const std::string& name)
      : m_layer(layer), m_system(system), m_name(name)
  {
  }

  virtual bool Exists(const std::string& key) const;
  bool Delete(const std::string& key);

  // Setters
  virtual void Set(const std::string& key, const std::string& value);

  void Set(const std::string& key, u32 newValue);
  void Set(const std::string& key, float newValue);
  void Set(const std::string& key, double newValue);
  void Set(const std::string& key, int newValue);
  void Set(const std::string& key, bool newValue);

  // Setters with default values
  void Set(const std::string& key, const std::string& newValue, const std::string& defaultValue);
  template <typename T>
  void Set(const std::string& key, T newValue, const T defaultValue)
  {
    if (newValue != defaultValue)
      Set(key, newValue);
    else
      Delete(key);
  }

  // Getters
  virtual bool Get(const std::string& key, std::string* value,
                   const std::string& default_value = NULL_STRING) const;

  bool Get(const std::string& key, int* value, int defaultValue = 0) const;
  bool Get(const std::string& key, u32* value, u32 defaultValue = 0) const;
  bool Get(const std::string& key, bool* value, bool defaultValue = false) const;
  bool Get(const std::string& key, float* value, float defaultValue = 0.0f) const;
  bool Get(const std::string& key, double* value, double defaultValue = 0.0) const;

  // Section chunk
  void SetLines(const std::vector<std::string>& lines) { m_lines = lines; }
  // XXX: Add to recursive layer
  virtual bool GetLines(std::vector<std::string>* lines, const bool remove_comments = true) const;
  virtual bool HasLines() const { return m_lines.size() > 0; }
  const std::string& GetName() const { return m_name; }
  const SectionValueMap& GetValues() const { return m_values; }
protected:
  LayerType m_layer;
  System m_system;
  const std::string m_name;
  static const std::string& NULL_STRING;

  SectionValueMap m_values;

  std::vector<std::string> m_lines;
};

// XXX: Allow easy migration!
class ConfigLayerLoader
{
public:
  ConfigLayerLoader(LayerType layer) : m_layer(layer) {}
  ~ConfigLayerLoader() {}
  virtual void Load(Layer* config_layer) = 0;
  virtual void Save(Layer* config_layer) = 0;

  LayerType GetLayer() const { return m_layer; }
private:
  const LayerType m_layer;
};

class Layer
{
public:
  Layer(LayerType layer) : m_layer(layer) {}
  Layer(std::unique_ptr<ConfigLayerLoader> loader);
  virtual ~Layer();

  // Convenience functions
  bool Exists(System system, const std::string& petal_name, const std::string& key);
  bool DeleteKey(System system, const std::string& petal_name, const std::string& key);
  template <typename T>
  bool GetIfExists(System system, const std::string& petal_name, const std::string& key, T* value)
  {
    if (Exists(system, petal_name, key))
      return GetOrCreateSection(system, petal_name)->Get(key, value);

    return false;
  }

  virtual Section* GetSection(System system, const std::string& petal_name);
  virtual Section* GetOrCreateSection(System system, const std::string& petal_name);

  // Explicit load and save of layers
  void Load();
  void Save();

  LayerType GetLayer() const { return m_layer; }
  const LayerMap& GetLayerMap() { return m_petals; }
  // Stay away from this routine as much as possible
  ConfigLayerLoader* GetLoader() { return m_loader.get(); }
protected:
  LayerMap m_petals;
  const LayerType m_layer;
  std::unique_ptr<ConfigLayerLoader> m_loader;
};

// Common function used for getting configuration
Section* GetOrCreateSection(System system, const std::string& petal_name);

// Layer management
OnionBloom* GetFullBloom();
void AddLayer(Layer* layer);
void AddLayer(ConfigLayerLoader* loader);
void AddLoadLayer(Layer* layer);
void AddLoadLayer(ConfigLayerLoader* loader);
Layer* GetLayer(LayerType layer);
void RemoveLayer(LayerType layer);
bool LayerExists(LayerType layer);

void AddConfigChangedCallback(CallbackFunction func, void* user_data);

// Explicit load and save of layers
void Load();
void Save();

void Init();
void Shutdown();

const std::string& GetSystemName(System system);
System GetSystemFromName(const std::string& system);
const std::string& GetLayerName(LayerType layer);
}
