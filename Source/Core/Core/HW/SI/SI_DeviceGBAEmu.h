// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Common/CommonTypes.h"
#include "Core/HW/SI/SI_Device.h"

namespace HW::GBA
{
class Core;
}  // namespace HW::GBA

class GBAHostInterface;

namespace SerialInterface
{
class CSIDevice_GBAEmu final : public ISIDevice
{
public:
  CSIDevice_GBAEmu(Core::System& system, SIDevices device, int device_number);
  ~CSIDevice_GBAEmu();

  int RunBuffer(u8* buffer, int request_length) override;
  int TransferInterval() override;
  bool GetData(u32& hi, u32& low) override;
  void SendCommand(u32 command, u8 poll) override;
  void DoState(PointerWrap& p) override;
  void OnEvent(u64 userdata, s64 cycles_late) override;

private:
  enum class NextAction
  {
    SendCommand,
    WaitTransferTime,
    ReceiveResponse
  };

  NextAction m_next_action = NextAction::SendCommand;
  EBufferCommands m_last_cmd{};
  u64 m_timestamp_sent = 0;
  u16 m_keys = 0;

  std::shared_ptr<HW::GBA::Core> m_core;
  std::shared_ptr<GBAHostInterface> m_gbahost;
};
}  // namespace SerialInterface
