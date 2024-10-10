// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Common/BitField.h"
#include "Common/CommonTypes.h"

#include "Core/HW/StreamADPCM.h"

class PointerWrap;
namespace Core
{
class CPUThreadGuard;
class System;
}  // namespace Core
namespace CoreTiming
{
struct EventType;
}
namespace DiscIO
{
class VolumeDisc;
struct Partition;
}  // namespace DiscIO
namespace MMIO
{
class Mapping;
}

namespace DVD
{
enum class DICommand : u8
{
  Inquiry = 0x12,
  ReportKey = 0xa4,
  Read = 0xa8,
  Seek = 0xab,
  ReadDVDMetadata = 0xad,
  ReadDVD = 0xd0,
  ReadDVDConfig = 0xd1,
  StopLaser = 0xd2,
  Offset = 0xd9,
  ReadBCA = 0xda,
  RequestDiscStatus = 0xdb,
  RequestRetryNumber = 0xdc,
  SetMaximumRotation = 0xdd,
  SerMeasControl = 0xdf,
  RequestError = 0xe0,
  AudioStream = 0xe1,
  RequestAudioStatus = 0xe2,
  StopMotor = 0xe3,
  AudioBufferConfig = 0xe4,
  Debug = 0xfe,
  DebugUnlock = 0xff,
  Unknown55 = 0x55,
  UnknownEE = 0xee,
};

// Disc drive state.
// Reported in error codes as 0 for Ready, and value-1 for the rest
// (i.e. Ready and ReadyNoReadsMade are both reported as 0)
enum class DriveState : u8
{
  Ready = 0,
  ReadyNoReadsMade = 1,
  CoverOpened = 2,
  DiscChangeDetected = 3,
  NoMediumPresent = 4,
  MotorStopped = 5,
  DiscIdNotRead = 6
};

// Actual drive error codes, which fill the remaining 3 bytes
// Numbers more or less match a SCSI sense key (1 nybble) followed by SCSI ASC/ASCQ (2 bytes).
enum class DriveError : u32
{
  None = 0x00000,                  // No error.
  MotorStopped = 0x20400,          // Motor stopped.
  NoDiscID = 0x20401,              // Disk ID not read.
  MediumNotPresent = 0x23a00,      // Medium not present / Cover opened.
  SeekNotDone = 0x30200,           // No seek complete.
  ReadError = 0x31100,             // Unrecovered read error.
  ProtocolError = 0x40800,         // Transfer protocol error.
  InvalidCommand = 0x52000,        // Invalid command operation code.
  NoAudioBuf = 0x52001,            // Audio Buffer not set.
  BlockOOB = 0x52100,              // Logical block address out of bounds.
  InvalidField = 0x52400,          // Invalid field in command packet.
  InvalidAudioCommand = 0x52401,   // Invalid audio command.
  InvalidPeriod = 0x52402,         // Configuration out of permitted period.
  EndOfUserArea = 0x56300,         // End of user area encountered on this track.
  MediumChanged = 0x62800,         // Medium may have changed.
  MediumRemovalRequest = 0xb5a01,  // Operator medium removal request.
};

enum class DIInterruptType : int
{
  DEINT = 0,
  TCINT = 1,
  BRKINT = 2,
  CVRINT = 3,
};

enum class ReplyType : u32
{
  NoReply,
  Interrupt,
  IOS,
  DTK,
};

enum class EjectCause
{
  User,
  Software,
};

class DVDInterface
{
public:
  explicit DVDInterface(Core::System& system);
  DVDInterface(const DVDInterface&) = delete;
  DVDInterface(DVDInterface&&) = delete;
  DVDInterface& operator=(const DVDInterface&) = delete;
  DVDInterface& operator=(DVDInterface&&) = delete;
  ~DVDInterface();

  void Init();
  void ResetDrive(bool spinup);
  void Shutdown();
  void DoState(PointerWrap& p);

  void RegisterMMIO(MMIO::Mapping* mmio, u32 base, bool is_wii);

  void SetDisc(std::unique_ptr<DiscIO::VolumeDisc> disc,
               std::optional<std::vector<std::string>> auto_disc_change_paths);
  bool IsDiscInside() const;
  void EjectDisc(const Core::CPUThreadGuard& guard, EjectCause cause);
  void ChangeDisc(const Core::CPUThreadGuard& guard, const std::vector<std::string>& paths);
  void ChangeDisc(const Core::CPUThreadGuard& guard, const std::string& new_path);
  bool AutoChangeDisc(const Core::CPUThreadGuard& guard);

  // This function returns true and calls SConfig::SetRunningGameMetadata(Volume&, Partition&)
  // if both of the following conditions are true:
  // - A disc is inserted
  // - The title_id argument doesn't contain a value, or its value matches the disc's title ID
  bool UpdateRunningGameMetadata(std::optional<u64> title_id = {});

  // Direct access to DI for IOS HLE (simpler to implement than how real IOS accesses DI,
  // and lets us skip encrypting/decrypting in some cases)
  void ExecuteCommand(ReplyType reply_type);
  void PerformDecryptingRead(u32 position, u32 length, u32 output_address,
                             const DiscIO::Partition& partition, ReplyType reply_type);

  // For circumventing Error #001 in DirectoryBlobs, which may have data in the offsets being
  // checked.
  void ForceOutOfBoundsRead(ReplyType reply_type);

  // Exposed for use by emulated BS2; does not perform any checks on drive state
  void AudioBufferConfig(bool enable_dtk, u8 dtk_buffer_length);

  void SetDriveState(DriveState state);
  void SetDriveError(DriveError error);

