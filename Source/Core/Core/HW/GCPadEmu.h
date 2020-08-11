// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackModes.h"

struct GCPadStatus;

namespace ControllerEmu
{
class AnalogStick;
class Buttons;
class MixedTriggers;
class PrimeHackModes;
}  // namespace ControllerEmu

enum class PadGroup
{
  Buttons,
  MainStick,
  CStick,
  DPad,
  Triggers,
  Rumble,
  Mic,
  Options,

  Beams,
  Visors,
  Camera,
  Misc,
  ControlStick,
  Modes
};

class GCPad : public ControllerEmu::EmulatedController
{
public:
  explicit GCPad(unsigned int index);
  GCPadStatus GetInput() const;
  void SetOutput(const ControlState strength);

  bool GetMicButton() const;

  std::string GetName() const override;

  ControllerEmu::ControlGroup* GetGroup(PadGroup group);

  void LoadDefaults(const ControllerInterface& ciface) override;

  bool CheckSpringBallCtrl();
  bool PrimeControllerMode();

  void SetPrimeMode(bool controller);

  std::tuple<double, double> GetPrimeStickXY();

  std::tuple<double, double, double, bool, bool> GetPrimeSettings();

  static const u8 MAIN_STICK_GATE_RADIUS = 87;
  static const u8 C_STICK_GATE_RADIUS = 74;

private:
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::AnalogStick* m_main_stick;
  ControllerEmu::AnalogStick* m_c_stick;
  ControllerEmu::Buttons* m_dpad;
  ControllerEmu::MixedTriggers* m_triggers;
  ControllerEmu::ControlGroup* m_rumble;
  ControllerEmu::Buttons* m_mic;
  ControllerEmu::ControlGroup* m_options;

  ControllerEmu::SettingValue<bool> m_always_connected_setting;

  ControllerEmu::ControlGroup* m_primehack_camera;
  ControllerEmu::ControlGroup* m_primehack_misc;
  ControllerEmu::AnalogStick* m_primehack_stick;
  ControllerEmu::PrimeHackModes* m_primehack_modes;

  ControllerEmu::SettingValue<bool> m_primehack_camera_sensitivity;
  ControllerEmu::SettingValue<bool> m_primehack_horizontal_sensitivity;
  ControllerEmu::SettingValue<bool> m_primehack_vertical_sensitivity;

  ControllerEmu::SettingValue<int> m_primehack_fieldofview;
  ControllerEmu::SettingValue<bool> m_primehack_invert_y;
  ControllerEmu::SettingValue<bool> m_primehack_invert_x;

  const unsigned int m_index;
};
