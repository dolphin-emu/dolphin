// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

class PointerWrap;
namespace DiscIO
{
class VolumeDisc;
struct Partition;
}  // namespace DiscIO
namespace MMIO
{
class Mapping;
}

namespace DVDInterface
{
class DVDInterfaceState
{
public:
  DVDInterfaceState();
  DVDInterfaceState(const DVDInterfaceState&) = delete;
  DVDInterfaceState(DVDInterfaceState&&) = delete;
  DVDInterfaceState& operator=(const DVDInterfaceState&) = delete;
  DVDInterfaceState& operator=(DVDInterfaceState&&) = delete;
  ~DVDInterfaceState();

  struct Data;
  Data& GetData() { return *m_data; }

private:
  std::unique_ptr<Data> m_data;
};

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

void Init();
void ResetDrive(bool spinup);
void Shutdown();
void DoState(PointerWrap& p);

void RegisterMMIO(MMIO::Mapping* mmio, u32 base, bool is_wii);

void SetDisc(std::unique_ptr<DiscIO::VolumeDisc> disc,
             std::optional<std::vector<std::string>> auto_disc_change_paths);
bool IsDiscInside();
void EjectDisc(EjectCause cause);                        // Must only be called on the CPU thread
void ChangeDisc(const std::vector<std::string>& paths);  // Must only be called on the CPU thread
void ChangeDisc(const std::string& new_path);            // Must only be called on the CPU thread
bool AutoChangeDisc();                                   // Must only be called on the CPU thread

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

// For circumventing Error #001 in DirectoryBlobs, which may have data in the offsets being checked.
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

}  // namespace DVDInterface
