// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/Triforce/Touchscreen.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "Common/BitUtils.h"
#include "Common/Logging/Log.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"
#include "InputCommon/ControllerEmu/ControlGroup/AnalogStick.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/GCPadStatus.h"

namespace
{
#pragma pack(push, 1)
// This is the "SmartSet Data Protocol".
struct SmartSetDataPacket
{
  u8 lead_in = 0x55;
  u8 cmd = 0x54;     // Always 0x54.
  u8 status = 0xff;  // Seems to be ignored by the game.
  u16 x{};           // Little endian (0-4095).
  u16 y{};           // Little endian (0-4095).
  u8 pressure{};
  u8 unused{};
  u8 checksum{};  // All previous bytes + 0xaa.
};
#pragma pack(pop)
static_assert(sizeof(SmartSetDataPacket) == 10);

u16 GetTouchAxisValue(ControlState axis_state, u8 center_value)
{
  const u8 axis = ControllerEmu::MapFloat<u8>(std::clamp(axis_state, -1.0, 1.0), center_value);
  return Common::ExpandValue(u16(axis), 4);
}

// The physical stick still moves in a circle. Expand that circular range before
// encoding the touchscreen packet so the whole touch surface is reachable.
Common::DVec2 ExpandCircleToSquare(ControlState x, ControlState y)
{
  const ControlState max_axis = std::max(std::abs(x), std::abs(y));
  if (max_axis == 0.0)
    return {x, y};

  const ControlState radius = std::hypot(x, y);
  const ControlState scale = radius / max_axis;
  return {std::clamp(x * scale, -1.0, 1.0), std::clamp(y * scale, -1.0, 1.0)};
}
}  // namespace

namespace Triforce
{

void Touchscreen::Update()
{
  if (const auto input = GetRxByteSpan(); !input.empty())
  {
    // The Key of Avalon doesn't write to the device, it only reads.
    WARN_LOG_FMT(SERIALINTERFACE_AMBB, "Unexpected write of {} bytes to touchscreen.",
                 input.size());
    ConsumeRxBytes(input.size());
  }

  // Our touchscreen conveniently produces exactly one packet every Update cycle.
  // I'm guessing the real hardware doesn't produce ~60hz input perfectly in-sync with SI updates,
  //  but Avalon doesn't seem to mind.

  const auto pad_status = Pad::GetStatus(0);
  const auto* const c_stick =
      static_cast<ControllerEmu::AnalogStick*>(Pad::GetGroup(0, PadGroup::CStick));
  const auto c_stick_state = c_stick->GetReshapableState(false);
  const auto touch_state = ExpandCircleToSquare(c_stick_state.x, c_stick_state.y);

  const u16 x = GetTouchAxisValue(touch_state.x, GCPadStatus::C_STICK_CENTER_X);
  const u16 y = GetTouchAxisValue(touch_state.y, GCPadStatus::C_STICK_CENTER_Y);

  // For reference, the game does something like this to scale the values from 0-4095.
  // Note the offsets of 4.
  // Someone who cares more might want to compensate for that.
  //
  // x = s32(0.15625f * x) + 4;
  // y = s32(480.f - (0.1171875f * y)) + 4;

  SmartSetDataPacket packet{
      .x = x,
      .y = y,
      .pressure = pad_status.triggerRight,
  };

  packet.checksum = std::accumulate(&packet.lead_in, &packet.checksum, u8{0xaa});

  WriteTxBytes(Common::AsU8Span(packet));
}

}  // namespace Triforce
