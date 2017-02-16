// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "Core/HW/WiimoteEmu/Attachment/Attachment.h"

namespace ControllerEmu
{
class AnalogStick;
class Buttons;
class ControlGroup;
class Force;
class Tilt;
}

namespace WiimoteEmu
{
enum class NunchukGroup;
struct ExtensionReg;

class Nunchuk : public Attachment
{
public:
  explicit Nunchuk(ExtensionReg& reg, int index);

  void GetState(u8* const data) override;
  bool IsButtonPressed() const override;

  ControllerEmu::ControlGroup* GetGroup(NunchukGroup group);

  enum
  {
    BUTTON_C = 0x02,
    BUTTON_Z = 0x01,
  };

  enum
  {
    ACCEL_ZERO_G = 0x80,
    ACCEL_ONE_G = 0xB3,
    ACCEL_RANGE = (ACCEL_ONE_G - ACCEL_ZERO_G),
  };

  enum
  {
    STICK_CENTER = 0x80,
    STICK_RADIUS = 0x7F,
  };

  void LoadDefaults(const ControllerInterface& ciface) override;

private:
  ControllerEmu::Tilt* m_tilt;
  ControllerEmu::Force* m_swing;

  ControllerEmu::Buttons* m_shake;

  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::AnalogStick* m_stick;

  std::array<u8, 3> m_shake_step{};

  int m_index;
};
}
