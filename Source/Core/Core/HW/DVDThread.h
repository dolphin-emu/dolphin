// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"

namespace DVDThread
{

void Start();
void Stop();
void DoState(PointerWrap &p);

void WaitUntilIdle();
void StartRead(u64 dvd_offset, u32 output_address, u32 length, bool decrypt,
               bool reply_to_ios, int ticks_until_completion);

}
