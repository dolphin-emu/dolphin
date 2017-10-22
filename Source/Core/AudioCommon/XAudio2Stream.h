// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// This audio backend uses XAudio2 via XAUDIO2_DLL
// It works on Windows 8+, where it is included as an OS component.
// This backend is always compiled, but only available if running on Win8+

#pragma once

#include <memory>

#include "AudioCommon/SoundStream.h"
#include "Common/Event.h"

#ifdef _WIN32

#include <windows.h>

struct StreamingVoiceContext;
struct IXAudio2;
struct IXAudio2MasteringVoice;

#endif

class XAudio2 final : public SoundStream
{
#ifdef _WIN32

private:
  class Releaser
  {
  public:
    template <typename R>
    void operator()(R* ptr)
    {
      ptr->Release();
    }
  };

  std::unique_ptr<IXAudio2, Releaser> m_xaudio2;
  std::unique_ptr<StreamingVoiceContext> m_voice_context;
  IXAudio2MasteringVoice* m_mastering_voice;

  Common::Event m_sound_sync_event;
  float m_volume;

  const bool m_cleanup_com;

  static HMODULE m_xaudio2_dll;
  static void* PXAudio2Create;

  static bool InitLibrary();

public:
  XAudio2();
  virtual ~XAudio2();

  bool Start() override;
  void Stop() override;

  void SetRunning(bool running) override;
  void SetVolume(int volume) override;

  static bool isValid() { return InitLibrary(); }
#endif
};
