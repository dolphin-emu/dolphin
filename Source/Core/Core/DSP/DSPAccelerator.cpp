// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/DSP/DSPAccelerator.h"

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"

#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPHWInterface.h"
#include "Core/DSP/DSPHost.h"

namespace DSP
{
u16 dsp_read_aram_d3()
{
  // Zelda ucode reads ARAM through 0xffd3.
  const u32 EndAddress = (g_dsp.ifx_regs[DSP_ACEAH] << 16) | g_dsp.ifx_regs[DSP_ACEAL];
  u32 Address = (g_dsp.ifx_regs[DSP_ACCAH] << 16) | g_dsp.ifx_regs[DSP_ACCAL];
  u16 val = 0;

  switch (g_dsp.ifx_regs[DSP_FORMAT])
  {
  case 0x5:  // u8 reads
    val = Host::ReadHostMemory(Address);
    Address++;
    break;
  case 0x6:  // u16 reads
    val = (Host::ReadHostMemory(Address * 2) << 8) | Host::ReadHostMemory(Address * 2 + 1);
    Address++;
    break;
  default:
    ERROR_LOG(DSPLLE, "dsp_read_aram_d3() - unknown format 0x%x", g_dsp.ifx_regs[DSP_FORMAT]);
    break;
  }

  if (Address >= EndAddress)
  {
    // Set address back to start address. (never seen this here!)
    Address = (g_dsp.ifx_regs[DSP_ACSAH] << 16) | g_dsp.ifx_regs[DSP_ACSAL];
  }

  g_dsp.ifx_regs[DSP_ACCAH] = Address >> 16;
  g_dsp.ifx_regs[DSP_ACCAL] = Address & 0xffff;
  return val;
}

void dsp_write_aram_d3(u16 value)
{
  // Zelda ucode writes a bunch of zeros to ARAM through d3 during
  // initialization.  Don't know if it ever does it later, too.
  // Pikmin 2 Wii writes non-stop to 0x10008000-0x1000801f (non-zero values too)
  // Zelda TP Wii writes non-stop to 0x10000000-0x1000001f (non-zero values too)
  u32 Address = (g_dsp.ifx_regs[DSP_ACCAH] << 16) | g_dsp.ifx_regs[DSP_ACCAL];

  switch (g_dsp.ifx_regs[DSP_FORMAT])
  {
  case 0xA:  // u16 writes
    Host::WriteHostMemory(value >> 8, Address * 2);
    Host::WriteHostMemory(value & 0xFF, Address * 2 + 1);
    Address++;
    break;
  default:
    ERROR_LOG(DSPLLE, "dsp_write_aram_d3() - unknown format 0x%x", g_dsp.ifx_regs[DSP_FORMAT]);
    break;
  }

  g_dsp.ifx_regs[DSP_ACCAH] = Address >> 16;
  g_dsp.ifx_regs[DSP_ACCAL] = Address & 0xffff;
}

u16 ReadAccelerator(u32 start_address, u32 end_address, u32* current_address, u16 sample_format,
                    s16* yn1, s16* yn2, u16* pred_scale, s16* coefs,
                    std::function<void()> end_exception)
{
  u16 val;
  u8 step_size_bytes = 0;

  // let's do the "hardware" decode DSP_FORMAT is interesting - the Zelda
  // ucode seems to indicate that the bottom two bits specify the "read size"
  // and the address multiplier.  The bits above that may be things like sign
  // extension and do/do not use ADPCM.  It also remains to be figured out
  // whether there's a difference between the usual accelerator "read
  // address" and 0xd3.
  switch (sample_format)
  {
  case 0x00:  // ADPCM audio
  {
    // ADPCM decoding, not much to explain here.
    if ((*current_address & 15) == 0)
    {
      *pred_scale = Host::ReadHostMemory((*current_address & ~15) >> 1);
      *current_address += 2;
    }

    switch (end_address & 15)
    {
    case 0:  // Tom and Jerry
      step_size_bytes = 1;
      break;
    case 1:  // Blazing Angels
      step_size_bytes = 0;
      break;
    default:
      step_size_bytes = 2;
      break;
    }

    int scale = 1 << (*pred_scale & 0xF);
    int coef_idx = (*pred_scale >> 4) & 0x7;

    s32 coef1 = coefs[coef_idx * 2 + 0];
    s32 coef2 = coefs[coef_idx * 2 + 1];

    int temp = (*current_address & 1) ? (Host::ReadHostMemory(*current_address >> 1) & 0xF) :
                                        (Host::ReadHostMemory(*current_address >> 1) >> 4);

    if (temp >= 8)
      temp -= 16;

    val = (scale * temp) + ((0x400 + coef1 * *yn1 + coef2 * *yn2) >> 11);
    val = MathUtil::Clamp<s16>(val, -0x7FFF, 0x7FFF);

    *yn2 = *yn1;
    *yn1 = val;
    *current_address += 1;
    break;
  }
  case 0x0A:  // 16-bit PCM audio
    val = (Host::ReadHostMemory(*current_address * 2) << 8) |
          Host::ReadHostMemory(*current_address * 2 + 1);
    *yn2 = *yn1;
    *yn1 = val;
    step_size_bytes = 2;
    *current_address += 1;
    break;
  case 0x19:  // 8-bit PCM audio
    val = Host::ReadHostMemory(*current_address) << 8;
    *yn2 = *yn1;
    *yn1 = val;
    step_size_bytes = 2;
    *current_address += 1;
    break;
  default:
    ERROR_LOG(DSPLLE, "dsp_read_accelerator() - unknown format 0x%x", g_dsp.ifx_regs[DSP_FORMAT]);
    step_size_bytes = 2;
    *current_address += 1;
    val = 0;
    break;
  }

  // TODO: Take GAIN into account
  // adpcm = 0, pcm8 = 0x100, pcm16 = 0x800
  // games using pcm8 : Phoenix Wright Ace Attorney (WiiWare), Megaman 9-10 (WiiWare)
  // games using pcm16: GC Sega games, ...

  // Check for loop.
  // Somehow, YN1 and YN2 must be initialized with their "loop" values,
  // so yeah, it seems likely that we should raise an exception to let
  // the DSP program do that, at least if DSP_FORMAT == 0x0A.
  if (*current_address == (end_address + step_size_bytes - 1))
  {
    // Set address back to start address.
    *current_address = start_address;
    end_exception();
  }
  return val;
}

u16 dsp_read_accelerator()
{
  const u32 start_address = (g_dsp.ifx_regs[DSP_ACSAH] << 16) | g_dsp.ifx_regs[DSP_ACSAL];
  const u32 end_address = (g_dsp.ifx_regs[DSP_ACEAH] << 16) | g_dsp.ifx_regs[DSP_ACEAL];
  u32 current_address = (g_dsp.ifx_regs[DSP_ACCAH] << 16) | g_dsp.ifx_regs[DSP_ACCAL];

  auto end_address_reached = [] { DSPCore_SetException(EXP_ACCOV); };
  const u16 val = ReadAccelerator(
      start_address, end_address, &current_address, g_dsp.ifx_regs[DSP_FORMAT],
      reinterpret_cast<s16*>(&g_dsp.ifx_regs[DSP_YN1]),
      reinterpret_cast<s16*>(&g_dsp.ifx_regs[DSP_YN2]), &g_dsp.ifx_regs[DSP_PRED_SCALE],
      reinterpret_cast<s16*>(&g_dsp.ifx_regs[DSP_COEF_A1_0]), end_address_reached);

  gdsp_ifx_write(DSP_ACCAH, current_address >> 16);
  gdsp_ifx_write(DSP_ACCAL, current_address & 0xffff);
  return val;
}
}  // namespace DSP
