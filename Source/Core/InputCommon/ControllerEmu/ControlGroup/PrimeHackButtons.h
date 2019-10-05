// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"

namespace ControllerEmu
{
class PrimeHackButtons : public ControlGroup
{
public:
  explicit PrimeHackButtons(const std::string& name_);
  PrimeHackButtons(const std::string& ini_name, const std::string& group_name);

  void AddInput(std::string button_name, bool toggle = false);

  const std::vector<bool>& isSettingToggled() const;

private:
  
};
}  // namespace ControllerEmu
