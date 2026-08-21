// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "Core/HW/EXI/EXI_Device.h"

class PointerWrap;

namespace Core
{
class System;
}

namespace ExpansionInterface
{
// SD card adapter on the EXI bus (SD Gecko in a memory card slot, SD2SP2 in Serial Port 2).
// These adapters are plain level shifters: the card speaks its SPI protocol directly over
// EXI, so this device implements the SD SPI-mode command set against a raw FAT image file.
// The image can be kept in sync with a host folder (see MAIN_GC_SD_CARD_ENABLE_FOLDER_SYNC).
class CEXISDCard final : public IEXIDevice
{
public:
  explicit CEXISDCard(Core::System& system);
  ~CEXISDCard() override;

  void SetCS(int cs) override;
  bool IsPresent() const override;
  void DoState(PointerWrap& p) override;

private:
  void TransferByte(u8& byte) override;

  void ProcessInputByte(u8 in);
  void ExecuteCommand();
  void ExecuteAppCommand(u8 command, u32 arg);

  u8 R1() const;
  void QueueR1();
  void QueueR1(u8 r1);
  void QueueDataBlock(const u8* data, size_t size);
  void QueueDataErrorToken(u8 token);
  bool QueueReadBlock(u64 block_address);
  void QueueNextMultiBlock();

  u64 ByteAddress(u32 arg) const;
  std::array<u8, 16> MakeCSD() const;
  std::array<u8, 16> MakeCID() const;

  File::IOFile m_image;
  u64 m_size = 0;
  bool m_sdhc = false;
  bool m_allow_writes = true;

  // SPI-mode protocol state
  bool m_idle_state = true;
  bool m_app_cmd = false;
  std::array<u8, 6> m_cmd{};
  u32 m_cmd_pos = 0;

  // Bytes the card is about to clock out; refilled per block during CMD18.
  std::vector<u8> m_response;
  size_t m_response_pos = 0;

  bool m_multi_read = false;
  u64 m_read_address = 0;

  enum class WriteState : u32
  {
    None,
    WaitToken,
    Data,
  };
  WriteState m_write_state = WriteState::None;
  bool m_multi_write = false;
  u64 m_write_address = 0;
  std::array<u8, 512 + 2> m_write_buffer{};
  u32 m_write_pos = 0;
};
}  // namespace ExpansionInterface
