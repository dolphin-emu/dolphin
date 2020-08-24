// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <deque>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "Core/HW/EXI/EXI_Device.h"

class PointerWrap;

namespace ExpansionInterface
{
// EXI-SD adapter (DOL-019)
class CEXISD final : public IEXIDevice
{
public:
  explicit CEXISD(Core::System& system);

  void ImmWrite(u32 data, u32 size) override;
  u32 ImmRead(u32 size) override;
  void ImmReadWrite(u32& data, u32 size) override;
  // TODO: DMA
  void SetCS(int cs) override;

  bool IsPresent() const override;
  void DoState(PointerWrap& p) override;

private:
  enum class Command
  {
    GoIdleState = 0,
    SendOpCond = 1,
    // AllSendCid = 2,        // Not SPI
    // SendRelativeAddr = 3,  // Not SPI
    // SetDSR = 4,            // Not SPI
    // Reserved for SDIO
    SwitchFunc = 6,
    // SelectCard = 7,  // or Deselect; not SPI
    SendInterfaceCond = 8,
    SendCSD = 9,
    SendCID = 10,
    // VoltageSwitch = 11,  // Not SPI
    StopTransmission = 12,
    SendStatus = 13,
    // 14 Reserved
    // GoInactiveState = 15,  // Not SPI

    SetBlockLen = 16,
    ReadSingleBlock = 17,
    ReadMultipleBlock = 18,
    SendTuningBlock = 19,
    // SpeedClassControl = 20,  // Not SPI
    // 21 Reserved
    // 22 Reserved
    SetBlockCount = 23,

    WriteBlock = 24,
    WriteMultipleBlock = 25,
    // 26 Reserved for manufacturer
    ProgramCSD = 27,

    SetWriteProt = 28,
    ClearWriteProt = 29,
    SendWriteProt = 30,
    // 31 Reserved

    EraseWriteBlockStart = 32,
    EraseWriteBlockEnd = 33,
    // 34-37 Reserved for command system set by SwitchFunc
    Erase = 38,
    // 39 Reserved
    // 40 Reserved for security spec
    // 41 Reserved

    LockUnlock = 42,
    // 43-49 Reserved
    // 50 Reserved for command system set by SwitchFunc
    // 51 Reserved
    // 52-54 used by SDIO

    AppCmd = 55,
    GenCmd = 56,
    // 57-58 Reserved
    // 60-63 Reserved for manufacturer
  };

  enum class AppCommand
  {
    // 1-5 Reserved
    // SetBusWidth = 6,  // Not SPI
    // 7-12 Reserved
    SDStatus = 13,
    // 14-16 Reserved for security spec
    // 17 Reserved
    // 18 Reserved for SD security
    // 19-21 Reserved
    SendNumWrittenBlocks = 22,
    SetWriteBlockEraseCount = 23,
    // 24 Reserved
    // 25 Reserved for SD security
    // 26 Reserved for SD security
    // 27-28 Reserved for security spec
    // 29 Reserved
    // 30-35 Reserved for security spec
    // 36-37 Reserved
    // 38 Reserved for SD security
    // 39-40 Reserved
    SDSendOpCond = 41,
    SetClearCardDetect = 42,
    // 43 Reserved for SD security
    // 49 Reserved for SD security
    SendSCR = 51,
    // 52-54 Reserved for security spec
    AppCmd = 55,
    // 56-59 Reserved for security spec
  };

  void WriteByte(u8 byte);
  void HandleCommand(Command command, u32 argument);
  void HandleAppCommand(AppCommand app_command, u32 argument);
  u8 ReadByte();

  enum class R1
  {
    InIdleState = 1 << 0,
    EraseRequest = 1 << 1,
    IllegalCommand = 1 << 2,
    CommunicationCRCError = 1 << 3,
    EraseSequenceError = 1 << 4,
    AddressError = 1 << 5,
    ParameterError = 1 << 6,
    // Top bit 0
  };
  enum class R2
  {
    CardIsLocked = 1 << 0,
    WriteProtectEraseSkip = 1 << 1,  // or lock/unlock command failed
    Error = 1 << 2,
    CardControllerError = 1 << 3,
    CardEccFailed = 1 << 4,
    WriteProtectViolation = 1 << 5,
    EraseParam = 1 << 6,
    // OUT_OF_RANGE_OR_CSD_OVERWRITE, not documented in text?
  };

  File::IOFile m_card;

  // STATE_TO_SAVE
  bool inited = false;
  bool get_id = false;
  bool next_is_appcmd = false;
  u32 command_position = 0;
  u32 block_position = 0;
  std::array<u8, 6> command_buffer = {};
  std::deque<u8> response;
  std::array<u8, 512> block_buffer = {};
};
}  // namespace ExpansionInterface
