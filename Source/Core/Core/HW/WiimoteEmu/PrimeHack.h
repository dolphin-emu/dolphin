// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "Core/HW/WiimoteCommon/WiimoteReport.h"
#include "Core/HW/WiimoteEmu/Dynamics.h"
#include "Core/HW/WiimoteEmu/Extension/Extension.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
class Buttons;
class ControlGroup;
}  // namespace ControllerEmu

namespace WiimoteEmu
{
enum class PrimeHackGroup
{
  Beams,
  Visors,
  Camera,
  Misc
};

class PrimeHack : public ControllerEmu::EmulatedController
{
public:
  PrimeHack();

  bool IsButtonPressed();

  ControllerEmu::ControlGroup* GetGroup(PrimeHackGroup group);

  void LoadDefaults(const ControllerInterface& ciface);

private:
  ControllerEmu::Buttons* m_beams;
  ControllerEmu::Buttons* m_visors;
  ControllerEmu::ControlGroup* m_misc;
  ControllerEmu::ControlGroup* m_camera;

  ControllerEmu::SettingValue<double> m_camera_sensitivity;
  ControllerEmu::SettingValue<double> m_cursor_sensitivity;
  ControllerEmu::SettingValue<double> m_fieldofview;
  ControllerEmu::SettingValue<bool> m_invert_y;
  ControllerEmu::SettingValue<bool> m_invert_x;
};
}  // namespace WiimoteEmu
