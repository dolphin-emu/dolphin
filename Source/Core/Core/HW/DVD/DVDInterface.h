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
class Volume;
struct Partition;
}
namespace MMIO
{
class Mapping;
}

namespace DVDInterface
{
// Not sure about endianness here. I'll just name them like this...
enum DIErrorLow
{
  ERROR_READY = 0x00000000,         // Ready.
  ERROR_COVER_L = 0x01000000,       // Cover is opened.
  ERROR_CHANGE_DISK = 0x02000000,   // Disk change.
  ERROR_NO_DISK = 0x03000000,       // No disk.
  ERROR_MOTOR_STOP_L = 0x04000000,  // Motor stop.
  ERROR_NO_DISKID_L = 0x05000000    // Disk ID not read.
};
enum DIErrorHigh
{
  ERROR_NONE = 0x000000,          // No error.
  ERROR_MOTOR_STOP_H = 0x020400,  // Motor stopped.
  ERROR_NO_DISKID_H = 0x020401,   // Disk ID not read.
  ERROR_COVER_H = 0x023a00,       // Medium not present / Cover opened.
  ERROR_SEEK_NDONE = 0x030200,    // No seek complete.
  ERROR_READ = 0x031100,          // Unrecovered read error.
  ERROR_PROTOCOL = 0x040800,      // Transfer protocol error.
  ERROR_INV_CMD = 0x052000,       // Invalid command operation code.
  ERROR_AUDIO_BUF = 0x052001,     // Audio Buffer not set.
  ERROR_BLOCK_OOB = 0x052100,     // Logical block address out of bounds.
  ERROR_INV_FIELD = 0x052400,     // Invalid field in command packet.
  ERROR_INV_AUDIO = 0x052401,     // Invalid audio command.
  ERROR_INV_PERIOD = 0x052402,    // Configuration out of permitted period.
  ERROR_END_USR_AREA = 0x056300,  // End of user area encountered on this track.
  ERROR_MEDIUM = 0x062800,        // Medium may have changed.
  ERROR_MEDIUM_REQ = 0x0b5a01     // Operator medium removal request.
};

enum DICommand
{
  DVDLowInquiry = 0x12,
  DVDLowReadDiskID = 0x70,
  DVDLowRead = 0x71,
  DVDLowWaitForCoverClose = 0x79,
  DVDLowGetCoverReg = 0x7a,  // DVDLowPrepareCoverRegister?
  DVDLowNotifyReset = 0x7e,
  DVDLowReadDvdPhysical = 0x80,
  DVDLowReadDvdCopyright = 0x81,
  DVDLowReadDvdDiscKey = 0x82,
  DVDLowClearCoverInterrupt = 0x86,
  DVDLowGetCoverStatus = 0x88,
  DVDLowReset = 0x8a,
  DVDLowOpenPartition = 0x8b,
  DVDLowClosePartition = 0x8c,
  DVDLowUnencryptedRead = 0x8d,
  DVDLowEnableDvdVideo = 0x8e,
  DVDLowReportKey = 0xa4,
  DVDLowSeek = 0xab,
  DVDLowReadDvd = 0xd0,
  DVDLowReadDvdConfig = 0xd1,
  DVDLowStopLaser = 0xd2,
  DVDLowOffset = 0xd9,
  DVDLowReadDiskBca = 0xda,
  DVDLowRequestDiscStatus = 0xdb,
  DVDLowRequestRetryNumber = 0xdc,
  DVDLowSetMaximumRotation = 0xdd,
  DVDLowSerMeasControl = 0xdf,
  DVDLowRequestError = 0xe0,
  DVDLowStopMotor = 0xe3,
  DVDLowAudioBufferConfig = 0xe4
};

enum DIInterruptType : int
{
  INT_DEINT = 0,
  INT_TCINT = 1,
  INT_BRKINT = 2,
  INT_CVRINT = 3,
};

enum class ReplyType : u32
{
  NoReply,
  Interrupt,
  IOS,
  DTK
};

void Init();
void Reset();
void Shutdown();
void DoState(PointerWrap& p);

void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

void SetDisc(std::unique_ptr<DiscIO::Volume> disc);
bool IsDiscInside();
void EjectDisc();                              // Must only be called on the CPU thread
void ChangeDisc(const std::string& new_path);  // Must only be called on the CPU thread

// This function returns true and calls SConfig::SetRunningGameMetadata(Volume&, Partition&)
// if both of the following conditions are true:
// - A disc is inserted
// - The title_id argument doesn't contain a value, or its value matches the disc's title ID
bool UpdateRunningGameMetadata(std::optional<u64> title_id = {});

// Direct access to DI for IOS HLE (simpler to implement than how real IOS accesses DI,
// and lets us skip encrypting/decrypting in some cases)
void ChangePartition(const DiscIO::Partition& partition);
void ExecuteCommand(u32 command_0, u32 command_1, u32 command_2, u32 output_address,
                    u32 output_length, bool reply_to_ios);

// Used by DVDThread
void FinishExecutingCommand(ReplyType reply_type, DIInterruptType interrupt_type, s64 cycles_late,
                            const std::vector<u8>& data = std::vector<u8>());

}  // end of namespace DVDInterface
