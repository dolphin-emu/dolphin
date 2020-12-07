// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "AudioCommon/Mixer.h"
#include "Common/CommonTypes.h"

class Mixer;

// Abstract class for different sound backends.
// Needs to be created and destroyed within the same thread
class SoundStream
{
protected:
  std::unique_ptr<Mixer> m_mixer;

public:
  SoundStream();
  virtual ~SoundStream() {}
  static bool IsValid() { return false; }
  Mixer* GetMixer() const { return m_mixer.get(); }
  virtual bool Init() { return false; }
  virtual void SetVolume(int) {}
  virtual void SoundLoop() {}
  virtual void Update() {}
  virtual bool SupportsRuntimeSettingsChanges() const { return false; }
  // No need to implement this if SupportsRuntimeSettingsChanges() returns false
  virtual bool IsSurroundEnabled() const { return false; }
  virtual void OnSettingsChanged() {}
  // Can be called by the main thread or the emulator/video thread,
  // never concurrently. Only call this through AudioCommons
  virtual bool SetRunning(bool running) { return false; }
};
