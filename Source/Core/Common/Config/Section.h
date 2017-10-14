// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>
#include <vector>

// XXX: Purely for case insensitive compare
#include "Common/CommonTypes.h"
#include "Common/Config/Enums.h"
#include "Common/IniFile.h"

namespace Config
{
class Layer;
class ConfigLayerLoader;

using SectionValueMap = std::map<std::string, std::string, CaseInsensitiveStringCompare>;

class Section
{
  friend Layer;
  friend ConfigLayerLoader;

public:
  Section(LayerType layer, System system, const std::string& name);
  virtual ~Section() = default;

  virtual bool Exists(const std::string& key) const;
  bool Delete(const std::string& key);

  // Setters
  virtual void Set(const std::string& key, const std::string& value);

  void Set(const std::string& key, u16 newValue);
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
  bool Get(const std::string& key, u16* value, u16 defaultValue = 0) const;
  bool Get(const std::string& key, u32* value, u32 defaultValue = 0) const;
  bool Get(const std::string& key, bool* value, bool defaultValue = false) const;
  bool Get(const std::string& key, float* value, float defaultValue = 0.0f) const;
  bool Get(const std::string& key, double* value, double defaultValue = 0.0) const;

  template <typename T>
  T Get(const std::string& key, const T& default_value) const
  {
    T value;
    Get(key, &value, default_value);
    return value;
  }

  // Section chunk
  void SetLines(const std::vector<std::string>& lines);
  // XXX: Add to recursive layer
  virtual bool GetLines(std::vector<std::string>* lines, const bool remove_comments = true) const;
  virtual bool HasLines() const;
  const std::string& GetName() const;
  const SectionValueMap& GetValues() const;
  const std::vector<std::string>& GetDeletedKeys() const;
  bool IsDirty() const;
  void ClearDirty();

protected:
  bool m_dirty = false;

  LayerType m_layer;
  System m_system;
  const std::string m_name;
  static const std::string& NULL_STRING;

  SectionValueMap m_values;
  std::vector<std::string> m_deleted_keys;

  std::vector<std::string> m_lines;
};

// Only to be used with the meta-layer
class RecursiveSection final : public Section
{
public:
  RecursiveSection(LayerType layer, System system, const std::string& name);

  bool Exists(const std::string& key) const override;

  bool Get(const std::string& key, std::string* value,
           const std::string& default_value = NULL_STRING) const override;

  void Set(const std::string& key, const std::string& value) override;
};
}
