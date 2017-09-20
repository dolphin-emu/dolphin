// Copyright 2008 Dolphin Emulator Project
// Copyright 2004 Duddie & Tratax
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/DSP/DSPHWInterface.h"

#include <atomic>
#include <cstddef>
#include <cstring>

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/Hash.h"
#include "Common/Intrinsics.h"
#include "Common/Logging/Log.h"
#include "Common/MemoryUtil.h"
#include "Common/Swap.h"

#include "Core/DSP/DSPAccelerator.h"
#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPHost.h"
#include "Core/DSP/DSPTables.h"

namespace DSP
{
static void gdsp_do_dma();

void gdsp_ifx_init()
{
  g_dsp.ifx_regs.fill(0);

  g_dsp.mbox[MAILBOX_CPU].store(0);
  g_dsp.mbox[MAILBOX_DSP].store(0);
}

u32 gdsp_mbox_peek(Mailbox mbx)
{
  return g_dsp.mbox[mbx].load();
}

void gdsp_mbox_write_h(Mailbox mbx, u16 val)
{
  const u32 old_value = g_dsp.mbox[mbx].load(std::memory_order_acquire);
  const u32 new_value = (old_value & 0xffff) | (val << 16);

  g_dsp.mbox[mbx].store(new_value & ~0x80000000, std::memory_order_release);
}

void gdsp_mbox_write_l(Mailbox mbx, u16 val)
{
  const u32 old_value = g_dsp.mbox[mbx].load(std::memory_order_acquire);
  const u32 new_value = (old_value & ~0xffff) | val;

  g_dsp.mbox[mbx].store(new_value | 0x80000000, std::memory_order_release);

#if defined(_DEBUG) || defined(DEBUGFAST)
  if (mbx == MAILBOX_DSP)
    DEBUG_LOG(DSP_MAIL, "DSP(WM) B:%i M:0x%08x (pc=0x%04x)", mbx, gdsp_mbox_peek(MAILBOX_DSP),
              g_dsp.pc);
  else
    DEBUG_LOG(DSP_MAIL, "CPU(WM) B:%i M:0x%08x (pc=0x%04x)", mbx, gdsp_mbox_peek(MAILBOX_CPU),
              g_dsp.pc);
#endif
}

u16 gdsp_mbox_read_h(Mailbox mbx)
{
  if (g_init_hax && mbx == MAILBOX_DSP)
  {
    return 0x8054;
  }

  return (u16)(g_dsp.mbox[mbx].load() >> 16);  // TODO: mask away the top bit?
}

u16 gdsp_mbox_read_l(Mailbox mbx)
{
  const u32 value = g_dsp.mbox[mbx].load(std::memory_order_acquire);
  g_dsp.mbox[mbx].store(value & ~0x80000000, std::memory_order_release);

  if (g_init_hax && mbx == MAILBOX_DSP)
  {
    g_init_hax = false;
    DSPCore_Reset();
    return 0x4348;
  }

#if defined(_DEBUG) || defined(DEBUGFAST)
  if (mbx == MAILBOX_DSP)
    DEBUG_LOG(DSP_MAIL, "DSP(RM) B:%i M:0x%08x (pc=0x%04x)", mbx, gdsp_mbox_peek(MAILBOX_DSP),
              g_dsp.pc);
  else
    DEBUG_LOG(DSP_MAIL, "CPU(RM) B:%i M:0x%08x (pc=0x%04x)", mbx, gdsp_mbox_peek(MAILBOX_CPU),
              g_dsp.pc);
#endif

  return (u16)value;
}

void gdsp_ifx_write(u32 addr, u16 val)
{
  g_dsp_cap->LogIFXWrite(addr, val);

  switch (addr & 0xff)
  {
  case DSP_DIRQ:
    if (val & 0x1)
      Host::InterruptRequest();
    else
      WARN_LOG(DSPLLE, "Unknown Interrupt Request pc=%04x (%04x)", g_dsp.pc, val);
    break;

  case DSP_DMBH:
    gdsp_mbox_write_h(MAILBOX_DSP, val);
    break;

  case DSP_DMBL:
    gdsp_mbox_write_l(MAILBOX_DSP, val);
    break;

  case DSP_CMBH:
    return gdsp_mbox_write_h(MAILBOX_CPU, val);

  case DSP_CMBL:
    return gdsp_mbox_write_l(MAILBOX_CPU, val);

  case DSP_DSBL:
    g_dsp.ifx_regs[DSP_DSBL] = val;
    g_dsp.ifx_regs[DSP_DSCR] |= 4;  // Doesn't really matter since we do DMA instantly
    if (!g_dsp.ifx_regs[DSP_AMDM])
      gdsp_do_dma();
    else
      NOTICE_LOG(DSPLLE, "Masked DMA skipped");
    g_dsp.ifx_regs[DSP_DSCR] &= ~4;
    g_dsp.ifx_regs[DSP_DSBL] = 0;
    break;

  case DSP_GAIN:
    if (val)
    {
      DEBUG_LOG(DSPLLE, "Gain Written: 0x%04x", val);
    }
  case DSP_DSPA:
  case DSP_DSMAH:
  case DSP_DSMAL:
  case DSP_DSCR:
    g_dsp.ifx_regs[addr & 0xFF] = val;
    break;

  case DSP_ACSAH:
    g_dsp.accelerator->SetStartAddress(val << 16 |
                                       static_cast<u16>(g_dsp.accelerator->GetStartAddress()));
    break;
  case DSP_ACSAL:
    g_dsp.accelerator->SetStartAddress(
        static_cast<u16>(g_dsp.accelerator->GetStartAddress() >> 16) << 16 | val);
    break;
  case DSP_ACEAH:
    g_dsp.accelerator->SetEndAddress(val << 16 |
                                     static_cast<u16>(g_dsp.accelerator->GetEndAddress()));
    break;
  case DSP_ACEAL:
    g_dsp.accelerator->SetEndAddress(
        static_cast<u16>(g_dsp.accelerator->GetEndAddress() >> 16) << 16 | val);
    break;
  case DSP_ACCAH:
    g_dsp.accelerator->SetCurrentAddress(val << 16 |
                                         static_cast<u16>(g_dsp.accelerator->GetCurrentAddress()));
    break;
  case DSP_ACCAL:
    g_dsp.accelerator->SetCurrentAddress(
        static_cast<u16>(g_dsp.accelerator->GetCurrentAddress() >> 16) << 16 | val);
    break;
  case DSP_FORMAT:
    g_dsp.accelerator->SetSampleFormat(val);
    break;
  case DSP_YN1:
    g_dsp.accelerator->SetYn1(val);
    break;
  case DSP_YN2:
    g_dsp.accelerator->SetYn2(val);
    break;
  case DSP_PRED_SCALE:
    g_dsp.accelerator->SetPredScale(val);
    break;
  case DSP_ACDATA1:  // Accelerator write (Zelda type) - "UnkZelda"
    g_dsp.accelerator->WriteD3(val);
    break;

  default:
    if ((addr & 0xff) >= 0xa0)
    {
      if (pdlabels[(addr & 0xFF) - 0xa0].name && pdlabels[(addr & 0xFF) - 0xa0].description)
      {
        DEBUG_LOG(DSPLLE, "%04x MW %s (%04x)", g_dsp.pc, pdlabels[(addr & 0xFF) - 0xa0].name, val);
      }
      else
      {
        ERROR_LOG(DSPLLE, "%04x MW %04x (%04x)", g_dsp.pc, addr, val);
      }
    }
    else
    {
      ERROR_LOG(DSPLLE, "%04x MW %04x (%04x)", g_dsp.pc, addr, val);
    }
    g_dsp.ifx_regs[addr & 0xFF] = val;
    break;
  }
}

static u16 _gdsp_ifx_read(u16 addr)
{
  switch (addr & 0xff)
  {
  case DSP_DMBH:
    return gdsp_mbox_read_h(MAILBOX_DSP);

  case DSP_DMBL:
    return gdsp_mbox_read_l(MAILBOX_DSP);

  case DSP_CMBH:
    return gdsp_mbox_read_h(MAILBOX_CPU);

  case DSP_CMBL:
    return gdsp_mbox_read_l(MAILBOX_CPU);

  case DSP_DSCR:
    return g_dsp.ifx_regs[addr & 0xFF];

  case DSP_ACSAH:
    return static_cast<u16>(g_dsp.accelerator->GetStartAddress() >> 16);
  case DSP_ACSAL:
    return static_cast<u16>(g_dsp.accelerator->GetStartAddress());
  case DSP_ACEAH:
    return static_cast<u16>(g_dsp.accelerator->GetEndAddress() >> 16);
  case DSP_ACEAL:
    return static_cast<u16>(g_dsp.accelerator->GetEndAddress());
  case DSP_ACCAH:
    return static_cast<u16>(g_dsp.accelerator->GetCurrentAddress() >> 16);
  case DSP_ACCAL:
    return static_cast<u16>(g_dsp.accelerator->GetCurrentAddress());
  case DSP_FORMAT:
    return g_dsp.accelerator->GetSampleFormat();
  case DSP_YN1:
    return g_dsp.accelerator->GetYn1();
  case DSP_YN2:
    return g_dsp.accelerator->GetYn2();
  case DSP_PRED_SCALE:
    return g_dsp.accelerator->GetPredScale();
  case DSP_ACCELERATOR:  // ADPCM Accelerator reads
    return g_dsp.accelerator->Read(reinterpret_cast<s16*>(&g_dsp.ifx_regs[DSP_COEF_A1_0]));
  case DSP_ACDATA1:  // Accelerator reads (Zelda type) - "UnkZelda"
    return g_dsp.accelerator->ReadD3();

  default:
    if ((addr & 0xff) >= 0xa0)
    {
      if (pdlabels[(addr & 0xFF) - 0xa0].name && pdlabels[(addr & 0xFF) - 0xa0].description)
      {
        DEBUG_LOG(DSPLLE, "%04x MR %s (%04x)", g_dsp.pc, pdlabels[(addr & 0xFF) - 0xa0].name,
                  g_dsp.ifx_regs[addr & 0xFF]);
      }
      else
      {
        ERROR_LOG(DSPLLE, "%04x MR %04x (%04x)", g_dsp.pc, addr, g_dsp.ifx_regs[addr & 0xFF]);
      }
    }
    else
    {
      ERROR_LOG(DSPLLE, "%04x MR %04x (%04x)", g_dsp.pc, addr, g_dsp.ifx_regs[addr & 0xFF]);
    }
    return g_dsp.ifx_regs[addr & 0xFF];
  }
}

u16 gdsp_ifx_read(u16 addr)
{
  u16 retval = _gdsp_ifx_read(addr);
  g_dsp_cap->LogIFXRead(addr, retval);
  return retval;
}

static const u8* gdsp_idma_in(u16 dsp_addr, u32 addr, u32 size)
{
  u16* dst = g_dsp.iram + (dsp_addr / 2);

  const u8* code = &g_dsp.cpu_ram[addr & 0x0fffffff];
  g_dsp.iram_crc = HashEctor(code, size);

  Common::UnWriteProtectMemory(g_dsp.iram, DSP_IRAM_BYTE_SIZE, false);
  memcpy(dst, code, size);
  for (size_t i = 0; i < size / 2; i++)
    dst[i] = Common::swap16(dst[i]);
  Common::WriteProtectMemory(g_dsp.iram, DSP_IRAM_BYTE_SIZE, false);

  Host::CodeLoaded(code, size);
  NOTICE_LOG(DSPLLE, "*** Copy new UCode from 0x%08x to 0x%04x (crc: %8x)", addr, dsp_addr,
             g_dsp.iram_crc);

  return reinterpret_cast<u8*>(dst);
}

static const u8* gdsp_idma_out(u16 dsp_addr, u32 addr, u32 size)
{
  ERROR_LOG(DSPLLE, "*** idma_out IRAM_DSP (0x%04x) -> RAM (0x%08x) : size (0x%08x)", dsp_addr / 2,
            addr, size);

  return nullptr;
}

#if defined(_M_X86) || defined(_M_X86_64)
static const __m128i s_mask = _mm_set_epi32(0x0E0F0C0DL, 0x0A0B0809L, 0x06070405L, 0x02030001L);

FUNCTION_TARGET_SSSE3
static void gdsp_ddma_in_SSSE3(u16 dsp_addr, u32 addr, u32 size, u8* dst)
{
  for (u32 i = 0; i < size; i += 16)
  {
    _mm_storeu_si128(
        (__m128i*)&dst[dsp_addr + i],
        _mm_shuffle_epi8(_mm_loadu_si128((__m128i*)&g_dsp.cpu_ram[(addr + i) & 0x7FFFFFFF]),
                         s_mask));
  }
}

FUNCTION_TARGET_SSSE3
static void gdsp_ddma_out_SSSE3(u16 dsp_addr, u32 addr, u32 size, const u8* src)
{
  for (u32 i = 0; i < size; i += 16)
  {
    _mm_storeu_si128((__m128i*)&g_dsp.cpu_ram[(addr + i) & 0x7FFFFFFF],
                     _mm_shuffle_epi8(_mm_loadu_si128((__m128i*)&src[dsp_addr + i]), s_mask));
  }
}
#endif

// TODO: These should eat clock cycles.
static const u8* gdsp_ddma_in(u16 dsp_addr, u32 addr, u32 size)
{
  u8* dst = reinterpret_cast<u8*>(g_dsp.dram);

#if defined(_M_X86) || defined(_M_X86_64)
  if (cpu_info.bSSSE3 && !(size % 16))
  {
    gdsp_ddma_in_SSSE3(dsp_addr, addr, size, dst);
  }
  else
#endif
  {
    for (u32 i = 0; i < size; i += 2)
    {
      *(u16*)&dst[dsp_addr + i] =
          Common::swap16(*(const u16*)&g_dsp.cpu_ram[(addr + i) & 0x7FFFFFFF]);
    }
  }
  DEBUG_LOG(DSPLLE, "*** ddma_in RAM (0x%08x) -> DRAM_DSP (0x%04x) : size (0x%08x)", addr,
            dsp_addr / 2, size);

  return dst + dsp_addr;
}

static const u8* gdsp_ddma_out(u16 dsp_addr, u32 addr, u32 size)
{
  const u8* src = reinterpret_cast<const u8*>(g_dsp.dram);

#ifdef _M_X86
  if (cpu_info.bSSSE3 && !(size % 16))
  {
    gdsp_ddma_out_SSSE3(dsp_addr, addr, size, src);
  }
  else
#endif
  {
    for (u32 i = 0; i < size; i += 2)
    {
      *(u16*)&g_dsp.cpu_ram[(addr + i) & 0x7FFFFFFF] =
          Common::swap16(*(const u16*)&src[dsp_addr + i]);
    }
  }

  DEBUG_LOG(DSPLLE, "*** ddma_out DRAM_DSP (0x%04x) -> RAM (0x%08x) : size (0x%08x)", dsp_addr / 2,
            addr, size);

  return src + dsp_addr;
}

static void gdsp_do_dma()
{
  u32 addr = (g_dsp.ifx_regs[DSP_DSMAH] << 16) | g_dsp.ifx_regs[DSP_DSMAL];
  u16 ctl = g_dsp.ifx_regs[DSP_DSCR];
  u16 dsp_addr = g_dsp.ifx_regs[DSP_DSPA] * 2;
  u16 len = g_dsp.ifx_regs[DSP_DSBL];

  if (len > 0x4000)
  {
    ERROR_LOG(DSPLLE,
              "DMA ERROR: PC: %04x, Control: %04x, Address: %08x, DSP Address: %04x, Size: %04x",
              g_dsp.pc, ctl, addr, dsp_addr, len);
    exit(0);
  }
#if defined(_DEBUG) || defined(DEBUGFAST)
  DEBUG_LOG(DSPLLE, "DMA pc: %04x, Control: %04x, Address: %08x, DSP Address: %04x, Size: %04x",
            g_dsp.pc, ctl, addr, dsp_addr, len);
#endif

  const u8* copied_data_ptr = nullptr;
  switch (ctl & 0x3)
  {
  case (DSP_CR_DMEM | DSP_CR_TO_CPU):
    copied_data_ptr = gdsp_ddma_out(dsp_addr, addr, len);
    break;

  case (DSP_CR_DMEM | DSP_CR_FROM_CPU):
    copied_data_ptr = gdsp_ddma_in(dsp_addr, addr, len);
    break;

  case (DSP_CR_IMEM | DSP_CR_TO_CPU):
    copied_data_ptr = gdsp_idma_out(dsp_addr, addr, len);
    break;

  case (DSP_CR_IMEM | DSP_CR_FROM_CPU):
    copied_data_ptr = gdsp_idma_in(dsp_addr, addr, len);
    break;
  }

  if (copied_data_ptr)
    g_dsp_cap->LogDMA(ctl, addr, dsp_addr, len, copied_data_ptr);
}
}  // namespace DSP