  // Used by DVDThread
  void FinishExecutingCommand(ReplyType reply_type, DIInterruptType interrupt_type, s64 cycles_late,
                              const std::vector<u8>& data = std::vector<u8>());

  // Used by IOS HLE
  void SetInterruptEnabled(DIInterruptType interrupt, bool enabled);
  void ClearInterrupt(DIInterruptType interrupt);

private:
  void DTKStreamingCallback(DIInterruptType interrupt_type, const std::vector<u8>& audio_data,
                            s64 cycles_late);
  size_t ProcessDTKSamples(s16* target_samples, size_t target_block_count,
                           const std::vector<u8>& audio_data);
  u32 AdvanceDTK(u32 maximum_blocks, u32* blocks_to_process);

  bool ShouldLidBeOpen();
  void SetLidOpen();
  void UpdateInterrupts();
  void GenerateDIInterrupt(DIInterruptType dvd_interrupt);

  bool CheckReadPreconditions();
  bool ExecuteReadCommand(u64 dvd_offset, u32 output_address, u32 dvd_length, u32 output_length,
                          const DiscIO::Partition& partition, ReplyType reply_type,
                          DIInterruptType* interrupt_type);
  void ScheduleReads(u64 offset, u32 length, const DiscIO::Partition& partition, u32 output_address,
                     ReplyType reply_type);

  static void AutoChangeDiscCallback(Core::System& system, u64 userdata, s64 cyclesLate);
  static void EjectDiscCallback(Core::System& system, u64 userdata, s64 cyclesLate);
  static void InsertDiscCallback(Core::System& system, u64 userdata, s64 cyclesLate);
  static void FinishExecutingCommandCallback(Core::System& system, u64 userdata, s64 cycles_late);

  // DI Status Register
  union UDISR
  {
    u32 Hex = 0;

    BitField<0, 1, u32> BREAK;      // Stop the Device + Interrupt
    BitField<1, 1, u32> DEINTMASK;  // Access Device Error Int Mask
    BitField<2, 1, u32> DEINT;      // Access Device Error Int
    BitField<3, 1, u32> TCINTMASK;  // Transfer Complete Int Mask
    BitField<4, 1, u32> TCINT;      // Transfer Complete Int
    BitField<5, 1, u32> BRKINTMASK;
    BitField<6, 1, u32> BRKINT;  // w 1: clear brkint
    BitField<7, 25, u32> reserved;

    UDISR() = default;
    explicit UDISR(u32 hex) : Hex{hex} {}
  };

  // DI Cover Register
  union UDICVR
  {
    u32 Hex = 0;

    BitField<0, 1, u32> CVR;         // 0: Cover closed  1: Cover open
    BitField<1, 1, u32> CVRINTMASK;  // 1: Interrupt enabled
    BitField<2, 1, u32> CVRINT;      // r 1: Interrupt requested w 1: Interrupt clear
    BitField<3, 29, u32> reserved;

    UDICVR() = default;
    explicit UDICVR(u32 hex) : Hex{hex} {}
  };

  // DI DMA Control Register
  union UDICR
  {
    u32 Hex = 0;

    BitField<0, 1, u32> TSTART;  // w:1 start   r:0 ready
    BitField<1, 1, u32> DMA;     // 1: DMA Mode
                                 // 0: Immediate Mode (can only do Access Register Command)
    BitField<2, 1, u32> RW;  // 0: Read Command (DVD to Memory)  1: Write Command (Memory to DVD)
    BitField<3, 29, u32> reserved;
  };

  // DI Config Register
  union UDICFG
  {
    u32 Hex = 0;

    BitField<0, 8, u32> CONFIG;
    BitField<8, 24, u32> reserved;

    UDICFG() = default;
    explicit UDICFG(u32 hex) : Hex{hex} {}
  };

  // Hardware registers
  UDISR m_DISR;
  UDICVR m_DICVR;
  std::array<u32, 3> m_DICMDBUF{};
  u32 m_DIMAR = 0;
  u32 m_DILENGTH = 0;
  UDICR m_DICR;
  u32 m_DIIMMBUF = 0;
  UDICFG m_DICFG;

  StreamADPCM::ADPCMDecoder m_adpcm_decoder;

  // DTK
  bool m_stream = false;
  bool m_stop_at_track_end = false;
  u64 m_audio_position = 0;
  u64 m_current_start = 0;
  u32 m_current_length = 0;
  u64 m_next_start = 0;
  u32 m_next_length = 0;
  u32 m_pending_blocks = 0;
  bool m_enable_dtk = false;
  u8 m_dtk_buffer_length = 0;  // TODO: figure out how this affects the regular buffer

  // Disc drive state
  DriveState m_drive_state = DriveState::Ready;
  DriveError m_error_code = DriveError::None;
  u64 m_disc_end_offset = 0;

  // Disc drive timing
  u64 m_read_buffer_start_time = 0;
  u64 m_read_buffer_end_time = 0;
  u64 m_read_buffer_start_offset = 0;
  u64 m_read_buffer_end_offset = 0;

  // Disc changing
  std::string m_disc_path_to_insert;
  std::vector<std::string> m_auto_disc_change_paths;
  size_t m_auto_disc_change_index = 0;

  // Events
  CoreTiming::EventType* m_finish_executing_command = nullptr;
  CoreTiming::EventType* m_auto_change_disc = nullptr;
  CoreTiming::EventType* m_eject_disc = nullptr;
  CoreTiming::EventType* m_insert_disc = nullptr;

  Core::System& m_system;
};
}  // namespace DVD
