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

// Disable warning C4265 in wrl/client.h:
//   'Microsoft::WRL::Details::RemoveIUnknownBase<T>': class has virtual functions,
//   but destructor is not virtual
#pragma warning(push)
#pragma warning(disable : 4265)
#include <wrl/client.h>
#pragma warning(pop)

using Microsoft::WRL::ComPtr;

struct StreamingVoiceContext;
struct IXAudio2;
struct IXAudio2MasteringVoice;

#endif

class XAudio2 final : public SoundStream
{
#ifdef _WIN32

private:
  ComPtr<IXAudio2> m_xaudio2;
  std::unique_ptr<StreamingVoiceContext> m_voice_context;
  // all XAudio2 objects are released when m_xaudio2 is released
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

  void Clear(bool mute) override;
  void SetVolume(int volume) override;

  static bool isValid() { return InitLibrary(); }
#endif
};
