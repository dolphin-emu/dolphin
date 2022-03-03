// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>

#include "AudioCommon/PulseAudioStream.h"

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Core/Config/MainSettings.h"

namespace
{
const size_t BUFFER_SAMPLES = 512;  // ~10 ms - needs to be at least 240 for surround
}

PulseAudio::PulseAudio() = default;

bool PulseAudio::Init()
{
  m_stereo = !Config::ShouldUseDPL2Decoder();
  m_channels = m_stereo ? 2 : 6;  // will tell PA we use a Stereo or 5.0 channel setup

  NOTICE_LOG_FMT(AUDIO, "PulseAudio backend using {} channels", m_channels);

  m_run_thread.Set();
  m_thread = std::thread(&PulseAudio::SoundLoop, this);

  return true;
}

PulseAudio::~PulseAudio()
{
  m_run_thread.Clear();
  m_thread.join();
}

// Called on audio thread.
void PulseAudio::SoundLoop()
{
  Common::SetCurrentThreadName("Audio thread - pulse");

  if (PulseInit())
  {
    while (m_run_thread.IsSet() && m_pa_connected == 1 && m_pa_error >= 0)
      m_pa_error = pa_mainloop_iterate(m_pa_ml, 1, nullptr);

    if (m_pa_error < 0)
      ERROR_LOG_FMT(AUDIO, "PulseAudio error: {}", pa_strerror(m_pa_error));

    PulseShutdown();
  }
}

