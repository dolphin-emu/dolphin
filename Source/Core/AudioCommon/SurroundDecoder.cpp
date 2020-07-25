// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <FreeSurround/FreeSurroundDecoder.h>
#include <limits>

#include "AudioCommon/SurroundDecoder.h"

namespace AudioCommon
{
constexpr size_t STEREO_CHANNELS = 2;
constexpr size_t SURROUND_CHANNELS = 6;

// Quality (higher quality also means more latency)
static u32 DPL2QualityToFrameBlockSize(DPL2Quality quality, u32 sample_rate)
{
  switch (quality)
  {
  case DPL2Quality::Lowest:
    return 512;
  case DPL2Quality::Low:
    return 1024;
  case DPL2Quality::Highest:
    return 4096;
  default:  // AudioCommon::DPL2Quality::High
    return 2048;
  }
}

SurroundDecoder::SurroundDecoder(u32 sample_rate, DPL2Quality quality)
    : m_sample_rate(sample_rate), m_frame_block_size(DPL2QualityToFrameBlockSize(quality, sample_rate))
{
  m_fsdecoder = std::make_unique<DPL2FSDecoder>();
  m_fsdecoder->Init(cs_5point1, m_frame_block_size, m_sample_rate);
  //To review (make config and remove comment, also, move to re-init): m_fsdecoder->set_bass_redirection(false);
}

SurroundDecoder::~SurroundDecoder() = default;

void SurroundDecoder::Clear()
{
  m_fsdecoder->flush();
  m_decoded_fifo.clear();
}

void SurroundDecoder::SetSampleRate(u32 sample_rate)
{
  //To finish, also set DPL2QualityToFrameBlockSize and fix it by sample rate
  // With "FreeSurroundDecoder" there is no need of setting the sample_rate, it's only used 
}

// Currently only 6 channels are supported.
size_t SurroundDecoder::QuerySamplesNeededForSurroundOutput(const size_t output_samples) const
{
  if (m_decoded_fifo.size() < output_samples * SURROUND_CHANNELS)
  {
    // Output stereo samples needed to have at least the desired number of surround samples
    size_t samples_needed = output_samples - m_decoded_fifo.size() / SURROUND_CHANNELS;
    return samples_needed + m_frame_block_size - samples_needed % m_frame_block_size;
  }

  return 0;
}

// Receive and decode samples
void SurroundDecoder::PushSamples(const s16* in, const size_t num_samples)
{
  // Maybe check if it is really power-of-2?
  s64 remaining_samples = static_cast<s64>(num_samples);
  size_t sample_index = 0;

  while (remaining_samples > 0)
  {
    // Convert to float
    for (size_t i = 0, end = m_frame_block_size * STEREO_CHANNELS; i < end; ++i)
    {
      m_float_conversion_buffer[i] = in[i + sample_index * STEREO_CHANNELS] /
                                     static_cast<float>(std::numeric_limits<s16>::max());
    }

    // Decode
    const float* dpl2_fs = m_fsdecoder->decode(m_float_conversion_buffer.data());

    // Add to ring buffer and fix channel mapping
    // Maybe modify FreeSurround to output the correct mapping?
    // FreeSurround:
    // FL | FC | FR | BL | BR | LFE
    // Most backends:
    // FL | FR | FC | LFE | BL | BR
    for (size_t i = 0; i < m_frame_block_size; ++i)
    {
      m_decoded_fifo.push(dpl2_fs[i * SURROUND_CHANNELS + 0]);  // LEFTFRONT
      m_decoded_fifo.push(dpl2_fs[i * SURROUND_CHANNELS + 2]);  // RIGHTFRONT
      m_decoded_fifo.push(dpl2_fs[i * SURROUND_CHANNELS + 1]);  // CENTREFRONT
      // The LFE channel is disabled in the surround decoder, as most people
      // have their own low pass crossover
      m_decoded_fifo.push(dpl2_fs[i * SURROUND_CHANNELS + 5]);  // sub/lfe
      m_decoded_fifo.push(dpl2_fs[i * SURROUND_CHANNELS + 3]);  // LEFTREAR
      m_decoded_fifo.push(dpl2_fs[i * SURROUND_CHANNELS + 4]);  // RIGHTREAR
    }

    remaining_samples = remaining_samples - static_cast<int>(m_frame_block_size);
    sample_index = sample_index + m_frame_block_size;
  }
}

void SurroundDecoder::GetDecodedSamples(float* out, const size_t num_samples)
{
  // Copy to output array with desired num_samples
  for (size_t i = 0, num_samples_output = num_samples * SURROUND_CHANNELS;
       i < num_samples_output; ++i)
  {
    out[i] = m_decoded_fifo.pop_front();
  }
}

}  // namespace AudioCommon
