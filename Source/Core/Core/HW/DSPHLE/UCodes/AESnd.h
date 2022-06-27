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

class AESndUCode final : public UCodeInterface
{
public:
  AESndUCode(DSPHLE* dsphle, u32 crc);

  void Initialize() override;
  void HandleMail(u32 mail) override;
  void Update() override;
  void DoState(PointerWrap& p) override;

private:
  void DMAInParameterBlock();
  void DMAOutParameterBlock();
  void SetUpAccelerator(u16 format, u16 gain);
  void DoMixing();

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
};
}  // namespace DSP::HLE