bool PulseAudio::PulseInit()
{
  m_pa_error = 0;
  m_pa_connected = 0;

  // create pulseaudio main loop and context
  // also register the async state callback which is called when the connection to the pa server has
  // changed
  m_pa_ml = pa_mainloop_new();
  m_pa_mlapi = pa_mainloop_get_api(m_pa_ml);
  m_pa_ctx = pa_context_new(m_pa_mlapi, "dolphin-emu");
  m_pa_error = pa_context_connect(m_pa_ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
  pa_context_set_state_callback(m_pa_ctx, StateCallback, this);

  // wait until we're connected to the pulseaudio server
  while (m_pa_connected == 0 && m_pa_error >= 0)
    m_pa_error = pa_mainloop_iterate(m_pa_ml, 1, nullptr);

  if (m_pa_connected == 2 || m_pa_error < 0)
  {
    ERROR_LOG_FMT(AUDIO, "PulseAudio failed to initialize: {}", pa_strerror(m_pa_error));
    return false;
  }

  // create a new audio stream with our sample format
  // also connect the callbacks for this stream
  pa_sample_spec ss;
  pa_channel_map channel_map;
  pa_channel_map* channel_map_p = nullptr;  // auto channel map
  if (m_stereo)
  {
    ss.format = PA_SAMPLE_S16LE;
    m_bytespersample = sizeof(s16);
  }
  else
  {
    // surround is remixed in floats, use a float PA buffer to save another conversion
    ss.format = PA_SAMPLE_FLOAT32NE;
    m_bytespersample = sizeof(float);

    channel_map_p = &channel_map;  // explicit channel map:
    channel_map.channels = 6;
    channel_map.map[0] = PA_CHANNEL_POSITION_FRONT_LEFT;
    channel_map.map[1] = PA_CHANNEL_POSITION_FRONT_RIGHT;
    channel_map.map[2] = PA_CHANNEL_POSITION_FRONT_CENTER;
    channel_map.map[3] = PA_CHANNEL_POSITION_LFE;
    channel_map.map[4] = PA_CHANNEL_POSITION_REAR_LEFT;
    channel_map.map[5] = PA_CHANNEL_POSITION_REAR_RIGHT;
  }
  ss.channels = m_channels;
  ss.rate = m_mixer->GetSampleRate();
  ASSERT(pa_sample_spec_valid(&ss));
  m_pa_s = pa_stream_new(m_pa_ctx, "Playback", &ss, channel_map_p);
  pa_stream_set_write_callback(m_pa_s, WriteCallback, this);
  pa_stream_set_underflow_callback(m_pa_s, UnderflowCallback, this);

  // connect this audio stream to the default audio playback
  // limit buffersize to reduce latency
  m_pa_ba.fragsize = -1;
  m_pa_ba.maxlength = -1;  // max buffer, so also max latency
  m_pa_ba.minreq = -1;     // don't read every byte, try to group them _a bit_
  m_pa_ba.prebuf = -1;     // start as early as possible
  m_pa_ba.tlength =
      BUFFER_SAMPLES * m_channels *
      m_bytespersample;  // designed latency, only change this flag for low latency output
  pa_stream_flags flags = pa_stream_flags(PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_ADJUST_LATENCY |
                                          PA_STREAM_AUTO_TIMING_UPDATE);
  m_pa_error = pa_stream_connect_playback(m_pa_s, nullptr, &m_pa_ba, flags, nullptr, nullptr);
  if (m_pa_error < 0)
  {
    ERROR_LOG_FMT(AUDIO, "PulseAudio failed to initialize: {}", pa_strerror(m_pa_error));
    return false;
  }

  INFO_LOG_FMT(AUDIO, "Pulse successfully initialized");
  return true;
}

void PulseAudio::PulseShutdown()
{
  pa_context_disconnect(m_pa_ctx);
  pa_context_unref(m_pa_ctx);
  pa_mainloop_free(m_pa_ml);
}

void PulseAudio::StateCallback(pa_context* c)
{
  pa_context_state_t state = pa_context_get_state(c);
  switch (state)
  {
  case PA_CONTEXT_FAILED:
  case PA_CONTEXT_TERMINATED:
    m_pa_connected = 2;
    break;
  case PA_CONTEXT_READY:
    m_pa_connected = 1;
    break;
  default:
    break;
  }
}
// on underflow, increase pulseaudio latency in ~10ms steps
void PulseAudio::UnderflowCallback(pa_stream* s)
{
  m_pa_ba.tlength += BUFFER_SAMPLES * m_channels * m_bytespersample;
  pa_operation* op = pa_stream_set_buffer_attr(s, &m_pa_ba, nullptr, nullptr);
  pa_operation_unref(op);

  WARN_LOG_FMT(AUDIO, "pulseaudio underflow, new latency: {} bytes", m_pa_ba.tlength);
}

void PulseAudio::WriteCallback(pa_stream* s, size_t length)
{
  int bytes_per_frame = m_channels * m_bytespersample;
  int frames = (length / bytes_per_frame);
  size_t trunc_length = frames * bytes_per_frame;

  // fetch dst buffer directly from pulseaudio, so no memcpy is needed
  void* buffer;
  m_pa_error = pa_stream_begin_write(s, &buffer, &trunc_length);

  if (!buffer || m_pa_error < 0)
    return;  // error will be printed from main loop

  if (m_stereo)
  {
    // use the raw s16 stereo mix
    m_mixer->Mix((s16*)buffer, frames);
  }
  else
  {
    if (m_channels == 6)  // Extract dpl2/5.1 Surround
    {
      m_mixer->MixSurround((float*)buffer, frames);
    }
    else
    {
      ERROR_LOG_FMT(AUDIO, "Unsupported number of PA channels requested: {}", m_channels);
      return;
    }
  }

  m_pa_error = pa_stream_write(s, buffer, trunc_length, nullptr, 0, PA_SEEK_RELATIVE);
}

// Callbacks that forward to internal methods (required because PulseAudio is a C API).

void PulseAudio::StateCallback(pa_context* c, void* userdata)
{
  PulseAudio* p = (PulseAudio*)userdata;
  p->StateCallback(c);
}

void PulseAudio::UnderflowCallback(pa_stream* s, void* userdata)
{
  PulseAudio* p = (PulseAudio*)userdata;
  p->UnderflowCallback(s);
}

void PulseAudio::WriteCallback(pa_stream* s, size_t length, void* userdata)
{
  PulseAudio* p = (PulseAudio*)userdata;
  p->WriteCallback(s, length);
}
