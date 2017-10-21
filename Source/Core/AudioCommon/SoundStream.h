// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "AudioCommon/Mixer.h"
#include "Common/CommonTypes.h"

class SoundStream
{
protected:
  std::unique_ptr<Mixer> m_mixer;

public:
  SoundStream() : m_mixer(new Mixer(48000)) {}
  virtual ~SoundStream() {}
  static bool isValid() { return false; }
  Mixer* GetMixer() const { return m_mixer.get(); }
  virtual bool Start() { return false; }
  virtual void SetVolume(int) {}
  virtual void SoundLoop() {}
  virtual void Stop() {}
  virtual void Update() {}
  virtual void SetRunning(bool running) {}
};
