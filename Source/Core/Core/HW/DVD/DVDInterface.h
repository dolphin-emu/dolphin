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

void SetDisc(std::unique_ptr<DiscIO::VolumeDisc> disc,
             std::optional<std::vector<std::string>> auto_disc_change_paths);
bool IsDiscInside();
void EjectDisc();                                        // Must only be called on the CPU thread
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
void ChangePartition(const DiscIO::Partition& partition);
void ExecuteCommand(u32 command_0, u32 command_1, u32 command_2, u32 output_address,
                    u32 output_length, bool reply_to_ios);

// Used by DVDThread
void FinishExecutingCommand(ReplyType reply_type, DIInterruptType interrupt_type, s64 cycles_late,
                            const std::vector<u8>& data = std::vector<u8>());

}  // end of namespace DVDInterface
