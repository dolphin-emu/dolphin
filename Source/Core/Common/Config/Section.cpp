// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <map>
#include <memory>

#include "Common/Assert.h"
#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "Common/Config/Section.h"
#include "Common/StringUtil.h"

namespace Config
{
const std::string& Section::NULL_STRING = "";

Section::Section(LayerType layer, System system, const std::string& name)
    : m_layer(layer), m_system(system), m_name(name)
{
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

  m_deleted_keys.push_back(key);
  m_dirty = true;
  return true;
}

void Section::Set(const std::string& key, const std::string& value)
{
  auto it = m_values.find(key);
  if (it != m_values.end() && it->second != value)
  {
    it->second = value;
    m_dirty = true;
  }
  else if (it == m_values.end())
  {
    m_values[key] = value;
    m_dirty = true;
  }
}

void Section::Set(const std::string& key, u16 newValue)
{
  Section::Set(key, StringFromFormat("0x%04x", newValue));
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
  Section::Set(key, std::to_string(newValue));
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

void Section::SetLines(const std::vector<std::string>& lines)
{
  m_lines = lines;
  m_dirty = true;
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

bool Section::Get(const std::string& key, u16* value, u16 defaultValue) const
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

bool Section::HasLines() const
{
  return !m_lines.empty();
}

const std::string& Section::GetName() const
{
  return m_name;
}

const SectionValueMap& Section::GetValues() const
{
  return m_values;
}

const std::vector<std::string>& Section::GetDeletedKeys() const
{
  return m_deleted_keys;
}

bool Section::IsDirty() const
{
  return m_dirty;
}
void Section::ClearDirty()
{
  m_dirty = false;
}

RecursiveSection::RecursiveSection(LayerType layer, System system, const std::string& name)
    : Section(layer, system, name)
{
}

bool RecursiveSection::Exists(const std::string& key) const
{
  auto layers_it = Config::GetLayers()->find(LayerType::Meta);
  do
  {
    const Section* layer_section = layers_it->second->GetSection(m_system, m_name);
    if (layer_section && layer_section->Exists(key))
    {
      return true;
    }
  } while (--layers_it != Config::GetLayers()->end());

  return false;
}

bool RecursiveSection::Get(const std::string& key, std::string* value,
                           const std::string& default_value) const
{
  for (auto layer_id : SEARCH_ORDER)
  {
    auto layers_it = Config::GetLayers()->find(layer_id);
    if (layers_it == Config::GetLayers()->end())
      continue;

    const Section* layer_section = layers_it->second->GetSection(m_system, m_name);
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
}
