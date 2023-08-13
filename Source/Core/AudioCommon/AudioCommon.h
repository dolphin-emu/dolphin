// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "AudioCommon/Enums.h"
#include "AudioCommon/SoundStream.h"

class Mixer;

namespace Core
{
class System;
}


// SlippiChange: Added as a hook for the Jukebox to call.
//
// I've intentionally kept this outside of the C++ namespace as I am unsure of the risks
// of having this inside there with regards to how Rust receives it.
//
// If someone can prove that it's safe, then feel free to move it back into the namespace
// proper. For now, I've just prefixed the method name with the namespace name
// for grep-ability.
int AudioCommonGetCurrentVolume();

namespace AudioCommon
{
void InitSoundStream(Core::System& system);
void PostInitSoundStream(Core::System& system);
void ShutdownSoundStream(Core::System& system);
std::string GetDefaultSoundBackend();
std::vector<std::string> GetSoundBackends();
DPL2Quality GetDefaultDPL2Quality();
bool SupportsDPL2Decoder(std::string_view backend);
bool SupportsLatencyControl(std::string_view backend);
bool SupportsVolumeChanges(std::string_view backend);
void UpdateSoundStream(Core::System& system);
void SetSoundStreamRunning(Core::System& system, bool running);
void SendAIBuffer(Core::System& system, const short* samples, unsigned int num_samples);
void StartAudioDump(Core::System& system);
void StopAudioDump(Core::System& system);
void IncreaseVolume(Core::System& system, unsigned short offset);
void DecreaseVolume(Core::System& system, unsigned short offset);
void ToggleMuteVolume(Core::System& system);
}  // namespace AudioCommon
