// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/WiimoteCommon/WiimoteReport.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

namespace WiimoteEmu
{
struct DesiredWiimoteState
{
  // 1g in Z direction, which is the default returned by an unmoving emulated Wiimote.
  static constexpr WiimoteCommon::AccelData DEFAULT_ACCELERATION = WiimoteCommon::AccelData(
      {Wiimote::ACCEL_ZERO_G << 2, Wiimote::ACCEL_ZERO_G << 2, Wiimote::ACCEL_ONE_G << 2});

  WiimoteCommon::ButtonData buttons{};  // non-button state in this is ignored
  WiimoteCommon::AccelData acceleration = DEFAULT_ACCELERATION;
};
}  // namespace WiimoteEmu
