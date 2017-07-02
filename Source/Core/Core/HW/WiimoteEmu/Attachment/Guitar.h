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
class Triggers;
class Slider;
}

namespace WiimoteEmu
{
enum class GuitarGroup;
struct ExtensionReg;

class Guitar : public Attachment
{
public:
  explicit Guitar(ExtensionReg& reg);
  void GetState(u8* const data) override;
  bool IsButtonPressed() const override;

  ControllerEmu::ControlGroup* GetGroup(GuitarGroup group);

  enum
  {
    BUTTON_PLUS = 0x04,
    BUTTON_MINUS = 0x10,
    BAR_DOWN = 0x40,

    BAR_UP = 0x0100,
    FRET_YELLOW = 0x0800,
    FRET_GREEN = 0x1000,
    FRET_BLUE = 0x2000,
    FRET_RED = 0x4000,
    FRET_ORANGE = 0x8000,
  };

private:
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::Buttons* m_frets;
  ControllerEmu::Buttons* m_strum;
  ControllerEmu::Triggers* m_whammy;
  ControllerEmu::AnalogStick* m_stick;
  ControllerEmu::Slider* m_slider_bar;
};
}
