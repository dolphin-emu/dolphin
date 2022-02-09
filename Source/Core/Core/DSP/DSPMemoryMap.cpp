// Copyright 2008 Dolphin Emulator Project
// Copyright 2004 Duddie & Tratax
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/DSP/DSPCore.h"

#include "Common/Logging/Log.h"

#include "Core/DSP/DSPTables.h"

namespace DSP
{
u16 SDSP::ReadIMEM(u16 address) const
{
  switch (address >> 12)
  {
  case 0:  // 0xxx IRAM
    return iram[address & DSP_IRAM_MASK];

  case 8:  // 8xxx IROM - contains code to receive code for IRAM, and a bunch of mixing loops.
    return irom[address & DSP_IROM_MASK];

  default:  // Unmapped/non-existing memory
    ERROR_LOG_FMT(DSPLLE, "{:04x} DSP ERROR: Executing from invalid ({:04x}) memory", pc, address);
    return 0;
  }
}

u16 SDSP::ReadDMEM(u16 address)
{
  switch (address >> 12)
  {
  case 0x0:  // 0xxx DRAM
    return dram[address & DSP_DRAM_MASK];

  case 0x1:  // 1xxx COEF
    DEBUG_LOG_FMT(DSPLLE, "{:04x} : Coefficient Read @ {:04x}", pc, address);
    return coef[address & DSP_COEF_MASK];

  case 0xf:  // Fxxx HW regs
    return ReadIFX(address);

  default:  // Unmapped/non-existing memory
    ERROR_LOG_FMT(DSPLLE, "{:04x} DSP ERROR: Read from UNKNOWN ({:04x}) memory", pc, address);
    return 0;
  }
}

void SDSP::WriteDMEM(u16 address, u16 value)
{
  switch (address >> 12)
  {
  case 0x0:  // 0xxx DRAM
    dram[address & DSP_DRAM_MASK] = value;
    break;

  case 0xf:  // Fxxx HW regs
    WriteIFX(address, value);
    break;

  default:  // Unmapped/non-existing memory
    ERROR_LOG_FMT(DSPLLE, "{:04x} DSP ERROR: Write to UNKNOWN ({:04x}) memory", pc, address);
    break;
  }
}

u16 SDSP::FetchInstruction()
{
  const u16 opc = PeekInstruction();
  pc++;
  return opc;
}

u16 SDSP::PeekInstruction() const
{
  return ReadIMEM(pc);
}

void SDSP::SkipInstruction()
{
  pc += GetOpTemplate(PeekInstruction())->size;
}
}  // namespace DSP
