// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"

namespace ControllerEmu
{
class ModifySettingsButton : public Buttons
{
public:
  explicit ModifySettingsButton(std::string button_name);

  void AddInput(std::string button_name, bool toggle = false);

  void GetState();

  const std::vector<bool>& isSettingToggled() const;
  const std::vector<bool>& getSettingsModifier() const;

private:
  std::vector<bool> threshold_exceeded;  // internal calculation (if "state" was above threshold)
  std::vector<bool> associated_settings_toggle;  // is setting toggled or hold?
  std::vector<bool> associated_settings;         // result
};
}  // namespace ControllerEmu
