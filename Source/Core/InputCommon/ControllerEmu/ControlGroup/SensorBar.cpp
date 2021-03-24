// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/SensorBar.h"

#include <memory>
#include <string>

#include "Common/Common.h"
#include "Common/MathUtil.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Control/Input.h"

namespace ControllerEmu
{
SensorBar::SensorBar(std::string name_, std::string ui_name_)
    : ControlGroup(std::move(name_), std::move(ui_name_), GroupType::SensorBar,
#ifdef ANDROID
          // Enabling this on Android devices which have an accelerometer and gyroscope prevents
          // touch controls from being used for pointing, and touch controls generally work better
          ControlGroup::DefaultValue::Disabled)
#else
          ControlGroup::DefaultValue::Enabled)
#endif
{
  AddSetting(
      &m_ir_camera_displacement_x_setting,
      // i18n: Refers to the positional offset of an emulated wiimote's IR camera compared to the
      // sensor bar.
      {_trans("IR Camera X"),
       // i18n: The symbol for meters.
       _trans("m"),
       // i18n: Refers to the emulated Wii remote.
       _trans(
           "How far the Wiimote's IR camera is to the right of the sensor bar's center.")},
      0, -10, 10);
  AddSetting(
      &m_ir_camera_displacement_y_setting,
      // i18n: Refers to the positional offset of an emulated wiimote's IR camera compared to the
      // sensor bar.
      {_trans("IR Camera Y"),
       // i18n: The symbol for meters.
       _trans("m"),
       // i18n: Refers to the emulated Wii remote.
       _trans(
           "How far the Wiimote's IR camera is back from the sensor bar's center.")},
      0, -10, 10);
  AddSetting(
      &m_ir_camera_displacement_z_setting,
      // i18n: Refers to the positional offset of an emulated wiimote's IR camera compared to the
      // sensor bar.
      {_trans("IR Camera Z"),
       // i18n: The symbol for meters.
       _trans("m"),
       // i18n: Refers to the emulated Wii remote.
       _trans("How far the Wiimote's IR camera is above the sensor bar's center.")},
      0, -10, 10);
  AddSetting(&m_ir_camera_pitch_setting,
             // i18n: Refers to the orientation of an emulated wiimote's IR camera.
             {_trans("IR Camera Pitch"),
              // i18n: The degrees symbol.
              _trans("°"),
              // i18n: Refers to the method used to pass position and orientation data to Dolphin.
              _trans("The pitch of the Wiimote's IR camera.")},
             0, -360, 360);
  AddSetting(&m_ir_camera_roll_setting,
             // i18n: Refers to the orientation of an emulated wiimote's IR camera
             {_trans("IR Camera Roll"),
              // i18n: The degrees symbol.
              _trans("°"),
              // i18n: Refers to the method used to pass position and orientation data to Dolphin.
              _trans("The roll of the Wiimote's IR camera.")},
             0, -360, 360);
  AddSetting(&m_ir_camera_yaw_setting,
             // i18n: Refers to the orientation of an emulated wiimote's IR camera.
             {_trans("IR Camera Yaw"),
              // i18n: The degrees symbol.
              _trans("°"),
              // i18n: Refers to the method used to pass position and orientation data to Dolphin.
              _trans("The yaw of the Wiimote's IR camera.")},
             0, -360, 360);
  AddSetting(&m_input_confidence,
             // i18n: Refers to how much confidence there is in the camera position and orientation being correct.
             {_trans("Input Confidence"),
              // i18n: The symbol for percent.
              _trans("%"),
              // i18n: Refers to the method used to pass position and orientation data to Dolphin.
              _trans("How confident Dolphin should be that the above data is accurate.")},
             100, 0, 100);
}

Common::Vec3 SensorBar::GetIRCameraDisplacement()
{
  return Common::Vec3(m_ir_camera_displacement_x_setting.GetValue(),
                      m_ir_camera_displacement_y_setting.GetValue(),
                      m_ir_camera_displacement_z_setting.GetValue());
}

Common::Vec3 SensorBar::GetIRCameraOrientation()
{
  return Common::Vec3(m_ir_camera_pitch_setting.GetValue() * MathUtil::TAU / 360,
                      m_ir_camera_roll_setting.GetValue() * MathUtil::TAU / 360,
                      m_ir_camera_yaw_setting.GetValue() * MathUtil::TAU / 360);
}

double SensorBar::GetInputConfidence()
{
  return m_input_confidence.GetValue();
}

}  // namespace ControllerEmu
