// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerEmu/ControlGroup/IMUCursor.h"

#include <memory>
#include <string>

#include "Common/Common.h"
#include "Common/MathUtil.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Control/Input.h"

namespace ControllerEmu
{
IMUCursor::IMUCursor(std::string name_, std::string ui_name_)
    : ControlGroup(
          std::move(name_), std::move(ui_name_), GroupType::IMUCursor,
#ifdef ANDROID
          // Enabling this on Android devices which have an accelerometer and gyroscope prevents
          // touch controls from being used for pointing, and touch controls generally work better
          ControlGroup::DefaultValue::Disabled)
#else
          ControlGroup::DefaultValue::Enabled)
#endif
{
  AddInput(Translate, _trans("Recenter"));

  // Default values chosen to reach screen edges in most games including the Wii Menu.

  AddSetting(&m_yaw_setting,
             // i18n: Refers to an amount of rotational movement about the "yaw" axis.
             {_trans("Total Yaw"),
              // i18n: The symbol/abbreviation for degrees (unit of angular measure).
              _trans("°"),
              // i18n: Refers to emulated wii remote movements.
              _trans("Clamping of rotation about the yaw axis.")},
             25, 0, 360);

  AddSetting(&m_accel_weight_setting,
             {// i18n: Percentage value of accelerometer data (complementary filter coefficient).
              _trans("Accelerometer Influence"),
              // i18n: The symbol/abbreviation for percent.
              _trans("%"),
              // i18n: Refers to a setting controling the influence of accelerometer data.
              _trans("Influence of accelerometer data on pitch and roll. Higher values will reduce "
                     "drift at the cost of noise. Consider values between 1% and 3%.")},
             2, 0, 100);
}

ControlState IMUCursor::GetTotalYaw() const
{
  return m_yaw_setting.GetValue() * MathUtil::TAU / 360;
}

ControlState IMUCursor::GetAccelWeight() const
{
  return m_accel_weight_setting.GetValue() / 100;
}

}  // namespace ControllerEmu
