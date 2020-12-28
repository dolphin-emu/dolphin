// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#if defined(HAVE_PULSEAUDIO) && HAVE_PULSEAUDIO
#include <pulse/pulseaudio.h>
#endif

#include <atomic>

#include "AudioCommon/SoundStream.h"
#include "Common/CommonTypes.h"
#include "Common/Flag.h"
#include "Common/Thread.h"

class PulseAudio final : public SoundStream
{
public:
  static std::string GetName() { return "Pulse"; }
#if defined(HAVE_PULSEAUDIO) && HAVE_PULSEAUDIO
  static bool IsValid() { return true; }
  ~PulseAudio() override;

  bool Init() override;
  bool SetRunning(bool running) override;
  static bool SupportsSurround() { return true; }
  AudioCommon::SurroundState GetSurroundState() const override;
  void StateCallback(pa_context* c);
  void WriteCallback(pa_stream* s, size_t length);
  void UnderflowCallback(pa_stream* s);

private:
  void SoundLoop() override;

  bool PulseInit();
  void PulseShutdown();

  // wrapper callback functions, last parameter _must_ be PulseAudio*
  static void StateCallback(pa_context* c, void* userdata);
  static void WriteCallback(pa_stream* s, size_t length, void* userdata);
  static void UnderflowCallback(pa_stream* s, void* userdata);

  std::thread m_thread;
  Common::Flag m_run_thread;
  std::atomic<bool> m_running = false;

  bool m_stereo;  // stereo, else surround
  int m_bytespersample;
  int m_channels;

  int m_pa_error;
  int m_pa_connected;
  pa_mainloop* m_pa_ml = nullptr;
  pa_mainloop_api* m_pa_mlapi = nullptr;
  pa_context* m_pa_ctx = nullptr;
  pa_stream* m_pa_s = nullptr;
  pa_buffer_attr m_pa_ba;
#endif
};
