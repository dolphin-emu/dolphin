// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// See CPP file for comments.

#pragma once

#include "Common/CommonTypes.h"

class PointerWrap;
namespace MMIO { class Mapping; }

namespace AudioInterface
{

void Init();
void Shutdown();
void DoState(PointerWrap &p);
bool IsPlaying();

void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

void Update(u64 userdata, int cyclesLate);

// Get the audio rates (48000 or 32000 only)
unsigned int GetAIDSampleRate();

void GenerateAISInterrupt();

}  // namespace
