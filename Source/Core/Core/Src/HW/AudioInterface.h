// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// See CPP file for comments.

#ifndef _AUDIOINTERFACE_H
#define _AUDIOINTERFACE_H

#include "CommonTypes.h"

class PointerWrap;

namespace AudioInterface
{

void Init();
void Shutdown();
void DoState(PointerWrap &p);

void Update(u64 userdata, int cyclesLate);

// Called by DSP emulator
void Callback_GetSampleRate(unsigned int &_AISampleRate, unsigned int &_DACSampleRate);
unsigned int Callback_GetStreaming(short* _pDestBuffer, unsigned int _numSamples, unsigned int _sampleRate = 48000);

void Read32(u32& _uReturnValue, const u32 _iAddress);
void Write32(const u32 _iValue, const u32 _iAddress);

// Get the audio rates (48000 or 32000 only)
unsigned int GetAIDSampleRate();

void GenerateAISInterrupt();

}  // namespace

#endif
