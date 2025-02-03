// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <optional>

#include "Core/HW/WiimoteCommon/WiimoteReport.h"
#include "Core/HW/WiimoteEmu/Camera.h"
#include "Core/HW/WiimoteEmu/Extension/DesiredExtensionState.h"
#include "Core/HW/WiimoteEmu/MotionPlus.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

namespace WiimoteEmu
{
struct DesiredWiimoteState
{
  // 1g in Z direction, which is the default returned by an unmoving emulated Wiimote.
  static constexpr WiimoteCommon::AccelData DEFAULT_ACCELERATION = WiimoteCommon::AccelData(
      {Wiimote::ACCEL_ZERO_G << 2, Wiimote::ACCEL_ZERO_G << 2, Wiimote::ACCEL_ONE_G << 2});

  // No light detected by the IR camera.
  static constexpr std::array<CameraPoint, 4> DEFAULT_CAMERA = {CameraPoint(), CameraPoint(),
                                                                CameraPoint(), CameraPoint()};

  WiimoteCommon::ButtonData buttons{};  // non-button state in this is ignored
  WiimoteCommon::AccelData acceleration = DEFAULT_ACCELERATION;
  std::array<CameraPoint, 4> camera_points = DEFAULT_CAMERA;
  std::optional<MotionPlus::DataFormat::Data> motion_plus = std::nullopt;
  DesiredExtensionState extension;
};

// For Netplay.
struct SerializedWiimoteState
{
  u8 length;
  std::array<u8, 36> data;  // 18 bytes Wiimote, 6 bytes MotionPlus, 12 bytes Extension
};

SerializedWiimoteState SerializeDesiredState(const DesiredWiimoteState& state);
bool DeserializeDesiredState(DesiredWiimoteState* state, const SerializedWiimoteState& serialized);
}  // namespace WiimoteEmu
