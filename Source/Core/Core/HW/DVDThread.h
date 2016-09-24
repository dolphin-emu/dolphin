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

namespace DVDThread
{
void Start();
void Stop();
void DoState(PointerWrap& p);

void SetDisc(std::unique_ptr<DiscIO::IVolume> disc);
bool HasDisc();
DiscIO::Platform GetDiscType();
bool GetTitleID(u64* title_id);
std::vector<u8> GetTMD();
bool ChangePartition(u64 offset);

void StartRead(u64 dvd_offset, u32 length, bool decrypt, DVDInterface::ReplyType reply_type,
               s64 ticks_until_completion);
void StartReadToEmulatedRAM(u32 output_address, u64 dvd_offset, u32 length, bool decrypt,
                            DVDInterface::ReplyType reply_type, s64 ticks_until_completion);
}
