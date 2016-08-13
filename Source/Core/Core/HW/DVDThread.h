// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

class PointerWrap;
namespace DVDInterface
{
enum class ReplyType : u32;
}

namespace DVDThread
{
void Start();
void Stop();
void DoState(PointerWrap& p);

void WaitUntilIdle();
void StartRead(u64 dvd_offset, u32 length, bool decrypt, DVDInterface::ReplyType reply_type,
               s64 ticks_until_completion);
void StartReadToEmulatedRAM(u32 output_address, u64 dvd_offset, u32 length, bool decrypt,
                            DVDInterface::ReplyType reply_type, s64 ticks_until_completion);
}
