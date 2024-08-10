// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "AudioCommon/Enums.h"

class Mixer;

namespace Core
{
class System;
}

namespace AudioCommon
{
void InitSoundStream(const Core::System& system);
void PostInitSoundStream(const Core::System& system);
void ShutdownSoundStream(const Core::System& system);
std::string GetDefaultSoundBackend();
std::vector<std::string> GetSoundBackends();
DPL2Quality GetDefaultDPL2Quality();
bool SupportsDPL2Decoder(std::string_view backend);
bool SupportsLatencyControl(std::string_view backend);
bool SupportsVolumeChanges(std::string_view backend);
void UpdateSoundStream(const Core::System& system);
void SetSoundStreamRunning(const Core::System& system, bool running);
void SendAIBuffer(const Core::System& system, const short* samples, unsigned int num_samples);
void StartAudioDump(const Core::System& system);
void StopAudioDump(const Core::System& system);
void IncreaseVolume(const Core::System& system, unsigned short offset);
void DecreaseVolume(const Core::System& system, unsigned short offset);
void ToggleMuteVolume(const Core::System& system);
}  // namespace AudioCommon
