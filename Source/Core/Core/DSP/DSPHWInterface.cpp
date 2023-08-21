// Copyright 2008 Dolphin Emulator Project
// Copyright 2004 Duddie & Tratax
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/DSP/DSPCore.h"

#include <atomic>
#include <cstddef>
#include <cstring>

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/Intrinsics.h"
#include "Common/Logging/Log.h"
#include "Common/MemoryUtil.h"
#include "Common/Swap.h"

#include "Core/DSP/DSPAccelerator.h"
#include "Core/DSP/DSPHost.h"
#include "Core/DSP/DSPTables.h"

namespace DSP
{
void SDSP::InitializeIFX()
{
  m_ifx_regs.fill(0);

  GetMailbox(Mailbox::CPU).store(0);
  GetMailbox(Mailbox::DSP).store(0);
}

u32 SDSP::PeekMailbox(Mailbox mailbox) const
{
  return GetMailbox(mailbox).load();
}

u16 SDSP::ReadMailboxLow(Mailbox mailbox)
{
  const u32 value = GetMailbox(mailbox).load(std::memory_order_acquire);
  GetMailbox(mailbox).store(value & ~0x80000000, std::memory_order_release);

#if defined(_DEBUG) || defined(DEBUGFAST)
  const char* const type = mailbox == Mailbox::DSP ? "DSP" : "CPU";
  DEBUG_LOG_FMT(DSP_MAIL, "{}(RM) B:{} M:{:#010x} (pc={:#06x})", type, static_cast<int>(mailbox),
                PeekMailbox(mailbox), pc);
#endif

  return static_cast<u16>(value);
}

u16 SDSP::ReadMailboxHigh(Mailbox mailbox)
{
  // TODO: mask away the top bit?
  return static_cast<u16>(PeekMailbox(mailbox) >> 16);
}

void SDSP::WriteMailboxLow(Mailbox mailbox, u16 value)
{
  const u32 old_value = GetMailbox(mailbox).load(std::memory_order_acquire);
  const u32 new_value = (old_value & ~0xffff) | value;

  GetMailbox(mailbox).store(new_value | 0x80000000, std::memory_order_release);

#if defined(_DEBUG) || defined(DEBUGFAST)
  const char* const type = mailbox == Mailbox::DSP ? "DSP" : "CPU";
  DEBUG_LOG_FMT(DSP_MAIL, "{}(WM) B:{} M:{:#010x} (pc={:#06x})", type, static_cast<int>(mailbox),
                PeekMailbox(mailbox), pc);
#endif
}

void SDSP::WriteMailboxHigh(Mailbox mailbox, u16 value)
{
  const u32 old_value = GetMailbox(mailbox).load(std::memory_order_acquire);
  const u32 new_value = (old_value & 0xffff) | (value << 16);

  GetMailbox(mailbox).store(new_value & ~0x80000000, std::memory_order_release);
}

void SDSP::WriteIFX(u32 address, u16 value)
{
  m_dsp_core.LogIFXWrite(address, value);

  switch (address & 0xff)
  {
  case DSP_DIRQ:
    if ((value & 1) != 0)
    {
      Host::InterruptRequest();
    }
    else if (value != 0)
    {
      // The homebrew libasnd uCode frequently writes 0 to DIRQ with a comment
      // saying "clear the interrupt" - we don't need to log in this case.
      WARN_LOG_FMT(DSPLLE, "Unknown Interrupt Request pc={:#06x} ({:#06x})", pc, value);
    }
    break;

  case DSP_DMBH:
    WriteMailboxHigh(Mailbox::DSP, value);
    break;

  case DSP_DMBL:
    WriteMailboxLow(Mailbox::DSP, value);
    break;

  case DSP_CMBH:
    WriteMailboxHigh(Mailbox::CPU, value);
    break;

  case DSP_CMBL:
    WriteMailboxLow(Mailbox::CPU, value);
    break;

  case DSP_DSBL:
    m_ifx_regs[DSP_DSBL] = value;
    m_ifx_regs[DSP_DSCR] |= 4;  // Doesn't really matter since we do DMA instantly
    if (!m_ifx_regs[DSP_AMDM])
      DoDMA();
    else
      NOTICE_LOG_FMT(DSPLLE, "Masked DMA skipped");
    m_ifx_regs[DSP_DSCR] &= ~4;
    m_ifx_regs[DSP_DSBL] = 0;
    break;

  case DSP_GAIN:
    if (value != 0)
    {
      DEBUG_LOG_FMT(DSPLLE, "Gain Written: {:#06x}", value);
    }
    [[fallthrough]];
  case DSP_DSPA:
  case DSP_DSMAH:
  case DSP_DSMAL:
  case DSP_DSCR:
    m_ifx_regs[address & 0xFF] = value;
    break;

  case DSP_ACSAH:
    m_accelerator->SetStartAddress(value << 16 |
                                   static_cast<u16>(m_accelerator->GetStartAddress()));
    break;
  case DSP_ACSAL:
    m_accelerator->SetStartAddress(static_cast<u16>(m_accelerator->GetStartAddress() >> 16) << 16 |
                                   value);
    break;
  case DSP_ACEAH:
    m_accelerator->SetEndAddress(value << 16 | static_cast<u16>(m_accelerator->GetEndAddress()));
    break;
  case DSP_ACEAL:
    m_accelerator->SetEndAddress(static_cast<u16>(m_accelerator->GetEndAddress() >> 16) << 16 |
                                 value);
    break;
  case DSP_ACCAH:
    m_accelerator->SetCurrentAddress(value << 16 |
                                     static_cast<u16>(m_accelerator->GetCurrentAddress()));
    break;
  case DSP_ACCAL:
    m_accelerator->SetCurrentAddress(
        static_cast<u16>(m_accelerator->GetCurrentAddress() >> 16) << 16 | value);
    break;
  case DSP_FORMAT:
    m_accelerator->SetSampleFormat(value);
    break;
  case DSP_YN1:
    m_accelerator->SetYn1(value);
    break;
  case DSP_YN2:
    m_accelerator->SetYn2(value);
    break;
  case DSP_PRED_SCALE:
    m_accelerator->SetPredScale(value);
    break;
  case DSP_ACDATA1:  // Accelerator write (Zelda type) - "UnkZelda"
    m_accelerator->WriteD3(value);
    break;

  default:
    if ((address & 0xff) >= 0xa0)
    {
      const u32 index = (address & 0xFF) - 0xa0;
      const auto& label = pdlabels[index];

      if (label.name && label.description)
      {
        DEBUG_LOG_FMT(DSPLLE, "{:04x} MW {} ({:04x})", pc, label.name, value);
      }
      else
      {
        ERROR_LOG_FMT(DSPLLE, "{:04x} MW {:04x} ({:04x})", pc, address, value);
      }
    }
    else
    {
      ERROR_LOG_FMT(DSPLLE, "{:04x} MW {:04x} ({:04x})", pc, address, value);
    }
    m_ifx_regs[address & 0xFF] = value;
    break;
  }
}

u16 SDSP::ReadIFXImpl(u16 address)
{
  switch (address & 0xff)
  {
  case DSP_DMBH:
    return ReadMailboxHigh(Mailbox::DSP);

  case DSP_DMBL:
    return ReadMailboxLow(Mailbox::DSP);

  case DSP_CMBH:
    return ReadMailboxHigh(Mailbox::CPU);

  case DSP_CMBL:
    return ReadMailboxLow(Mailbox::CPU);

  case DSP_DSCR:
    return m_ifx_regs[address & 0xFF];

  case DSP_ACSAH:
    return static_cast<u16>(m_accelerator->GetStartAddress() >> 16);
  case DSP_ACSAL:
    return static_cast<u16>(m_accelerator->GetStartAddress());
  case DSP_ACEAH:
    return static_cast<u16>(m_accelerator->GetEndAddress() >> 16);
  case DSP_ACEAL:
    return static_cast<u16>(m_accelerator->GetEndAddress());
  case DSP_ACCAH:
    return static_cast<u16>(m_accelerator->GetCurrentAddress() >> 16);
  case DSP_ACCAL:
    return static_cast<u16>(m_accelerator->GetCurrentAddress());
  case DSP_FORMAT:
    return m_accelerator->GetSampleFormat();
  case DSP_YN1:
    return m_accelerator->GetYn1();
  case DSP_YN2:
    return m_accelerator->GetYn2();
  case DSP_PRED_SCALE:
    return m_accelerator->GetPredScale();
  case DSP_ACCELERATOR:  // ADPCM Accelerator reads
    return m_accelerator->Read(reinterpret_cast<s16*>(&m_ifx_regs[DSP_COEF_A1_0]));
  case DSP_ACDATA1:  // Accelerator reads (Zelda type) - "UnkZelda"
    return m_accelerator->ReadD3();

  default:
  {
    const u16 ifx_reg = m_ifx_regs[address & 0xFF];

    if ((address & 0xff) >= 0xa0)
    {
      const u32 index = (address & 0xFF) - 0xa0;
      const auto& label = pdlabels[index];

      if (label.name && label.description)
      {
        DEBUG_LOG_FMT(DSPLLE, "{:04x} MR {} ({:04x})", pc, label.name, ifx_reg);
      }
      else
      {
        ERROR_LOG_FMT(DSPLLE, "{:04x} MR {:04x} ({:04x})", pc, address, ifx_reg);
      }
    }
    else
    {
      ERROR_LOG_FMT(DSPLLE, "{:04x} MR {:04x} ({:04x})", pc, address, ifx_reg);
    }
    return ifx_reg;
  }
  }
}

u16 SDSP::ReadIFX(u16 address)
{
  const u16 retval = ReadIFXImpl(address);
  m_dsp_core.LogIFXRead(address, retval);
  return retval;
}

const u8* SDSP::IDMAIn(u16 dsp_addr, u32 addr, u32 size)
{
  Common::UnWriteProtectMemory(iram, DSP_IRAM_BYTE_SIZE, false);
  Host::DMAToDSP(iram + dsp_addr / 2, addr, size);
  Common::WriteProtectMemory(iram, DSP_IRAM_BYTE_SIZE, false);

  Host::CodeLoaded(m_dsp_core, addr, size);
  NOTICE_LOG_FMT(DSPLLE, "*** Copy new UCode from {:#010x} to {:#06x} (crc: {:#08x})", addr,
                 dsp_addr, m_iram_crc);

  return reinterpret_cast<const u8*>(iram) + dsp_addr;
}

const u8* SDSP::IDMAOut(u16 dsp_addr, u32 addr, u32 size)
{
  ERROR_LOG_FMT(DSPLLE, "*** idma_out IRAM_DSP ({:#06x}) -> RAM ({:#010x}) : size ({:#010x})",
                dsp_addr / 2, addr, size);
  return nullptr;
}

// TODO: These should eat clock cycles.
const u8* SDSP::DDMAIn(u16 dsp_addr, u32 addr, u32 size)
{
  Host::DMAToDSP(dram + dsp_addr / 2, addr, size);
  DEBUG_LOG_FMT(DSPLLE, "*** ddma_in RAM ({:#010x}) -> DRAM_DSP ({:#06x}) : size ({:#010x})", addr,
                dsp_addr / 2, size);

  return reinterpret_cast<const u8*>(dram) + dsp_addr;
}

const u8* SDSP::DDMAOut(u16 dsp_addr, u32 addr, u32 size)
{
  Host::DMAFromDSP(dram + dsp_addr / 2, addr, size);
  DEBUG_LOG_FMT(DSPLLE, "*** ddma_out DRAM_DSP ({:#06x}) -> RAM ({:#010x}) : size ({:#010x})",
                dsp_addr / 2, addr, size);

  return reinterpret_cast<const u8*>(dram) + dsp_addr;
}

void SDSP::DoDMA()
{
  const u32 addr = (m_ifx_regs[DSP_DSMAH] << 16) | m_ifx_regs[DSP_DSMAL];
  const u16 ctl = m_ifx_regs[DSP_DSCR];
  const u16 dsp_addr = m_ifx_regs[DSP_DSPA] * 2;
  const u16 len = m_ifx_regs[DSP_DSBL];

  if (len > 0x4000)
  {
    ERROR_LOG_FMT(DSPLLE,
                  "DMA ERROR: PC: {:04x}, Control: {:04x}, Address: {:08x}, DSP Address: {:04x}, "
                  "Size: {:04x}",
                  pc, ctl, addr, dsp_addr, len);
    std::exit(0);
  }
#if defined(_DEBUG) || defined(DEBUGFAST)
  DEBUG_LOG_FMT(
      DSPLLE, "DMA pc: {:04x}, Control: {:04x}, Address: {:08x}, DSP Address: {:04x}, Size: {:04x}",
      pc, ctl, addr, dsp_addr, len);
#endif

  const u8* copied_data_ptr = nullptr;
  switch (ctl & 0x3)
  {
  case (DSP_CR_DMEM | DSP_CR_TO_CPU):
    copied_data_ptr = DDMAOut(dsp_addr, addr, len);
    break;

  case (DSP_CR_DMEM | DSP_CR_FROM_CPU):
    copied_data_ptr = DDMAIn(dsp_addr, addr, len);
    break;

  case (DSP_CR_IMEM | DSP_CR_TO_CPU):
    copied_data_ptr = IDMAOut(dsp_addr, addr, len);
    break;

  case (DSP_CR_IMEM | DSP_CR_FROM_CPU):
    copied_data_ptr = IDMAIn(dsp_addr, addr, len);
    break;
  }

  if (copied_data_ptr)
    m_dsp_core.LogDMA(ctl, addr, dsp_addr, len, copied_data_ptr);
}
}  // namespace DSP
