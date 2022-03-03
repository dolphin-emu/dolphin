// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioCommon/SurroundDecoder.h"

#include <FreeSurround/FreeSurroundDecoder.h>
#include <limits>

namespace AudioCommon
{
constexpr size_t STEREO_CHANNELS = 2;
constexpr size_t SURROUND_CHANNELS = 6;

SurroundDecoder::SurroundDecoder(u32 sample_rate, u32 frame_block_size)
    : m_sample_rate(sample_rate), m_frame_block_size(frame_block_size)
{
  m_fsdecoder = std::make_unique<DPL2FSDecoder>();
  m_fsdecoder->Init(cs_5point1, m_frame_block_size, m_sample_rate);
}

SurroundDecoder::~SurroundDecoder() = default;

void SurroundDecoder::Clear()
{
  m_fsdecoder->flush();
  m_decoded_fifo.clear();
}

// Currently only 6 channels are supported.
size_t SurroundDecoder::QueryFramesNeededForSurroundOutput(const size_t output_frames) const
{
  if (m_decoded_fifo.size() < output_frames * SURROUND_CHANNELS)
  {
    // Output stereo frames needed to have at least the desired number of surround frames
    size_t frames_needed = output_frames - m_decoded_fifo.size() / SURROUND_CHANNELS;
    return frames_needed + m_frame_block_size - frames_needed % m_frame_block_size;
  }

  return 0;
}

// Receive and decode samples
void SurroundDecoder::PutFrames(const short* in, const size_t num_frames_in)
{
  // Maybe check if it is really power-of-2?
  s64 remaining_frames = static_cast<s64>(num_frames_in);
  size_t frame_index = 0;

  while (remaining_frames > 0)
  {
    // Convert to float
    for (size_t i = 0, end = m_frame_block_size * STEREO_CHANNELS; i < end; ++i)
    {
      m_float_conversion_buffer[i] = in[i + frame_index * STEREO_CHANNELS] /
                                     static_cast<float>(std::numeric_limits<short>::max());
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
      m_decoded_fifo.push(dpl2_fs[i * SURROUND_CHANNELS + 5]);  // sub/lfe
      m_decoded_fifo.push(dpl2_fs[i * SURROUND_CHANNELS + 3]);  // LEFTREAR
      m_decoded_fifo.push(dpl2_fs[i * SURROUND_CHANNELS + 4]);  // RIGHTREAR
    }

    remaining_frames = remaining_frames - static_cast<int>(m_frame_block_size);
    frame_index = frame_index + m_frame_block_size;
  }
}

void SurroundDecoder::ReceiveFrames(float* out, const size_t num_frames_out)
{
  // Copy to output array with desired num_frames_out
  for (size_t i = 0, num_samples_output = num_frames_out * SURROUND_CHANNELS;
       i < num_samples_output; ++i)
  {
    out[i] = m_decoded_fifo.pop_front();
  }
}

}  // namespace AudioCommon
