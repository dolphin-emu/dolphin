// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/IniFile.h"

namespace ControllerEmu
{
class BooleanSetting;
class Control;
class NumericSetting;

enum class GroupType
{
  Other,
  Stick,
  MixedTriggers,
  Buttons,
  Force,
  Extension,
  Tilt,
  Cursor,
  Triggers,
  Slider
};

class ControlGroup
{
public:
  explicit ControlGroup(const std::string& name, GroupType type = GroupType::Other);
  ControlGroup(const std::string& name, const std::string& ui_name,
               GroupType type = GroupType::Other);
  virtual ~ControlGroup();

  virtual void LoadConfig(IniFile::Section* sec, const std::string& defdev = "",
                          const std::string& base = "");
  virtual void SaveConfig(IniFile::Section* sec, const std::string& defdev = "",
                          const std::string& base = "");

  void SetControlExpression(int index, const std::string& expression);

  const std::string name;
  const std::string ui_name;
  const GroupType type;

  std::vector<std::unique_ptr<Control>> controls;
  std::vector<std::unique_ptr<NumericSetting>> numeric_settings;
  std::vector<std::unique_ptr<BooleanSetting>> boolean_settings;
};
}  // namespace ControllerEmu
