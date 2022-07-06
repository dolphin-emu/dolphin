// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <ctime>
#include <memory>
#include <queue>

#include "Common/CommonTypes.h"

#include <speex/speex_resampler.h>

struct FrameDumpContext;
class PointerWrap;

class FrameDump
{
public:
  FrameDump();
  ~FrameDump();

  // Holds relevant emulation state during a rendered frame for
  // when it is later asynchronously written.
  struct FrameState
  {
    u64 ticks = 0;
    int frame_number = 0;
    u32 savestate_index = 0;
    int refresh_rate_num = 0;
    int refresh_rate_den = 0;
  };

  struct FrameData
  {
    const u8* data = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0;
    FrameState state;
  };

  static constexpr u32 OUT_RESAMPLE_RATE = 48000;

  struct AudioState
  {
    std::queue<s16> sample_buffer = {};
    u32 sample_rate_divisor = 0;
    SpeexResamplerState* speex_ctx = nullptr;
  };

  struct AudioData
  {
    const s16* data = nullptr;
    u32 num_samples = 0;
    u32 sample_rate_divisor = 0;  // dividend is constant
    std::pair<u32, u32> volume = {};
    bool is_dtk = false;
  };

  bool Start(int w, int h, u64 start_ticks);
  void AddFrame(const FrameData&);
  void AddAudio(const AudioData&);
  void FlushAudio();
  void Stop();
  void DoState(PointerWrap&);
  bool IsStarted() const;
  FrameState FetchState(u64 ticks, int frame_number) const;

private:
  bool IsFirstFrameInCurrentFile() const;
  bool PrepareEncoding(int w, int h, u64 start_ticks, u32 savestate_index);
  bool CreateVideoFile();
  void CloseVideoFile();
  void CheckForConfigChange(const FrameData&);
  void ProcessPackets();

#if defined(HAVE_FFMPEG)
  std::unique_ptr<FrameDumpContext> m_context;
#endif

  std::mutex m_mutex;

  // Used for FetchState:
  u32 m_savestate_index = 0;

  // Used for filename generation.
  std::time_t m_start_time = {};
  u32 m_file_index = 0;
};

#if !defined(HAVE_FFMPEG)
inline FrameDump::FrameDump() = default;
inline FrameDump::~FrameDump() = default;

inline FrameDump::FrameState FrameDump::FetchState(u64 ticks, int frame_number) const
{
  return {};
}
#endif
