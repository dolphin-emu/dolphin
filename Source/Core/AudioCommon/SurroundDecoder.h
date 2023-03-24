// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
  explicit SurroundDecoder(u32 sample_rate, u32 frame_block_size);
  ~SurroundDecoder();
  size_t QueryFramesNeededForSurroundOutput(const size_t output_frames) const;
  void PutFrames(const short* in, const size_t num_frames_in);
  void ReceiveFrames(float* out, const size_t num_frames_out);
  void Clear();

private:
  u32 m_sample_rate;
  u32 m_frame_block_size;

  std::unique_ptr<DPL2FSDecoder> m_fsdecoder;
  std::array<float, 32768> m_float_conversion_buffer;
  Common::FixedSizeQueue<float, 32768> m_decoded_fifo;
};

}  // namespace AudioCommon
