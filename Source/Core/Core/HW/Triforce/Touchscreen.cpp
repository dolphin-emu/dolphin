// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/Triforce/Touchscreen.h"

#include <numeric>

#include "Common/BitUtils.h"
#include "Common/Logging/Log.h"
#include "Core/HW/GCPad.h"
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

  // We currently feed the touch screen from c-stick and right-trigger just to make it usable.
  // TODO: Expose it in a better way.

  const auto pad_status = Pad::GetStatus(0);

  // For reference, the game does something like this to scale the values from 0-4095.
  // Note the offsets of 4.
  // Someone who cares more might want to compensate for that.
  //
  // x = s32(0.15625f * x) + 4;
  // y = s32(480.f - (0.1171875f * y)) + 4;

  SmartSetDataPacket packet{
      .x = Common::ExpandValue(u16(pad_status.substickX), 4),
      .y = Common::ExpandValue(u16(pad_status.substickY), 4),
      .pressure = pad_status.triggerRight,
  };

  packet.checksum = std::accumulate(&packet.lead_in, &packet.checksum, u8{0xaa});

  WriteTxBytes(Common::AsU8Span(packet));
}

}  // namespace Triforce
