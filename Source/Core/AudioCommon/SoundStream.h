// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "AudioCommon/Mixer.h"
#include "AudioCommon/Enums.h"
#include "Common/CommonTypes.h"

class Mixer;

// Abstract class for different sound backends.
// Needs to be created and destroyed within the same thread.
// You generally want to redeclare IsValid() and GetName().
// And optionally SupportsSurround(), SupportsCustomLatency() and SupportsVolumeChanges()
class SoundStream
{
protected:
  std::unique_ptr<Mixer> m_mixer;

public:
  SoundStream();
  virtual ~SoundStream() {}
  // Returns a friendly name for UI and configs
  static std::string GetName() { return ""; }
  static bool IsValid() { return false; }
  static bool SupportsSurround() { return false; }
  static bool SupportsCustomLatency() { return false; }
  static bool SupportsVolumeChanges() { return false; }
  Mixer* GetMixer() const { return m_mixer.get(); }
  virtual bool Init() { return false; }
  virtual void SetVolume(int) {}
  virtual void SoundLoop() {}
  virtual void Update() {}
  virtual bool SupportsRuntimeSettingsChanges() const { return false; }
  // This is for UI so it doesn't need to be 100% up to date
  virtual AudioCommon::SurroundState GetSurroundState() const
  {
    return AudioCommon::SurroundState::Disabled;
  }
  virtual void OnSettingsChanged() {}
  // Can be called by the main thread or the emulator/video thread,
  // never concurrently. Only call this through AudioCommon
  virtual bool SetRunning(bool running) { return false; }
};
