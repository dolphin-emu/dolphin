// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <FreeSurround/FreeSurroundDecoder.h>

#include <cassert>
#include <limits>

#include "AudioCommon/Enums.h"
#include "AudioCommon/SurroundDecoder.h"
#include "Common/MathUtil.h"
#include "Core/Config/MainSettings.h"
#include "VideoCommon/OnScreenDisplay.h"

namespace AudioCommon
{
static bool IsInteger(double value)
{
  return std::floor(value) == std::ceil(value);
}

// Quality (higher quality also means more latency).
// We set a range for the quality in which we try to find a latency that is a multiple of our
// own backend latency, to keep them in sync, which decreases the final latency (if they are aligned
// the added latency is half the block size), or prioritize performance and make it a pow of 2.
static u32 DPL2QualityToFrameBlockSize(DPL2Quality quality, u32 sample_rate,
                                       u32 block_size_aid_latency_in_samples)
{
  u32 frame_block_time_min, frame_block_time_max;
  // Ranges in ms. Make sure the average of min and max is an integer,
  // it's better if they are even as well
  switch (quality)
  {
  case DPL2Quality::Low:
    // This already sounds terrible, don't go any lower
    frame_block_time_min = 6;
    frame_block_time_max = 20;
    break;
  case DPL2Quality::High:
    frame_block_time_min = 40;
    frame_block_time_max = 60;
    break;
  case DPL2Quality::Extreme:
    frame_block_time_min = 60;
    frame_block_time_max = 100;
    break;
  case DPL2Quality::Normal:
  default:
    // FreeSurround said to not go over 20ms, so this might be overkill already but that's likely a
    // mistake, the difference can be told
    frame_block_time_min = 20;
    frame_block_time_max = 40;
  }

  //To review: this is all useless, added latency is always half of the DPLII block size... right?
  // Try to find a multiple or dividend of the backend latency to align them
  double frame_block_time_average = (frame_block_time_min + frame_block_time_max) * 0.5;
  double backend_latency = double(block_size_aid_latency_in_samples * 1000) / sample_rate;
  double ratio = frame_block_time_average / backend_latency;
  double ratio_inverted = backend_latency / frame_block_time_average;
  bool is_multiple_or_dividend = IsInteger(ratio) || IsInteger(ratio_inverted);

  // if is_multiple_or_dividend, already accept frame_block_time_average
  double best_frame_block_time =
      is_multiple_or_dividend ? frame_block_time_average : backend_latency;
  while (best_frame_block_time < frame_block_time_min)
  {
    best_frame_block_time += backend_latency;
  }
  while (best_frame_block_time > frame_block_time_max)
  {
    // Theoretically even a DPLII latency of 3/4 the backend latency
    // would improve syncing, but it's too complicated to put in
    best_frame_block_time *= 0.5;
    if (best_frame_block_time < frame_block_time_min)
    {
      // We really don't want to go over the min (or the max) so fallback to quality average
      best_frame_block_time = frame_block_time_average;
    }
  }

  // If sample_rate*frame_block_time is not a multiple of 1000 or the result isn't a multiple of 2
  // (this needs to return a multiple of 2), we can't align the latencies so fallback to using pow 2
  double frame_block_double = sample_rate * best_frame_block_time / 1000.0;
  u32 frame_block = std::round(frame_block_double);
  bool use_power_of_2 = !IsInteger(frame_block_double) || (frame_block != ((frame_block / 2) * 2));

  // If we are prioritizing performance over latency, try to make the block size a power of 2
  if (use_power_of_2 || Config::Get(Config::MAIN_DPL2_PERFORMANCE_OVER_LATENCY))
  {
    // Recalculate the new block size based on the preset middle point
    frame_block = std::round(sample_rate * frame_block_time_average / 1000.0);
    frame_block = MathUtil::NearestPowerOf2(frame_block);

    // Find the actual used frame_block_time to see if it's within the accepted ranges
    double frame_block_time = frame_block * 1000 / sample_rate;
    if (frame_block_time > frame_block_time_max || frame_block_time < frame_block_time_min)
    {
      frame_block = std::round(sample_rate * frame_block_time_average / 1000.0);
    }
  }

  frame_block = (frame_block / 2) * 2;

  // Assert because FreeSurround would crash anyway, this can't be triggered as of now
  assert(frame_block > 1);
  return frame_block;
}

SurroundDecoder::SurroundDecoder(u32 sample_rate, u32 num_samples)
{
  m_fsdecoder = std::make_unique<DPL2FSDecoder>();
  Init(sample_rate, num_samples);
}

SurroundDecoder::~SurroundDecoder() = default;

void SurroundDecoder::Init(u32 sample_rate, u32 num_samples)
{
  // Guess the new block size aid latency if it hasn't been provided
  double old_latency = double(m_block_size_aid_latency_in_samples) / m_sample_rate;
  u32 new_latency_in_samples = sample_rate * old_latency;
  if (num_samples != 0)
    new_latency_in_samples = num_samples;

  u32 frame_block_size = DPL2QualityToFrameBlockSize(Config::Get(Config::MAIN_DPL2_QUALITY),
                                                     sample_rate, new_latency_in_samples);

  // Re-init. It should keep the samples in the buffer (and filling the rest with 0) while just
  // updating the settings
  if (m_sample_rate != sample_rate || m_frame_block_size != frame_block_size)
  {
    m_sample_rate = sample_rate;
    m_block_size_aid_latency_in_samples = new_latency_in_samples;
    m_frame_block_size = frame_block_size;
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
      // will overwrite older samples. Avoid adding a check/warning for now as it's very unlikely.
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
  for (size_t i = 0; i < readable_samples; ++i)
  {
    out[i] =
        s16(std::clamp(s32(m_float_conversion_buffer[i] * s32(-std::numeric_limits<s16>::min())),
                       s32(std::numeric_limits<s16>::min()), s32(std::numeric_limits<s16>::max())));
  }
  m_float_conversion_buffer.erase(readable_samples);
  has_finished = (m_float_conversion_buffer.size() == 0);
  // We could also return (and "unconvert") the samples within the m_fsdecoder but it's not worth it
  return readable_samples / STEREO_CHANNELS;
}

bool SurroundDecoder::CanReturnSamples() const
{
  return m_float_conversion_buffer.size() > 0;
}
}  // namespace AudioCommon
