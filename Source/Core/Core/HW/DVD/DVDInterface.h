// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

// "low" error codes
constexpr u32 ERROR_READY = 0x0000000;          // Ready.
constexpr u32 ERROR_COVER = 0x01000000;         // Cover is opened.
constexpr u32 ERROR_CHANGE_DISK = 0x02000000;   // Disk change.
constexpr u32 ERROR_NO_DISK_L = 0x03000000;     // No disk.
constexpr u32 ERROR_MOTOR_STOP_L = 0x04000000;  // Motor stop.
constexpr u32 ERROR_NO_DISKID_L = 0x05000000;   // Disk ID not read.
constexpr u32 LOW_ERROR_MASK = 0xff000000;

// "high" error codes
constexpr u32 ERROR_NONE = 0x000000;          // No error.
constexpr u32 ERROR_MOTOR_STOP_H = 0x020400;  // Motor stopped.
constexpr u32 ERROR_NO_DISKID_H = 0x020401;   // Disk ID not read.
constexpr u32 ERROR_NO_DISK_H = 0x023a00;     // Medium not present / Cover opened.
constexpr u32 ERROR_SEEK_NDONE = 0x030200;    // No seek complete.
constexpr u32 ERROR_READ = 0x031100;          // Unrecovered read error.
constexpr u32 ERROR_PROTOCOL = 0x040800;      // Transfer protocol error.
constexpr u32 ERROR_INV_CMD = 0x052000;       // Invalid command operation code.
constexpr u32 ERROR_AUDIO_BUF = 0x052001;     // Audio Buffer not set.
constexpr u32 ERROR_BLOCK_OOB = 0x052100;     // Logical block address out of bounds.
constexpr u32 ERROR_INV_FIELD = 0x052400;     // Invalid field in command packet.
constexpr u32 ERROR_INV_AUDIO = 0x052401;     // Invalid audio command.
constexpr u32 ERROR_INV_PERIOD = 0x052402;    // Configuration out of permitted period.
constexpr u32 ERROR_END_USR_AREA = 0x056300;  // End of user area encountered on this track.
constexpr u32 ERROR_MEDIUM = 0x062800;        // Medium may have changed.
constexpr u32 ERROR_MEDIUM_REQ = 0x0b5a01;    // Operator medium removal request.
constexpr u32 HIGH_ERROR_MASK = 0x00ffffff;

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
void Reset(bool spinup = true);
void Shutdown();
void DoState(PointerWrap& p);

void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

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
// Exposed for use by emulated BS2; does not perform any checks on drive state
void AudioBufferConfig(bool enable_dtk, u8 dtk_buffer_length);

void SetLowError(u32 low_error);
void SetHighError(u32 high_error);

// Used by DVDThread
void FinishExecutingCommand(ReplyType reply_type, DIInterruptType interrupt_type, s64 cycles_late,
                            const std::vector<u8>& data = std::vector<u8>());

// Used by IOS HLE
void SetInterruptEnabled(DIInterruptType interrupt, bool enabled);
void ClearInterrupt(DIInterruptType interrupt);

}  // end of namespace DVDInterface
