// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
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

void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

void Update(u64 userdata, int cyclesLate);

// Called by DSP emulator
void Callback_GetSampleRate(unsigned int &_AISampleRate, unsigned int &_DACSampleRate);
unsigned int Callback_GetStreaming(short* _pDestBuffer, unsigned int _numSamples, unsigned int _sampleRate = 48000);

// Get the audio rates (48000 or 32000 only)
unsigned int GetAIDSampleRate();

void GenerateAISInterrupt();

}  // namespace
