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

  // November 14, 2008 version (padded to 0x05a0 bytes) - initial release
  // https://github.com/devkitPro/libogc/compare/c76d8b851fafc11b0a5debc0b40842929d5a5825~...353a44f038e75e5982eb550173ec8127ab35e3e3
  static constexpr u32 HASH_2008 = 0x8d69a19b;
  // February 5, 2009 version (padded to 0x05c0 bytes) - added MAIL_TERMINATE
  // https://github.com/devkitPro/libogc/compare/1925217ffb4c97cbee5cf21fa3c0231029b340e2~...3b1f018dbe372859a43bff8560e2525f6efa4433
  static constexpr u32 HASH_2009 = 0xcc2fd441;
  // June 11, 2011 version (padded to 0x0620 bytes) - added new sample formats, which shifted flags
  // Note that the source include in the repo does not match the compiled binary exactly; the
  // compiled version differs by using asl instead of lsl, $acc1 instead of $acc0, and $ac0.l
  // instead of $ac0.m in various locations, as well as having the "jmp out_samp" line uncommented
  // in stereo_16bits_le. None of these result in a behavior difference, from the source, though.
  // Note that gcdsptool was also updated, which results in some differences in the source that
  // don't actually correspond to different instructions (e.g. s40 was renamed to s16)
  // https://github.com/devkitPro/libogc/commit/b1b8ecab3af3745c8df0b401abd512bdf5fcc011
  static constexpr u32 HASH_2011 = 0xa81582e2;
  // June 12, 2020 version (0x0606 bytes) - libogc switched to compiling the ucode at build time
  // instead of including a pre-compiled version in the repo, so this now corresponds to the code
  // provided in the repo. There appear to be no behavior differences from the 2011 version.
  // https://github.com/devkitPro/libogc/compare/bfb705fe1607a3031d18b65d603975b68a1cffd4~...d20f9bdcfb43260c6c759f4fb98d724931443f93
  static constexpr u32 HASH_2020 = 0xdbbeeb61;
  // Variant of the above used in libogc-rice and libogc2 starting on December 11, 2020 and padded
  // to 0x0620 bytes. These forks have gcdsptool generate a header file instead of a bin file
  // (followed by bin2o), so padding is still applied (for libogc-rice, the header is manually
  // generated, while libogc2 generates it as part of the build process).
  // https://github.com/extremscorner/libogc2/commit/80e01cbd8ead0370d98e092b426f851f21175e60
  // https://github.com/extremscorner/libogc2/commit/0b64f879808953d80ba06501a1c079b0fbf017d2
  // https://github.com/extremscorner/libogc-rice/commit/ce22c3269699fdbd474f2f28ca2ffca211954659
  static constexpr u32 HASH_2020_PAD = 0xbad876ef;
  // Variant used in Desert Bus v1.04 - this is based off of the code in libogc (as it existed in
  // 2011, even though that code only became used in 2020), but the left and right channels are
  // swapped (with the left channel coming before the right channel, which is the the conventional
  // behavior). Padded to 0x0620 bytes.
  static constexpr u32 HASH_DESERT_BUS_2011 = 0xfa9c576f;
  // Variant used in Desert Bus v1.05 - this is the same as the previous version, except 4 junk
  // instructions were added to the start, which do not change behavior in any way. Padded to 0x0620
  // bytes.
  static constexpr u32 HASH_DESERT_BUS_2012 = 0x614dd145;
  // March 22, 2024 version (0x0606 bytes) - libogc fixed left and right channels being reversed,
  // which apparently has been the case from the start but was not obvious in earlier testing
  // because of the oggplayer sample using a mono sound file.
  // https://github.com/devkitPro/libogc/commit/a0b4b5680944ee7c2ae1b7af63a721623c1a6b69
  static constexpr u32 HASH_2024 = 0x5dbf8bf1;
  // March 22, 2024 version (padded to 0x0620 bytes) - same as above, but padded as it's used by
  // libogc2 and libogc-rice.
  // https://github.com/extremscorner/libogc2/commit/f3fd10635d4b3fbc6ee03cec335eeb2a2237fd56
  // https://github.com/extremscorner/libogc-rice/commit/5ebbf8b96d7433bc2af9e882f730e67a5eb20f00
  static constexpr u32 HASH_2024_PAD = 0x373a950e;

private:
  void DMAInVoiceData();
  void DMAOutVoiceData();
  void DMAInSampleData();
  void DMAInSampleDataAssumeAligned();
  void ChangeBuffer();
  void DoMixing(u32 return_mail);

  bool SwapLeftRight() const;
  bool UseNewFlagMasks() const;

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

  bool m_next_mail_is_voice_addr = false;
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
