// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "InputCommon/ControllerEmu.h"

class GCPad : public ControllerEmu
{
public:
  GCPad(const unsigned int index);
  GCPadStatus GetInput() const;
  void SetOutput(const ControlState strength);

  bool GetMicButton() const;

  void SetForcedInput(GCPadStatus* const pad);

  std::string GetName() const override;

  void LoadDefaults(const ControllerInterface& ciface) override;

private:
  Buttons* m_buttons;
  AnalogStick* m_main_stick;
  AnalogStick* m_c_stick;
  Buttons* m_dpad;
  MixedTriggers* m_triggers;
  ControlGroup* m_rumble;
  ControlGroup* m_options;

  GCPadStatus    m_forced_input;

  const unsigned int m_index;

  // Default analog stick radius for GameCube controllers.
  static constexpr ControlState DEFAULT_PAD_STICK_RADIUS = 1.0;
};
