// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/WiimoteEmu/Extension/Extension.h"

namespace ControllerEmu
{
class Buttons;
class AnalogStick;
class Triggers;
class ControlGroup;
}  // namespace ControllerEmu

namespace WiimoteEmu
{
enum class UDrawTabletGroup
{
  Buttons,
  Stylus,
  Touch,
};

class UDrawTablet : public Extension3rdParty
{
public:
  UDrawTablet();

  void BuildDesiredExtensionState(DesiredExtensionState* target_state) override;
  void Update(const DesiredExtensionState& target_state) override;
  void Reset() override;

  ControllerEmu::ControlGroup* GetGroup(UDrawTabletGroup group);

  static constexpr u8 BUTTON_ROCKER_UP = 0x1;
  static constexpr u8 BUTTON_ROCKER_DOWN = 0x2;

  struct DataFormat
  {
    // Bytes 0-2 are 0xff when stylus is lifted
    // X increases from left to right
    // Y increases from bottom to top
    u8 stylus_x1;
    u8 stylus_y1;
    u8 stylus_x2 : 4;
    u8 stylus_y2 : 4;

    // Valid even when stylus is lifted
    u8 pressure;

    // Always 0xff
    u8 unk;

    // Buttons are 0 when pressed
    // 0x04 is always unset (neutral state is 0xfb)
    u8 buttons;
  };

  static_assert(6 == sizeof(DataFormat), "Wrong size.");

private:
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::AnalogStick* m_stylus;
  ControllerEmu::Triggers* m_touch;
};
}  // namespace WiimoteEmu
