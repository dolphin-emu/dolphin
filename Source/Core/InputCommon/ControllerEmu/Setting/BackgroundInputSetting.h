// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "InputCommon/ControllerEmu/Setting/BooleanSetting.h"

namespace ControllerEmu
{
class BackgroundInputSetting : public BooleanSetting
{
public:
  BackgroundInputSetting(const std::string& setting_name);

  bool GetValue() const override;
  void SetValue(bool value) override;
};

}  // namespace ControllerEmu
