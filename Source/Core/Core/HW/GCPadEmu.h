// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "Common/Common.h"

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

struct GCPadStatus;

namespace ControllerEmu
{
class AnalogStick;
class Buttons;
class MixedTriggers;
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
  Options
};

class GCPad : public ControllerEmu::EmulatedController
{
public:
  explicit GCPad(unsigned int index);
  GCPadStatus GetInput() const;
  void SetOutput(const ControlState strength);

  bool GetMicButton() const;

  std::string GetName() const override;

  InputConfig* GetConfig() const override;

  ControllerEmu::ControlGroup* GetGroup(PadGroup group);

  void LoadDefaults(const ControllerInterface& ciface) override;

  // Values averaged from multiple genuine GameCube controllers.
  static constexpr ControlState MAIN_STICK_GATE_RADIUS = 0.7937125;
  static constexpr ControlState C_STICK_GATE_RADIUS = 0.7221375;

  static constexpr const char* BUTTONS_GROUP = _trans("Buttons");
  static constexpr const char* MAIN_STICK_GROUP = "Main Stick";
  static constexpr const char* C_STICK_GROUP = "C-Stick";
  static constexpr const char* DPAD_GROUP = _trans("D-Pad");
  static constexpr const char* TRIGGERS_GROUP = _trans("Triggers");
  static constexpr const char* RUMBLE_GROUP = _trans("Rumble");
  static constexpr const char* MIC_GROUP = _trans("Microphone");
  static constexpr const char* OPTIONS_GROUP = _trans("Options");

  static constexpr const char* A_BUTTON = "A";
  static constexpr const char* B_BUTTON = "B";
  static constexpr const char* X_BUTTON = "X";
  static constexpr const char* Y_BUTTON = "Y";
  static constexpr const char* Z_BUTTON = "Z";
  static constexpr const char* START_BUTTON = "Start";

  // i18n: The left trigger button (labeled L on real controllers)
  static constexpr const char* L_DIGITAL = _trans("L");
  // i18n: The right trigger button (labeled R on real controllers)
  static constexpr const char* R_DIGITAL = _trans("R");
  // i18n: The left trigger button (labeled L on real controllers) used as an analog input
  static constexpr const char* L_ANALOG = _trans("L-Analog");
  // i18n: The right trigger button (labeled R on real controllers) used as an analog input
  static constexpr const char* R_ANALOG = _trans("R-Analog");

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

  const unsigned int m_index;
};
