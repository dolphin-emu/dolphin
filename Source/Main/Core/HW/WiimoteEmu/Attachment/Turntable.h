// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/WiimoteEmu/Attachment/Attachment.h"

namespace ControllerEmu
{
class AnalogStick;
class Buttons;
class ControlGroup;
class Slider;
class Triggers;
}

namespace WiimoteEmu
{
enum class TurntableGroup;
struct ExtensionReg;

class Turntable : public Attachment
{
public:
  explicit Turntable(ExtensionReg& reg);
  void GetState(u8* const data) override;
  bool IsButtonPressed() const override;

  ControllerEmu::ControlGroup* GetGroup(TurntableGroup group);

  enum
  {
    BUTTON_EUPHORIA = 0x1000,

    BUTTON_L_GREEN = 0x0800,
    BUTTON_L_RED = 0x20,
    BUTTON_L_BLUE = 0x8000,

    BUTTON_R_GREEN = 0x2000,
    BUTTON_R_RED = 0x02,
    BUTTON_R_BLUE = 0x0400,

    BUTTON_MINUS = 0x10,
    BUTTON_PLUS = 0x04,
  };

private:
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::AnalogStick* m_stick;
  ControllerEmu::Triggers* m_effect_dial;
  ControllerEmu::Slider* m_left_table;
  ControllerEmu::Slider* m_right_table;
  ControllerEmu::Slider* m_crossfade;
};
}
