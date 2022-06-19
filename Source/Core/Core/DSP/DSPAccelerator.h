// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/BitField.h"
#include "Common/CommonTypes.h"

class PointerWrap;

namespace DSP
{
class Accelerator
{
public:
  virtual ~Accelerator() = default;

  u16 ReadSample(const s16* coefs);
  // Zelda ucode reads ARAM through 0xffd3.
  u16 ReadRaw();
  void WriteRaw(u16 value);

  u32 GetStartAddress() const { return m_start_address; }
  u32 GetEndAddress() const { return m_end_address; }
  u32 GetCurrentAddress() const { return m_current_address; }
  u16 GetSampleFormat() const { return m_sample_format.hex; }
  s16 GetGain() const { return m_gain; }
  s16 GetYn1() const { return m_yn1; }
  s16 GetYn2() const { return m_yn2; }
  u16 GetPredScale() const { return m_pred_scale; }
  void SetStartAddress(u32 address);
  void SetEndAddress(u32 address);
  void SetCurrentAddress(u32 address);
  void SetSampleFormat(u16 format);
  void SetGain(s16 gain);
  void SetYn1(s16 yn1);
  void SetYn2(s16 yn2);
  void SetPredScale(u16 pred_scale);

  void DoState(PointerWrap& p);

protected:
  virtual void OnRawReadEndException() = 0;
  virtual void OnRawWriteEndException() = 0;
  virtual void OnSampleReadEndException() = 0;
  virtual u8 ReadMemory(u32 address) = 0;
  virtual void WriteMemory(u32 address, u8 value) = 0;
  u16 GetCurrentSample();

  // DSP accelerator registers.
  u32 m_start_address = 0;
  u32 m_end_address = 0;
  u32 m_current_address = 0;

  enum class FormatSize : u16
  {
    Size4Bit = 0,
    Size8Bit = 1,
    Size16Bit = 2,
    SizeInvalid = 3
  };

  enum class FormatDecode : u16
  {
    ADPCM = 0,
    PCM = 1,
  };

  // When reading samples (at least in PCM mode), they are multiplied by the gain, then shifted
  // right by an amount dependent on this config
  enum class FormatGainCfg : u16
  {
    GainShift11 = 0,
    GainShift0 = 1,
    GainShift16 = 2,
    GainInvalid = 3
  };

  union SampleFormat
  {
    u16 hex;
    BitField<0, 2, FormatSize> size;
    BitField<2, 1, bool, u16>
        raw_only;  // When this bit is set, sample reads seem broken, while raw accesses work
    BitField<3, 1, FormatDecode> decode;
    BitField<4, 2, FormatGainCfg> gain_cfg;
    BitField<6, 10, u16> unk;
  } m_sample_format{0};

  s16 m_gain = 0;
  s16 m_yn1 = 0;
  s16 m_yn2 = 0;
  u16 m_pred_scale = 0;

  // When an ACCOV is triggered, the accelerator stops reading back anything
  // and updating the current address register, unless the YN2 register is written to.
  // This is kept track of internally; this state is not exposed via any register.
  bool m_reads_stopped = false;
};
}  // namespace DSP
