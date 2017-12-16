// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

class PointerWrap;

namespace DSP
{
class Accelerator
{
public:
  virtual ~Accelerator() = default;

  u16 Read(s16* coefs);
  // Zelda ucode reads ARAM through 0xffd3.
  u16 ReadD3();
  void WriteD3(u16 value);

  u32 GetStartAddress() const { return m_start_address; }
  u32 GetEndAddress() const { return m_end_address; }
  u32 GetCurrentAddress() const { return m_current_address; }
  u16 GetSampleFormat() const { return m_sample_format; }
  s16 GetYn1() const { return m_yn1; }
  s16 GetYn2() const { return m_yn2; }
  u16 GetPredScale() const { return m_pred_scale; }
  void SetStartAddress(u32 address);
  void SetEndAddress(u32 address);
  void SetCurrentAddress(u32 address);
  void SetSampleFormat(u16 format);
  void SetYn1(s16 yn1);
  void SetYn2(s16 yn2);
  void SetPredScale(u16 pred_scale);

  void DoState(PointerWrap& p);

protected:
  virtual void OnEndException() = 0;
  virtual u8 ReadMemory(u32 address) = 0;
  virtual void WriteMemory(u32 address, u8 value) = 0;

  // DSP accelerator registers.
  u32 m_start_address = 0;
  u32 m_end_address = 0;
  u32 m_current_address = 0;
  u16 m_sample_format = 0;
  s16 m_yn1 = 0;
  s16 m_yn2 = 0;
  u16 m_pred_scale = 0;

  // When an ACCOV is triggered, the accelerator stops reading back anything
  // and updating the current address register, unless the YN2 register is written to.
  // This is kept track of internally; this state is not exposed via any register.
  bool m_reads_stopped = false;
};
}  // namespace DSP
