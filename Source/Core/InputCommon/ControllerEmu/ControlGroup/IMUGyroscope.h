// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <string>

#include "Common/Matrix.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
class IMUGyroscope : public ControlGroup
{
public:
  using StateData = Common::Vec3;

  IMUGyroscope(std::string name, std::string ui_name);

  StateData GetRawState() const;
  std::optional<StateData> GetState() const;

  // Value is in rad/s.
  ControlState GetDeadzone() const;

private:
  SettingValue<double> m_deadzone_setting;
};
}  // namespace ControllerEmu
