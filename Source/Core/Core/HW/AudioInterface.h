// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// See CPP file for comments.

#pragma once

#include <memory>

#include "Common/CommonTypes.h"

class PointerWrap;
namespace MMIO
{
class Mapping;
}

namespace AudioInterface
{
class AudioInterfaceState
{
public:
  AudioInterfaceState();
  AudioInterfaceState(const AudioInterfaceState&) = delete;
  AudioInterfaceState(AudioInterfaceState&&) = delete;
  AudioInterfaceState& operator=(const AudioInterfaceState&) = delete;
  AudioInterfaceState& operator=(AudioInterfaceState&&) = delete;
  ~AudioInterfaceState();

  struct Data;
  Data& GetData() { return *m_data; }

private:
  std::unique_ptr<Data> m_data;
};

void Init();
void Shutdown();
void DoState(PointerWrap& p);
bool IsPlaying();

void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

// Get the audio rate divisors (divisors for 48KHz or 32KHz only)
// Mixer::FIXED_SAMPLE_RATE_DIVIDEND will be the dividend used for these divisors
u32 GetAIDSampleRateDivisor();
u32 GetAISSampleRateDivisor();

u32 Get32KHzSampleRateDivisor();
u32 Get48KHzSampleRateDivisor();

void GenerateAISInterrupt();

}  // namespace AudioInterface
