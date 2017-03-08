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
}

namespace WiimoteEmu
{
enum class DrumsGroup;
struct ExtensionReg;

class Drums : public Attachment
{
public:
  explicit Drums(ExtensionReg& reg);
  void GetState(u8* const data) override;
  bool IsButtonPressed() const override;

  ControllerEmu::ControlGroup* GetGroup(DrumsGroup group);

  enum
  {
    BUTTON_PLUS = 0x04,
    BUTTON_MINUS = 0x10,

    PAD_BASS = 0x0400,
    PAD_BLUE = 0x0800,
    PAD_GREEN = 0x1000,
    PAD_YELLOW = 0x2000,
    PAD_RED = 0x4000,
    PAD_ORANGE = 0x8000,
  };

private:
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::Buttons* m_pads;
  ControllerEmu::AnalogStick* m_stick;
};
}
