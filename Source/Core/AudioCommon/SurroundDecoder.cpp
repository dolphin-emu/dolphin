// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <FreeSurround/FreeSurroundDecoder.h>

#include <cassert>
#include <limits>

#include "AudioCommon/Enums.h"
#include "AudioCommon/SurroundDecoder.h"
#include "Core/Config/MainSettings.h"
#include "VideoCommon/OnScreenDisplay.h"
#pragma optimize("", off) //To delete

namespace AudioCommon
{
// Quality (higher quality also means more latency). Needs to be a multiple of 2
static u32 DPL2QualityToFrameBlockSize(DPL2Quality quality, u32 sample_rate)
{
  u32 frame_block_time;  // ms
  switch (quality)
  {
  case DPL2Quality::Low:
    frame_block_time = 10;
    break;
  // FreeSurround said to not go over 20ms, so this might be overkill already
  case DPL2Quality::High:
    frame_block_time = 40;
    break;
  case DPL2Quality::Extreme:
    frame_block_time = 80;
    break;
  case DPL2Quality::Normal:
  default:
    frame_block_time = 20;
  }
  u32 frame_block = std::round(sample_rate * frame_block_time / 1000.0);
  frame_block = (frame_block / 2) * 2;
  // Assert because FreeSurround would crash anyway, this can't be triggered as of now
  assert(frame_block > 1);
  return frame_block;
}

SurroundDecoder::SurroundDecoder(u32 sample_rate)
{
  m_fsdecoder = std::make_unique<DPL2FSDecoder>();
  InitAndSetSampleRate(sample_rate);
}

SurroundDecoder::~SurroundDecoder() = default;

void SurroundDecoder::InitAndSetSampleRate(u32 sample_rate)
{
  u32 frame_block_size =
      DPL2QualityToFrameBlockSize(Config::Get(Config::MAIN_DPL2_QUALITY), sample_rate);

  // Re-init. It should keep the samples in the buffer (and filling the rest with 0) while just
  // updating the settings
  if (m_sample_rate != sample_rate || m_frame_block_size != frame_block_size)
  {
    m_frame_block_size = frame_block_size;
    m_sample_rate = sample_rate;
    // If we passed in a block size that is a power of 2, decoding performance would be better,
    // (quality would be the same), though we've decided to prioritize low latency over performance,
    // so it's better to have a block size aligned (or being a multiple/dividend) of the backend
    // latency. We could write some code and UI setting to align the backend latency to the DPLII
    // one but it wouldn't be straight forward as some backends don't support custom latency.
    // If this turned out to be too intensive on slower CPUs, we could reconsider it.
    m_fsdecoder->Init(cs_5point1, m_frame_block_size, m_sample_rate);

    // Increase m_decoded_fifo size if this becomes common occurrence
    // (the warning is sent with a very large safety margin)
    if (m_frame_block_size * SURROUND_CHANNELS * MAX_BLOCKS_BUFFERED > m_decoded_fifo.max_size())
      OSD::AddMessage("This DPLII Quality at this Sample Rate is not supported.\nLower your "
                      "settings to not risk missing samples", 6000U);
  }
  // The LFE channel (bass redirection) is disabled in the surround decoder, as most people
  // have their own low pass crossover
  m_fsdecoder->set_bass_redirection(Config::Get(Config::MAIN_DPL2_BASS_REDIRECTION));
}

void SurroundDecoder::Clear()
{
  m_fsdecoder->flush();
  m_decoded_fifo.clear();
  m_float_conversion_buffer.clear();
}

// Receive and decode samples
void SurroundDecoder::PushSamples(const s16* in, u32 num_samples)
{
  u32 read_samples = 0;
  // We do it this way so m_float_conversion_buffer never exceeds m_frame_block_size
  // and we can be sure we won't miss samples on that front
  while (num_samples > 0)
  {
    u32 samples_to_block = 0;
    if (u32(m_float_conversion_buffer.size() / STEREO_CHANNELS) < m_frame_block_size)
    {
      samples_to_block =
          m_frame_block_size - (u32(m_float_conversion_buffer.size() / STEREO_CHANNELS));
    }
    u32 samples_to_push = std::min(samples_to_block, num_samples);

    // Convert to float (samples are from SHRT_MIN to SHRT_MAX)
    for (size_t i = read_samples * STEREO_CHANNELS;
         i < (samples_to_push * STEREO_CHANNELS) + (read_samples * STEREO_CHANNELS); ++i)
    {
      m_float_conversion_buffer.push(in[i] / float(-std::numeric_limits<s16>::min()));
    }
    read_samples += samples_to_push;
    num_samples -= samples_to_push;

    // We have enough samples to process a block
    if (m_float_conversion_buffer.size() >= m_frame_block_size * STEREO_CHANNELS)
    {
      // Decode. Note that output isn't clamped within -1.f and +1.f.
      // Will return half silent (~0) samples for the first call
      const float* dpl2_fs = m_fsdecoder->decode(
          &m_float_conversion_buffer.front(), &m_float_conversion_buffer.beginning(),
          u32(m_float_conversion_buffer.size_to_end() / STEREO_CHANNELS));
      m_float_conversion_buffer.erase(m_frame_block_size * STEREO_CHANNELS);

      // TODO: modify FreeSurround to have the same channel mapping for performance (see channel_id)
      // Add to ring buffer and fix channel mapping.
      // If, at maxed out settings, your backend latency is 8 times higher than the block size, this
      // will overwrite older samples. Avoid adding a check/warning for now as it's very unlikely
      // FreeSurround:
      // FL | FC | FR | BL | BR | LFE
      // Most backends:
      // FL | FR | FC | LFE | BL | BR
      for (size_t i = 0; i < m_frame_block_size; ++i)
      {
        m_decoded_fifo.push(dpl2_fs[i * SURROUND_CHANNELS + 0]);  // LEFTFRONT
        m_decoded_fifo.push(dpl2_fs[i * SURROUND_CHANNELS + 2]);  // RIGHTFRONT
        m_decoded_fifo.push(dpl2_fs[i * SURROUND_CHANNELS + 1]);  // CENTREFRONT
        m_decoded_fifo.push(dpl2_fs[i * SURROUND_CHANNELS + 5]);  // LFE/SUB
        m_decoded_fifo.push(dpl2_fs[i * SURROUND_CHANNELS + 3]);  // LEFTREAR
        m_decoded_fifo.push(dpl2_fs[i * SURROUND_CHANNELS + 4]);  // RIGHTREAR
      }
    }
  }
}

void SurroundDecoder::GetDecodedSamples(float* out, u32 num_samples)
{
  if (num_samples == 0)
    return;

  // Copy to output array with desired num_samples
  const u32 readable_samples =
      std::min(num_samples * SURROUND_CHANNELS, u32(m_decoded_fifo.size()));
  const u32 front_readable_samples = std::min(readable_samples, u32(m_decoded_fifo.size_to_end()));
  // Samples looping over the ring buffer
  const u32 beginning_readable_samples = readable_samples - front_readable_samples;

  memcpy(&out[0], &m_decoded_fifo.front(), sizeof(float) * front_readable_samples);
  if (beginning_readable_samples > 0)
    memcpy(&out[front_readable_samples], &m_decoded_fifo.beginning(),
           sizeof(float) * beginning_readable_samples);
  m_decoded_fifo.erase(readable_samples);

  if (readable_samples > 0)
  {
    memcpy(&m_last_decoded_samples[0], &out[readable_samples - SURROUND_CHANNELS],
           sizeof(float) * SURROUND_CHANNELS);
  }

  // Padding should never happen. Once we have reached enough samples in the buffer,
  // it should always keep that number, unless the backend latency changes
  u32 read_samples = readable_samples;
  while (read_samples < num_samples * SURROUND_CHANNELS)
  {
    memcpy(&out[read_samples], &m_last_decoded_samples[0], sizeof(float) * SURROUND_CHANNELS);
    read_samples += SURROUND_CHANNELS;
  }
}

u32 SurroundDecoder::ReturnSamples(s16* out, u32 num_samples, bool& has_finished)
{
  const u32 readable_samples =
      std::min(u32(m_float_conversion_buffer.size()), num_samples * STEREO_CHANNELS);
  for (size_t i = 0, k = 0; i < readable_samples; ++i)
  {
    out[k] =
        s16(std::clamp(s32(m_float_conversion_buffer[i] * s32(-std::numeric_limits<s16>::min())),
                       s32(std::numeric_limits<s16>::min()), s32(std::numeric_limits<s16>::max())));
    ++k;
  }
  m_float_conversion_buffer.erase(readable_samples);
  has_finished = (m_float_conversion_buffer.size() == 0);
  // We could also return a part of the samples kept for future overlap in m_decoded_fifo
  // and the already converted ones in m_decoded_fifo, but it's not worth it
  return readable_samples / STEREO_CHANNELS;
}

bool SurroundDecoder::CanReturnSamples() const
{
  return m_float_conversion_buffer.size() > 0;
}
}  // namespace AudioCommon
