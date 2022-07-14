// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/BitFieldView.h"
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

    u8 _data1;

    BFVIEW_IN(_data1, u8, 0, 3, pressure2);
    BFVIEW_IN(_data1, u8, 3, 5, status);
  };

  static_assert(6 == sizeof(DataFormat), "Wrong size.");

private:
  ControllerEmu::AnalogStick* m_stylus;
  ControllerEmu::Triggers* m_touch;
};
}  // namespace WiimoteEmu
