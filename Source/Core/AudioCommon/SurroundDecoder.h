// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>

#include "Common/CommonTypes.h"
#include "Common/FixedSizeQueue.h"

class DPL2FSDecoder;

namespace AudioCommon
{
class SurroundDecoder
{
public:
  explicit SurroundDecoder(u32 sample_rate);
  ~SurroundDecoder();
  u32 QuerySamplesNeededForSurroundOutput(u32 output_samples) const;
  void InitAndSetSampleRate(u32 sample_rate);
  void PushSamples(const s16* in, u32 num_samples);
  void GetDecodedSamples(float* out, u32 num_samples);
  void Clear();

private:
  u32 m_sample_rate = 0;
  u32 m_frame_block_size;

  static constexpr u32 STEREO_CHANNELS = 2;
  static constexpr u32 SURROUND_CHANNELS = 6;
  // Max supported samples rate is about 192kHz at highest block quality (~80ms)
  static constexpr u32 MAX_BLOCKS_SIZE = 16384;
  static constexpr u32 MAX_BLOCKS_BUFFERED = 8;

  std::unique_ptr<DPL2FSDecoder> m_fsdecoder;
  std::array<float, MAX_BLOCKS_SIZE * STEREO_CHANNELS> m_float_conversion_buffer;
  FixedSizeQueue<float, MAX_BLOCKS_SIZE * SURROUND_CHANNELS * MAX_BLOCKS_BUFFERED> m_decoded_fifo;
};

}  // namespace AudioCommon
