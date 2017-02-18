// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// Utils for loading and storing settings without duplicating the setting keys everywhere.

#include <wx/control.h>

#include <map>
#include <string>

#include "Common/Config.h"

class wxCheckBox;
class wxWindow;

namespace ConfigUtils
{
struct Setting
{
  Config::System system;
  std::string section;
  std::string key;
};

using SettingsMap = std::map<wxWindow*, Setting>;

template <typename T>
T Get(const SettingsMap& map, wxControl* control)
{
  const auto base_layer = Config::GetLayer(Config::LayerType::Base);
  const auto setting = map.at(control);

  T value;
  base_layer->GetIfExists(setting.system, setting.section, setting.key, &value);
  return value;
}

template <typename T>
void Set(const SettingsMap& map, wxControl* control, const T& value)
{
  const auto base_layer = Config::GetLayer(Config::LayerType::Base);
  const auto setting = map.at(control);

  auto section = base_layer->GetOrCreateSection(setting.system, setting.section);
  section->Set(setting.key, value);
}

// Convenience wrappers for common widgets
void LoadValue(const SettingsMap& map, wxCheckBox* control);
void SaveOnChange(const SettingsMap& map, wxCheckBox* checkbox);
}  // namespace ConfigUtils
