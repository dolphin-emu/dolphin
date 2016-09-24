// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

#include "Common/CommonTypes.h"

class PointerWrap;
namespace DVDInterface
{
enum class ReplyType : u32;
}
namespace DiscIO
{
enum class Platform;
class IVolume;
}
namespace IOS
{
namespace ES
{
class TMDReader;
class TicketReader;
}
}

namespace DVDThread
{
void Start();
void Stop();
void DoState(PointerWrap& p);

void SetDisc(std::unique_ptr<DiscIO::IVolume> disc);
bool HasDisc();

u64 PartitionOffsetToRawOffset(u64 offset);
DiscIO::Platform GetDiscType();
IOS::ES::TMDReader GetTMD();
IOS::ES::TicketReader GetTicket();
bool ChangePartition(u64 offset);
// If a disc is inserted and its title ID is equal to the title_id argument, returns true and
// calls SConfig::SetRunningGameMetadata(IVolume&). Otherwise, returns false.
bool UpdateRunningGameMetadata(u64 title_id);
// If a disc is inserted, returns true and calls
// SConfig::SetRunningGameMetadata(IVolume&). Otherwise, returns false.
bool UpdateRunningGameMetadata();

void StartRead(u64 dvd_offset, u32 length, bool decrypt, DVDInterface::ReplyType reply_type,
               s64 ticks_until_completion);
void StartReadToEmulatedRAM(u32 output_address, u64 dvd_offset, u32 length, bool decrypt,
                            DVDInterface::ReplyType reply_type, s64 ticks_until_completion);
}
