#pragma once
// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <string>

#include "Common/MathUtil.h"
#include "Common/Matrix.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

namespace ControllerEmu
{
class SensorBar : public ControlGroup
{
public:
  using StateData = Common::Vec3;

  SensorBar(std::string name, std::string ui_name);

  Common::Vec3 GetIRCameraDisplacement();
  Common::Vec3 GetIRCameraOrientation();

  SettingValue<double> m_ir_camera_displacement_x_setting;
  SettingValue<double> m_ir_camera_displacement_y_setting;
  SettingValue<double> m_ir_camera_displacement_z_setting;
  SettingValue<double> m_ir_camera_pitch_setting;
  SettingValue<double> m_ir_camera_roll_setting;
  SettingValue<double> m_ir_camera_yaw_setting;
  SettingValue<double> m_input_confidence;
};
}  // namespace ControllerEmu
