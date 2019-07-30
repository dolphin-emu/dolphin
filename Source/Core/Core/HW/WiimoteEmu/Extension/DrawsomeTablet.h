// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/BitField.h"
#include "Common/Swap.h"
#include "Core/HW/WiimoteEmu/Extension/Extension.h"

namespace ControllerEmu
{
class AnalogStick;
class Triggers;
class ControlGroup;
}  // namespace ControllerEmu

namespace WiimoteEmu
{
enum class DrawsomeTabletGroup
{
  Stylus,
  Touch,
};

class DrawsomeTablet : public Extension3rdParty
{
public:
  DrawsomeTablet();

  void Update() override;
  bool IsButtonPressed() const override;
  void Reset() override;

  ControllerEmu::ControlGroup* GetGroup(DrawsomeTabletGroup group);

  struct DataFormat
  {
    // Pen X/Y is little endian.
    u8 stylus_x1;
    u8 stylus_x2;

    u8 stylus_y1;
    u8 stylus_y2;

    u8 pressure1;

    union
    {
      BitField<0, 3, u8> pressure2;
      BitField<3, 5, u8> status;
    };
  };

  static_assert(6 == sizeof(DataFormat), "Wrong size.");

private:
  ControllerEmu::AnalogStick* m_stylus;
  ControllerEmu::Triggers* m_touch;
};
}  // namespace WiimoteEmu
