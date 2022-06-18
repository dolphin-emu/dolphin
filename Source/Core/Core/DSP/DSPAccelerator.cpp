// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/DSP/DSPAccelerator.h"

#include <algorithm>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

namespace DSP
{
u16 Accelerator::GetCurrentSample()
{
  u16 val = 0;
  // The lower two bits of the sample format indicate the access size
  switch (m_sample_format.size)
  {
  case FormatSize::Size4Bit:
    val = ReadMemory(m_current_address >> 1);
    if (m_current_address & 1)
      val &= 0xf;
    else
      val >>= 4;
    break;
  case FormatSize::Size8Bit:
    val = ReadMemory(m_current_address);
    break;
  case FormatSize::Size16Bit:
    val = (ReadMemory(m_current_address * 2) << 8) | ReadMemory(m_current_address * 2 + 1);
    break;
  default:  // produces garbage, but affects the current address
    ERROR_LOG_FMT(DSPLLE, "dsp_get_current_sample() - bad format {:#x}", m_sample_format.hex);
    break;
  }
  return val;
}

u16 Accelerator::ReadRaw()
{
  u16 val = GetCurrentSample();
  if (m_sample_format.size != FormatSize::SizeInvalid)
  {
    m_current_address++;
  }
  else
  {
    m_current_address = (m_current_address & ~3) | ((m_current_address + 1) & 3);
  }

  // There are edge cases that are currently not handled here in u4 and u8 mode
  // In u8 mode, if ea & 1 == 0 and ca == ea + 1, the accelerator can be read twice. Upon the second
  // read, the data returned appears to be the other half of the u16 from the first read, and the
  // DSP resets the current address and throws exception 3

  // During these reads, ca is not incremented normally.

  // Instead, incrementing ca works like this: ca = (ca & ~ 3) | ((ca + 1) & 3)
  // u4 mode extends this further.

  // When ea & 3 == 0, and ca in [ea + 1, ea + 3], the accelerator can be read (4 - (ca - ea - 1))
  // times. On the last read, the data returned appears to be the remaining nibble of the u16 from
  // the first read, and the DSP resets the current address and throws exception 3

  // When ea & 3 == 1, and ca in [ea + 1, ea + 2], the accelerator can be read (4 - (ca - ea - 1))
  // times. On the last read, the data returned appears to be the remaining nibble of the u16 from
  // the first read, and the DSP resets the current address and throws exception 3

  // When ea & 3 == 2, and ca == ea + 1, the accelerator can be read 4 times. On the last read, the
  // data returned appears to be the remaining nibble of the u16 from the first read, and the DSP
  // resets the current address and throws exception 3

  // There are extra extra edge cases if ca, ea, and potentially other registers are adjusted during
  // this pre-reset phase

  // The cleanest way to emulate the normal non-edge behavior is to only reset things if we just
  // read the end address If the current address is larger than the end address (and not in the edge
  // range), it ignores the end address
  if (m_current_address - 1 == m_end_address)
  {
    // Set address back to start address (confirmed on hardware)
    m_current_address = m_start_address;
    OnEndException();
  }

  SetCurrentAddress(m_current_address);
  return val;
}

void Accelerator::WriteRaw(u16 value)
{
  // Zelda ucode writes a bunch of zeros to ARAM through d3 during
  // initialization.  Don't know if it ever does it later, too.
  // Pikmin 2 Wii writes non-stop to 0x10008000-0x1000801f (non-zero values too)
  // Zelda TP Wii writes non-stop to 0x10000000-0x1000001f (non-zero values too)

  // Writes only seem to be accepted when the upper most bit of the address is set
  if (m_current_address & 0x80000000)
  {
    // The format doesn't matter for raw writes, all writes are u16 and the address is treated as if
    // we are in a 16-bit format
    WriteMemory(m_current_address * 2, value >> 8);
    WriteMemory(m_current_address * 2 + 1, value & 0xFF);
    m_current_address++;
  }
  else
  {
    ERROR_LOG_FMT(DSPLLE,
                  "dsp_write_aram_raw() - tried to write to address {:#x} without high bit set",
                  m_current_address);
  }
}

u16 Accelerator::ReadSample(const s16* coefs)
{
  if (m_reads_stopped)
    return 0x0000;

  if (m_sample_format.raw_only)
  {
    // Seems to return garbage on hardware
    ERROR_LOG_FMT(
        DSPLLE,
        "dsp_read_accelerator_sample() tried sample read with raw only bit set for format {:#x}",
        m_sample_format.hex);
    return 0x0000;
  }

  if (m_sample_format.unk != 0)
  {
    WARN_LOG_FMT(DSPLLE, "dsp_read_accelerator_sample() format {:#x} has unknown upper bits set",
                 m_sample_format.hex);
  }

  u16 val = 0;
  u8 step_size = 0;

  s16 raw_sample = GetCurrentSample();
  int coef_idx = (m_pred_scale >> 4) & 0x7;

  s32 coef1 = coefs[coef_idx * 2 + 0];
  s32 coef2 = coefs[coef_idx * 2 + 1];

  u8 gain_shift = 0;
  switch (m_sample_format.gain_cfg)
  {
  case FormatGainCfg::GainShift11:
    gain_shift = 11;
    break;
  case FormatGainCfg::GainShift0:
    gain_shift = 0;
    break;
  case FormatGainCfg::GainShift16:
    gain_shift = 16;
    break;
  default:
    ERROR_LOG_FMT(DSPLLE, "dsp_read_accelerator_sample() invalid gain mode in format {:#x}",
                  m_sample_format.hex);
    break;
  }

  switch (m_sample_format.decode)
  {
  case FormatDecode::ADPCM:  // ADPCM audio
  {
    if (m_sample_format.size != FormatSize::Size4Bit)
    {
      ERROR_LOG_FMT(
          DSPLLE, "dsp_read_accelerator_sample() tried to read ADPCM with bad size in format {:#x}",
          m_sample_format.hex);
      break;
    }
    int scale = 1 << (m_pred_scale & 0xF);

    if (raw_sample >= 8)
      raw_sample -= 16;

    // Not sure if GAIN is applied for ADPCM or not
    s32 val32 = (scale * raw_sample) + ((0x400 + coef1 * m_yn1 + coef2 * m_yn2) >> 11);
    val = static_cast<s16>(std::clamp<s32>(val32, -0x7FFF, 0x7FFF));
    step_size = 2;

    m_yn2 = m_yn1;
    m_yn1 = val;
    m_current_address += 1;

    // These two cases are handled in a special way, separate from normal overflow handling:
    // the ACCOV exception does not fire at all, the predscale register is not updated,
    // and if the end address is 16-byte aligned, the DSP loops to start_address + 1
    // instead of start_address.
    if ((m_end_address & 0xf) == 0x0 && m_current_address == m_end_address)
    {
      m_current_address = m_start_address + 1;
    }
    else if ((m_end_address & 0xf) == 0x1 && m_current_address == m_end_address - 1)
    {
      m_current_address = m_start_address;
    }
    // If any of these special cases were hit, the DSP does not update the predscale register.
    else if ((m_current_address & 15) == 0)
    {
      m_pred_scale = ReadMemory((m_current_address & ~15) >> 1);
      m_current_address += 2;
      step_size += 2;
    }
    break;
  }
  case FormatDecode::PCM:  // 16-bit PCM audio
  {
    s32 val32 = ((static_cast<s32>(m_gain) * raw_sample) >> gain_shift) +
                (((coef1 * m_yn1) >> gain_shift) + ((coef2 * m_yn2) >> gain_shift));
    val = static_cast<s16>(val32);
    m_yn2 = m_yn1;
    m_yn1 = val;
    step_size = 2;
    m_current_address += 1;
    break;
  }
  }

  // Check for loop.
  // Somehow, YN1 and YN2 must be initialized with their "loop" values,
  // so yeah, it seems likely that we should raise an exception to let
  // the DSP program do that, at least if DSP_FORMAT == 0x0A.
  if (m_current_address == (m_end_address + step_size - 1))
  {
    // Set address back to start address.
    m_current_address = m_start_address;
    m_reads_stopped = true;
    OnEndException();
  }

  SetCurrentAddress(m_current_address);
  return val;
}

void Accelerator::DoState(PointerWrap& p)
{
  p.Do(m_start_address);
  p.Do(m_end_address);
  p.Do(m_current_address);
  p.Do(m_sample_format);
  p.Do(m_yn1);
  p.Do(m_yn2);
  p.Do(m_pred_scale);
  p.Do(m_reads_stopped);
}

constexpr u32 START_END_ADDRESS_MASK = 0x3fffffff;
constexpr u32 CURRENT_ADDRESS_MASK = 0xbfffffff;

void Accelerator::SetStartAddress(u32 address)
{
  m_start_address = address & START_END_ADDRESS_MASK;
}

void Accelerator::SetEndAddress(u32 address)
{
  m_end_address = address & START_END_ADDRESS_MASK;
}

void Accelerator::SetCurrentAddress(u32 address)
{
  m_current_address = address & CURRENT_ADDRESS_MASK;
}

void Accelerator::SetSampleFormat(u16 format)
{
  m_sample_format.hex = format;
}

void Accelerator::SetGain(s16 gain)
{
  m_gain = gain;
}

void Accelerator::SetYn1(s16 yn1)
{
  m_yn1 = yn1;
}

void Accelerator::SetYn2(s16 yn2)
{
  m_yn2 = yn2;
  m_reads_stopped = false;
}

void Accelerator::SetPredScale(u16 pred_scale)
{
  m_pred_scale = pred_scale & 0x7f;
}
}  // namespace DSP
