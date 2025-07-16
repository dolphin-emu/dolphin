// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "Core/HW/EXI/EXI_Device.h"

namespace Core
{
class System;
}

namespace ExpansionInterface
{
void GenerateInterrupt(int flag);

class CEXIBaseboard : public IEXIDevice
{
public:
  explicit CEXIBaseboard(Core::System& system);
  virtual ~CEXIBaseboard();

  void SetCS(int cs) override;
  bool IsInterruptSet() override;
  bool IsPresent() const override;
  void DoState(PointerWrap& p) override;
  void DMAWrite(u32 addr, u32 size) override;
  void DMARead(u32 addr, u32 size) override;

private:
  enum Command
  {
    BackupOffsetSet = 0x01,
    BackupWrite = 0x02,
    BackupRead = 0x03,

    DMAOffsetLengthSet = 0x05,

    ReadISR = 0x82,
    WriteISR = 0x83,
    ReadIMR = 0x86,
    WriteIMR = 0x87,

    WriteLANCNT = 0xFF,
  };

  u32 m_position;
  u32 m_backup_dma_offset;
  u32 m_backup_dma_length;
  u8 m_command[4];
  u16 m_backup_offset;
  File::IOFile m_backup;

protected:
  void TransferByte(u8& _uByte) override;
};
}  // namespace ExpansionInterface
