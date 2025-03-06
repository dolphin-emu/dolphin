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
  // No light detected by the IR camera.
  static constexpr std::array<CameraPoint, 4> DEFAULT_CAMERA = {CameraPoint(), CameraPoint(),
                                                                CameraPoint(), CameraPoint()};

  WiimoteCommon::ButtonData buttons{};  // non-button state in this is ignored
  std::optional<WiimoteCommon::AccelData> acceleration = std::nullopt;
  std::array<CameraPoint, 4> camera_points = DEFAULT_CAMERA;
  std::optional<MotionPlus::DataFormat::Data> motion_plus = std::nullopt;
  DesiredExtensionState extension;
};

// For Netplay.
struct SerializedWiimoteState
{
  u8 length;

  // 18 bytes Wiimote + ((6 bytes MotionPlus + 6 bytes Extension) | 8 bytes BalanceBoardExt)
  static constexpr size_t MAX_EXT_DATA_SIZE = std::max(6, 8);
  std::array<u8, 18 + std::max(6 + 6, 8)> data;
};

SerializedWiimoteState SerializeDesiredState(const DesiredWiimoteState& state);
bool DeserializeDesiredState(DesiredWiimoteState* state, const SerializedWiimoteState& serialized);
}  // namespace WiimoteEmu
