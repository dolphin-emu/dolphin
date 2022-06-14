// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <utility>

#include "Common/CommonTypes.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"

namespace DSP::HLE
{
class DSPHLE;

class ASndUCode final : public UCodeInterface
{
public:
  ASndUCode(DSPHLE* dsphle, u32 crc);

  void Initialize() override;
  void HandleMail(u32 mail) override;
  void Update() override;
  void DoState(PointerWrap& p) override;

private:
  void DMAInVoiceData();
  void DMAOutVoiceData();
  void DMAInSampleData();
  void DMAInSampleDataAssumeAligned();
  void ChangeBuffer();
  void DoMixing(u32 return_mail);

  std::pair<s16, s16> ReadSampleMono8Bits() const;
  std::pair<s16, s16> ReadSampleStereo8Bits() const;
  std::pair<s16, s16> ReadSampleMono16Bits() const;
  std::pair<s16, s16> ReadSampleStereo16Bits() const;
  std::pair<s16, s16> ReadSampleMono8BitsUnsigned() const;
  std::pair<s16, s16> ReadSampleMono16BitsLittleEndian() const;
  std::pair<s16, s16> ReadSampleStereo8BitsUnsigned() const;
  std::pair<s16, s16> ReadSampleStereo16BitsLittleEndian() const;

  // Copied from libasnd/asndlib.c's t_sound_data
  struct VoiceData
  {
    // output buffer 4096 bytes aligned to 32
    u32 out_buf;
    // samples per delay to start (48000 == 1sec)
    u32 delay_samples;
    // 2008/2009 versions: (step<<16) | (loop<<2) | (type & 3) used in DSP side
    // 2011/2020 versions: (step<<16) | (statuses<<8) | (type & 7) used in DSP side
    u32 flags;
    // internal addr counter
    u32 start_addr;
    // end voice physical pointer (bytes without alignament, but remember it reads in blocks of 32
    // bytes (use padding to the end))
    u32 end_addr;
    // freq operation
    u32 freq;
    // internally used to store de last sample played
    s16 left, right;
    // internally used to convert freq to 48000Hz samples
    u32 counter;
    // volume (from 0 to 256)
    u16 volume_l, volume_r;
    // initial voice2 physical pointer (bytes aligned 32 bytes) (to do a ring)
    u32 start_addr2;
    // end voice2 physical pointer (bytes without alignament, but remember it reads in blocks of 32
    // bytes (use padding to the end))
    u32 end_addr2;
    // volume (from 0 to 256) for voice 2
    u16 volume2_l, volume2_r;
    // initial voice physical pointer backup (bytes aligned to 32 bytes): It is used for test
    // pointers purpose
    u32 backup_addr;
    // voice tick counter - not used by DSP code
    u32 tick_counter;
    // callback - not used by DSP code
    u32 cb;
    u32 _pad;
  };
  static_assert(sizeof(VoiceData) == sizeof(u16) * 0x20);

  bool m_next_command_is_voice_addr = false;
  u32 m_voice_addr = 0;

  VoiceData m_current_voice{};

  // Number of bytes in the input sample buffer.
  static constexpr u32 INPUT_SAMPLE_BUFFER_SIZE_BYTES = 0x20;
  // Mask used for addresses for the sample buffer - note that the sample buffer is also assumed to
  // be 0x20-aligned in main memory.
  static constexpr u32 INPUT_SAMPLE_BUFFER_BYTE_MASK = INPUT_SAMPLE_BUFFER_SIZE_BYTES - 1;
  // The DSP itself operates on 16-bit words instead of individual bytes, meaning the size is 0x10.
  static constexpr u32 INPUT_SAMPLE_BUFFER_SIZE_WORDS = INPUT_SAMPLE_BUFFER_SIZE_BYTES / 2;
  // ... and thus uses a different mask of 0x0f.
  static constexpr u32 INPUT_SAMPLE_BUFFER_WORD_MASK = INPUT_SAMPLE_BUFFER_SIZE_WORDS - 1;
  // Lastly, the uCode actually allocates 0x20 words, not 0x10 words, for the sample buffer,
  // but only the first 0x10 words are used except for an edge-case where they mask with 0xf and
  // then add 1 in ReadSampleStereo16Bits and ReadSampleStereo16BitsLittleEndian - this results in
  // index 0x10 possibly being read, which is otherwise unused (but this probably doesn't happen in
  // practice, as it would require the buffer address starting at 2 instead of 0). We use the same
  // oversized buffer to accurately emulate this behavior without actually reading invalid memory.
  static constexpr u32 INPUT_SAMPLE_BUFFER_SIZE_WORDS_ACTUAL = INPUT_SAMPLE_BUFFER_SIZE_WORDS * 2;

  std::array<u16, INPUT_SAMPLE_BUFFER_SIZE_WORDS_ACTUAL> m_input_sample_buffer{};

  // Number of 16-bit stereo samples in the output buffer
  static constexpr u32 NUM_OUTPUT_SAMPLES = 1024;

  std::array<u16, NUM_OUTPUT_SAMPLES * 2> m_output_buffer{};
};
}  // namespace DSP::HLE
