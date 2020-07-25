// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>

#include "AudioCommon/Enums.h"
#include "Common/CommonTypes.h"
#include "Common/FixedSizeQueue.h"

class DPL2FSDecoder;

namespace AudioCommon
{
class SurroundDecoder
{
public:
  explicit SurroundDecoder(u32 sample_rate, DPL2Quality quality);
  ~SurroundDecoder();
  size_t QuerySamplesNeededForSurroundOutput(const size_t output_samples) const;
  void SetSampleRate(u32 sample_rate);
  void PushSamples(const s16* in, const size_t num_samples);
  void GetDecodedSamples(float* out, const size_t num_samples);
  void Clear();

private:
  u32 m_sample_rate;
  u32 m_frame_block_size;

  std::unique_ptr<DPL2FSDecoder> m_fsdecoder;
  std::array<float, 32768> m_float_conversion_buffer;
  FixedSizeQueue<float, 32768> m_decoded_fifo;
};

}  // namespace AudioCommon
