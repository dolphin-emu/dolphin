// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <deque>
#include <string>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "Core/HW/EXI/EXI_Device.h"

class PointerWrap;

namespace ExpansionInterface
{
// EXI-SD adapter (DOL-019)
// The SD adapter uses SPI mode to communicate with the SD card (SPI and EXI are basically the
// same). Refer to the simplified specification, version 3.01, at
// https://web.archive.org/web/20131205014133/https://www.sdcard.org/downloads/pls/simplified_specs/archive/part1_301.pdf
// for details (SPI information starts on section 7, page 113 (125 in the PDF).
class CEXISD final : public IEXIDevice
{
public:
  explicit CEXISD(Core::System& system, int channel_num);

  void ImmWrite(u32 data, u32 size) override;
  u32 ImmRead(u32 size) override;
  void ImmReadWrite(u32& data, u32 size) override;
  // TODO: DMA
  void SetCS(u32 cs, bool was_selected, bool is_selected) override;

  bool IsPresent() const override;
  void DoState(PointerWrap& p) override;

private:
  enum class Command  // § 7.3.1.1, Table 7-3
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

    WriteSingleBlock = 24,
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
    // 57 Reserved for command system set by SwitchFunc
    ReadOCR = 58,
    CRCOnOff = 59,
    // 60-63 Reserved for manufacturer
  };

  enum class AppCommand  // § 7.3.1.1, Table 7-4
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

  enum class OCR : u32  // Operating Conditions Register, § 5.1
  {
    // 0-6 reserved
    // 7 reserved for Low Voltage Range
    // 8-14 reserved
    // All of these (including the above reserved bits) are the VDD Voltage Window.
    // e.g. Vdd27To28 indicates 2.7 to 2.8 volts
    Vdd27To28 = 1 << 15,
    Vdd28To29 = 1 << 16,
    Vdd29To30 = 1 << 17,
    Vdd30To31 = 1 << 18,
    Vdd31To32 = 1 << 19,
    Vdd32To33 = 1 << 20,
    Vdd33To34 = 1 << 21,
    Vdd34To35 = 1 << 22,
    Vdd35To36 = 1 << 23,
    // "S18A" (not part of VDD Voltage Window)
    SwitchTo18VAccepted = 1 << 24,
    // 25-29 reserved
    // "CCS", only valid after startup done.  This bit being set indicates SDHC.
    CardCapacityStatus = 1 << 30,
    // 0 if card is still starting up
    CardPowerUpStatus = 1u << 31,
  };

  static constexpr Common::Flags<OCR> OCR_DEFAULT{
      OCR::Vdd27To28, OCR::Vdd28To29, OCR::Vdd29To30, OCR::Vdd30To31, OCR::Vdd31To32,
      OCR::Vdd32To33, OCR::Vdd33To34, OCR::Vdd34To35, OCR::Vdd35To36, OCR::CardPowerUpStatus};

  // § 7.3.3
  static constexpr u8 START_BLOCK = 0xfe, START_MULTI_BLOCK = 0xfc, END_BLOCK = 0xfd;
  // The spec has the first 3 bits of the data responses marked with an x, and doesn't explain why
  static constexpr u8 DATA_RESPONSE_ACCEPTED = 0b0'010'0;
  static constexpr u8 DATA_RESPONSE_BAD_CRC = 0b0'101'0;
  static constexpr u8 DATA_RESPONSE_WRITE_ERROR = 0b0'110'0;
  // "Same error bits" as in R2, but I guess that only refers to meaning, not the actual bit values
  static constexpr u8 DATA_ERROR_ERROR = 0x01;
  static constexpr u8 DATA_ERROR_CONTROLLER = 0x02;
  static constexpr u8 DATA_ERROR_ECC = 0x04;
  static constexpr u8 DATA_ERROR_OUT_OF_RANGE = 0x08;

  static constexpr size_t BLOCK_SIZE = 512;

  enum class State
  {
    // Hacky setup
    Uninitialized,
    GetId,
    // Actual states for transmiting and receiving
    ReadyForCommand,
    ReadyForAppCommand,
    SingleBlockRead,
    MultipleBlockRead,
    SingleBlockWrite,
    MultipleBlockWrite,
  };

  enum class BlockState
  {
    Nothing,
    Response,
    Token,
    Block,
    Checksum1,
    Checksum2,
    ChecksumWritten,
  };

  void WriteByte(u8 byte);
  void HandleCommand(Command command, u32 argument);
  void HandleAppCommand(AppCommand app_command, u32 argument);
  u8 ReadByte();

  u8 ReadForBlockRead();
  void WriteForBlockRead(u8 byte);
  u8 ReadForBlockWrite();
  void WriteForBlockWrite(u8 byte);

  enum class R1  // § 7.3.2.1
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
  enum class R2  // § 7.3.2.3
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
  State state = State::Uninitialized;
  BlockState block_state = BlockState::Nothing;
  u32 command_position = 0;
  u32 block_position = 0;
  std::array<u8, 6> command_buffer = {};
  std::deque<u8> response = {};
  std::array<u8, BLOCK_SIZE> block_buffer = {};
  u64 address = 0;
  u16 block_crc = 0;
};
}  // namespace ExpansionInterface
