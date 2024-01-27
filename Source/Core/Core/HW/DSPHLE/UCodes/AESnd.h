// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <utility>

#include "Common/CommonTypes.h"
#include "Core/DSP/DSPAccelerator.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"

namespace DSP
{
class DSPManager;
}

namespace DSP::HLE
{
class DSPHLE;

class AESndAccelerator final : public Accelerator
{
public:
  explicit AESndAccelerator(DSP::DSPManager& dsp);
  AESndAccelerator(const AESndAccelerator&) = delete;
  AESndAccelerator(AESndAccelerator&&) = delete;
  AESndAccelerator& operator=(const AESndAccelerator&) = delete;
  AESndAccelerator& operator=(AESndAccelerator&&) = delete;
  ~AESndAccelerator();

protected:
  void OnEndException() override;
  u8 ReadMemory(u32 address) override;
  void WriteMemory(u32 address, u8 value) override;

private:
  DSP::DSPManager& m_dsp;
};

class AESndUCode final : public UCodeInterface
{
public:
  AESndUCode(DSPHLE* dsphle, u32 crc);

  void Initialize() override;
  void HandleMail(u32 mail) override;
  void Update() override;
  void DoState(PointerWrap& p) override;

  // June 5, 2010 version (padded to 0x03e0 bytes) - initial release
  // First included with libogc 1.8.4 on October 3, 2010: https://devkitpro.org/viewtopic.php?t=2249
  // https://github.com/devkitPro/libogc/blob/b5fdbdb069c45584aa4dfd950a136a8db9b1144c/libaesnd/dspcode/dspmixer.s
  static constexpr u32 HASH_2010 = 0x008366af;
  // April 11, 2012 version (padded to 0x03e0 bytes) - swapped input channels
  // First included with libogc 1.8.11 on April 22, 2012: https://devkitpro.org/viewtopic.php?t=3094
  // https://github.com/devkitPro/libogc/commit/8f188e12b6a3d8b5a0d49a109fe6a3e4e1702aab
  static constexpr u32 HASH_2012 = 0x078066ab;
  // Modified version used by EDuke32 Wii (padded to 0x03e0 bytes) - added unsigned 8-bit formats;
  // broke the mono formats. The patch is based on the 2010 version, but it also includes the 2012
  // input channel swap change. https://dukeworld.duke4.net/eduke32/wii/library_source_code/
  static constexpr u32 HASH_EDUKE32 = 0x5ad4d933;
  // June 14, 2020 version (0x03e6 bytes) - added unsigned formats
  // First included with libogc 2.1.0 on June 15, 2020: https://devkitpro.org/viewtopic.php?t=9079
  // https://github.com/devkitPro/libogc/commit/eac8fe2c29aa790d552dd6166a1fb195dfdcb825
  static constexpr u32 HASH_2020 = 0x84c680a9;
  // Padded version of the above (0x0400 bytes), added to libogc-rice on June 16, 2012 (that's the
  // commit author date; the commit date is November 24, 2016) and libogc2 on 25 May 2020. Used by
  // Not64 and OpenTTD (starting with the December 1, 2012 release).
  // https://github.com/extremscorner/libogc-rice/commit/cfddd4f3bec77812d6d333954e39d401d2276cd8
  // https://github.com/extremscorner/libogc2/commit/89ae39544e22f720a9c986af3524f7e6f20e7293
  static constexpr u32 HASH_2020_PAD = 0xa02a6131;
  // July 19, 2022 version (padded to 0x0400 bytes) - fixed MAIL_TERMINATE. This padded version
  // is only in libogc2 and libogc-rice (which generate a padded header file).
  // https://github.com/extremscorner/libogc2/commit/38edc9db93232faa612f680c91be1eb4d95dd1c6
  static constexpr u32 HASH_2022_PAD = 0x2e5e4100;
  // March 13, 2023 version (0x03e8 bytes) - fixed MAIL_TERMINATE. This is the same fix as the
  // above version, and was released in regular libogc 2.4.0 on April 17, 2023.
  // https://github.com/devkitPro/libogc/commit/a7e4bcd3ad4477d8dfc3aa196cfeb10cf195cd6a
  static constexpr u32 HASH_2023 = 0x002e5e41;

private:
  void DMAInParameterBlock();
  void DMAOutParameterBlock();
  void SetUpAccelerator(u16 format, u16 gain);
  void DoMixing();

  bool SwapLeftRight() const;
  bool UseNewFlagMasks() const;

  // Copied from libaesnd/aesndlib.c's aesndpb_t (specifically the first 64 bytes)
#pragma pack(1)
  struct ParameterBlock
  {
    u32 out_buf;

    u32 buf_start;
    u32 buf_end;
    u32 buf_curr;

    u16 yn1;
    u16 yn2;
    u16 pds;

    // Note: only u16-aligned, not u32-aligned.
    // libogc's version has separate u16 freq_l and freq_h fields, but we use #pragma pack(1).
    u32 freq;
    u16 counter;

    s16 left;
    s16 right;
    u16 volume_l;
    u16 volume_r;

    u32 delay;

    u32 flags;
    u8 _pad[20];
  };
#pragma pack()
  static_assert(sizeof(ParameterBlock) == sizeof(u16) * 0x20);

  bool m_next_mail_is_parameter_block_addr = false;
  u32 m_parameter_block_addr = 0;

  ParameterBlock m_parameter_block{};

  // Number of 16-bit stereo samples in the output buffer: 2ms of sample data
  static constexpr u32 NUM_OUTPUT_SAMPLES = 96;

  std::array<s16, NUM_OUTPUT_SAMPLES * 2> m_output_buffer{};

  AESndAccelerator m_accelerator;

  bool m_has_shown_unsupported_sample_format_warning = false;
};
}  // namespace DSP::HLE
