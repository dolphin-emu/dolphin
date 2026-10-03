// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/DirectIOFile.h"

#include "Core/HW/Triforce/ICCardReader.h"
#include "Core/HW/Triforce/SerialDevice.h"

class PointerWrap;

namespace Triforce
{

// Serial deck reader used by The Key of Avalon games.
class DeckReader final : public SerialDevice
{
public:
  void Update() override;

  void DoState(PointerWrap& p) override;

  auto* GetICCardReader() { return &m_ic_card_reader; }

private:
  // It seems that the IC Card Reader must be connected through the Deck Reader.
  // The Deck Reader forwards appropriate commands to the IC Card Reader.
  // IC Card Reader responses are passed through back to the baseboard as-is.
  // It can't be the other way around because FirmwareUpdate sends many raw bytes.
  // Unless FirmwareUpdate temporarily cuts off the IC Card Reader ?
  ICCardReader m_ic_card_reader{0};

  void HandleFirmwareUpdate();

  u8 m_firmware_update_timeout = 0;

  File::DirectIOFile m_firmware_dump_file;
};

}  // namespace Triforce
