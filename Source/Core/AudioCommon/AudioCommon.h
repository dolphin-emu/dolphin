// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "AudioCommon/Enums.h"
#include "AudioCommon/SoundStream.h"

class Mixer;

extern std::unique_ptr<SoundStream> g_sound_stream;

extern bool g_selected_sound_stream_failed;
extern std::mutex g_sound_stream_mutex;
extern std::mutex g_sound_stream_running_mutex;

namespace AudioCommon
{
void InitSoundStream();
void PostInitSoundStream();
void ShutdownSoundStream();
std::string GetDefaultSoundBackend();
std::vector<std::string> GetSoundBackends();
std::string GetBackendName();
DPL2Quality GetDefaultDPL2Quality();
bool SupportsSurround(std::string_view backend);
bool SupportsLatencyControl(std::string_view backend);
bool SupportsVolumeChanges(std::string_view backend);
bool BackendSupportsRuntimeSettingsChanges();
SurroundState GetSurroundState();
// Default "Dolphin" output and internal mixer sample rate
unsigned long GetDefaultSampleRate();
// Returns the min buffer time length it can hold (in ms), our backends can't have a latency
// higher than this, we'd ask for more than we can give. This depends on the current game DMA
// and DVD sample rate, but let's theorize the worse case (GC 48kHz mode: ~48043Hz).
// Of course we shouldn't use anything above half of what this returns
unsigned long GetMaxSupportedLatency();
// Returns the user preferred backend latency in ms.
// Can return 0 and is clamped by GetMaxSupportedLatency()
unsigned long GetUserTargetLatency();
// Returns the OS mixer sample rate (based on the currently used audio device)
unsigned long GetOSMixerSampleRate();
// Either volume only, or other settings that require a backend re-init (e.g. Latency, Device,
// Sample Rate, DPLII Enabled, DPLII Quality)
void UpdateSoundStreamSettings(bool volume_changed, bool settings_changed = false);
bool SetSoundStreamRunning(bool running, bool send_error = false);
void SendAIBuffer(const short* samples, unsigned int num_samples);
void StartAudioDump();
void StopAudioDump();
void IncreaseVolume(unsigned short offset);
void DecreaseVolume(unsigned short offset);
void ToggleMuteVolume();
}  // namespace AudioCommon
