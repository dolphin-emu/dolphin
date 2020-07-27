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

#pragma optimize("", off) //To delete

namespace AudioCommon
{
// Quality (higher quality also means more latency). Needs to be a pow of 2 so we find the closest
static u32 DPL2QualityToFrameBlockSize(DPL2Quality quality, u32 sample_rate)
{
  u32 frame_block_time;  // ms
  switch (quality)
  {
  case DPL2Quality::Lowest:
    frame_block_time = 10;
    break;
  case DPL2Quality::High:
    frame_block_time = 40;
    break;
  // TODO: review this case, it's too much, FreeSurround said to not go over 20ms.
  // The quality/latency trade off is not worth it and it might introduce crackling.
  case DPL2Quality::Highest:
    frame_block_time = 80;
    break;
  case DPL2Quality::Low:
  default:
    frame_block_time = 20;
  }
  const u32 frame_block = std::round(sample_rate * frame_block_time / 1000.0);
  assert(frame_block > 1);
  return MathUtil::NearestPowerOf2(frame_block);
}

u32 SurroundDecoder::QuerySamplesNeededForSurroundOutput(u32 output_samples) const
{
  //To review: what would happen if they are ==? How is the output fifo supposed to decide the input amount?
  if (output_samples > u32(m_decoded_fifo.size()) / SURROUND_CHANNELS)
  {
    // Output stereo samples needed to have at least the desired number of surround samples
    u32 samples_needed = output_samples - (u32(m_decoded_fifo.size()) / SURROUND_CHANNELS);
    return samples_needed + m_frame_block_size - (samples_needed % m_frame_block_size);
  }

  return 0;
}

SurroundDecoder::SurroundDecoder(u32 sample_rate)
{
  m_fsdecoder = std::make_unique<DPL2FSDecoder>();
  InitAndSetSampleRate(sample_rate);
}

SurroundDecoder::~SurroundDecoder() = default;

void SurroundDecoder::InitAndSetSampleRate(u32 sample_rate)
{
  if (m_sample_rate == sample_rate)
    return;
  m_sample_rate = sample_rate;

  m_frame_block_size =
      DPL2QualityToFrameBlockSize(Config::Get(Config::MAIN_DPL2_QUALITY), m_sample_rate);

  // This DPLII quality at this sample rate is not supported, increase the size or lower your
  // settings
  assert(m_frame_block_size * STEREO_CHANNELS <= m_float_conversion_buffer.max_size());
  assert(m_frame_block_size * SURROUND_CHANNELS * MAX_BLOCKS_BUFFERED <= m_decoded_fifo.max_size());

  // Re-init. It should keep the samples in the buffer (and filling the rest with 0) while just
  // changing the settings
  m_fsdecoder->Init(cs_5point1, m_frame_block_size, m_sample_rate);
  // The LFE channel (bass redirection) is disabled in the surround decoder, as most people
  // have their own low pass crossover
  m_fsdecoder->set_bass_redirection(Config::Get(Config::MAIN_DPL2_BASS_REDIRECTION));
}

void SurroundDecoder::Clear()
{
  m_fsdecoder->flush();
  m_decoded_fifo.clear();
}

// Receive and decode samples
void SurroundDecoder::PushSamples(const s16* in, u32 num_samples)
{
  //To make sure
  assert(num_samples % m_frame_block_size == 0);
  // We support a max of MAX_BLOCKS_BUFFERED blocks in the buffer, because of m_decoded_fifo,
  // just increase if you need. This might trigger if you have very high backend latencies
  assert(num_samples <= m_frame_block_size * MAX_BLOCKS_BUFFERED);
  u32 remaining_samples = num_samples;
  size_t sample_index = 0;

  while (remaining_samples > 0)
  {
    // Convert to float
    for (size_t i = 0, end = m_frame_block_size * STEREO_CHANNELS; i < end; ++i)
    {
      m_float_conversion_buffer[i] =
          in[i + sample_index * STEREO_CHANNELS] / float(std::numeric_limits<s16>::max());
    }

    // Decode. Note that output isn't clamped within -1.f and +1.f
    const float* dpl2_fs = m_fsdecoder->decode(m_float_conversion_buffer.data());

    // Add to ring buffer and fix channel mapping
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

    remaining_samples = remaining_samples - s32(m_frame_block_size);
    sample_index = sample_index + m_frame_block_size;
  }
}

void SurroundDecoder::GetDecodedSamples(float* out, u32 num_samples)
{
  // TODO: this could be optimized by copying the ring buffer in two parts
  // Copy to output array with desired num_samples
  for (size_t i = 0, num_samples_output = num_samples * SURROUND_CHANNELS; i < num_samples_output;
       ++i)
  {
    out[i] = m_decoded_fifo.pop_front();
  }
}
}  // namespace AudioCommon
