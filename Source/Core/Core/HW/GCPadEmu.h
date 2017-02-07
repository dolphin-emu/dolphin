// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "InputCommon/ControllerEmu.h"

class ControlGroup;

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

class GCPad : public ControllerEmu
{
public:
  explicit GCPad(unsigned int index);
  GCPadStatus GetInput() const;
  void SetOutput(const ControlState strength);

  bool GetMicButton() const;

  std::string GetName() const override;

  ControlGroup* GetGroup(PadGroup group);

  void LoadDefaults(const ControllerInterface& ciface) override;

private:
  Buttons* m_buttons;
  AnalogStick* m_main_stick;
  AnalogStick* m_c_stick;
  Buttons* m_dpad;
  MixedTriggers* m_triggers;
  ControlGroup* m_rumble;
  Buttons* m_mic;
  ControlGroup* m_options;

  const unsigned int m_index;

  // Default analog stick radius for GameCube controllers.
  static constexpr ControlState DEFAULT_PAD_STICK_RADIUS = 1.0;
};
