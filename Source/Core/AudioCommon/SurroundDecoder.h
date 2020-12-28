// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "Common/CommonTypes.h"
#include "Common/FixedSizeQueue.h"

class DPL2FSDecoder;

namespace AudioCommon
{
// GC/Wii encodes sounds for DPLII, so we only support 5.1, anything more wouldn't be able to decode
// any additional information. Wii mote sounds will pass through here as well if enabled
class SurroundDecoder
{
public:
  explicit SurroundDecoder(u32 sample_rate);
  ~SurroundDecoder();
  // Can be used to re-initialize as well
  void Init(u32 sample_rate);
  // Receive and decode samples
  void PushSamples(const s16* in, u32 num_samples);
  void GetDecodedSamples(float* out, u32 num_samples);
  // Gives us back the samples which are still being computed, to not miss samples when
  // disabling DPLII
  u32 ReturnSamples(s16* out, u32 num_samples);
  bool CanReturnSamples() const;
  void Clear();

  // A DPLII decoder block is internally divided in 2
  static constexpr u32 DECODER_GRANULARITY = 2;
  static constexpr u32 STEREO_CHANNELS = 2;
  static constexpr u32 SURROUND_CHANNELS = 6;
  //To restore and review: this doesn't seem to always be checked correctly, it can break without warning over 100ms
  // Max supported block size (e.g. samples rate 192kHz at highest block quality, 100ms)
  static constexpr u32 MAX_BLOCKS_SIZE = 19200 * 48;
  // Max times our backend/mixer latency can be greater than our block size (e.g. 800ms at 192kHz)
  static constexpr u32 MAX_BLOCKS_BUFFERED = 8;

private:
  u32 m_sample_rate = 0;
  u32 m_block_size = 0;  // Doesn't count channels
  u32 m_returnable_samples_in_decoder = 0;  // Counts channels
  bool latency_warning_sent = false;

  std::unique_ptr<DPL2FSDecoder> m_fsdecoder;
  FixedSizeQueue<float, MAX_BLOCKS_SIZE * STEREO_CHANNELS> m_float_conversion_buffer;
  std::vector<float> m_float_conversion_buffer_copy; //To delete
  // This can end up being quite big, we could make a ptr and only allocate it when DPLII is used
  FixedSizeQueue<float, MAX_BLOCKS_SIZE * SURROUND_CHANNELS * MAX_BLOCKS_BUFFERED> m_decoded_fifo;
  float m_last_decoded_samples[SURROUND_CHANNELS] = {0.f};
};
}  // namespace AudioCommon
