// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Interpreter/Interpreter.h"

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/SystemTimers.h"
#include "Core/PowerPC/Interpreter/ExceptionUtils.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

/*

Most of these are together with fctiwx
mffsx: 800c3624
mffsx: 80043c98
mffsx: 8003dd48
mffsx: 8003dd9c
mffsx: 80036608
mffsx: 80036650 (huh?)

*/

static void FPSCRUpdated(UReg_FPSCR* fpscr)
{
  UpdateFPExceptionSummary(fpscr);
  PowerPC::RoundingModeUpdated();
}

void Interpreter::mtfsb0x(Interpreter& interpreter, UGeckoInstruction inst)
{
  u32 b = 0x80000000 >> inst.CRBD;

  PowerPC::ppcState.fpscr.Hex &= ~b;
  FPSCRUpdated(&PowerPC::ppcState.fpscr);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

// This instruction can affect FX
void Interpreter::mtfsb1x(Interpreter& interpreter, UGeckoInstruction inst)
{
  const u32 bit = inst.CRBD;
  const u32 b = 0x80000000 >> bit;

  if ((b & FPSCR_ANY_X) != 0)
    SetFPException(&PowerPC::ppcState.fpscr, b);
  else
    PowerPC::ppcState.fpscr |= b;

  FPSCRUpdated(&PowerPC::ppcState.fpscr);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::mtfsfix(Interpreter& interpreter, UGeckoInstruction inst)
{
  const u32 field = inst.CRFD;
  const u32 pre_shifted_mask = 0xF0000000;
  const u32 mask = (pre_shifted_mask >> (4 * field));
  const u32 imm = (inst.hex << 16) & pre_shifted_mask;

  PowerPC::ppcState.fpscr = (PowerPC::ppcState.fpscr.Hex & ~mask) | (imm >> (4 * field));

  FPSCRUpdated(&PowerPC::ppcState.fpscr);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::mtfsfx(Interpreter& interpreter, UGeckoInstruction inst)
{
  const u32 fm = inst.FM;
  u32 m = 0;
  for (u32 i = 0; i < 8; i++)
  {
    if ((fm & (1U << i)) != 0)
      m |= (0xFU << (i * 4));
  }

  PowerPC::ppcState.fpscr = (PowerPC::ppcState.fpscr.Hex & ~m) |
                            (static_cast<u32>(PowerPC::ppcState.ps[inst.FB].PS0AsU64()) & m);
  FPSCRUpdated(&PowerPC::ppcState.fpscr);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::mcrxr(Interpreter& interpreter, UGeckoInstruction inst)
{
  PowerPC::ppcState.cr.SetField(inst.CRFD, PowerPC::ppcState.GetXER().Hex >> 28);
  PowerPC::ppcState.xer_ca = 0;
  PowerPC::ppcState.xer_so_ov = 0;
}

void Interpreter::mfcr(Interpreter& interpreter, UGeckoInstruction inst)
{
  PowerPC::ppcState.gpr[inst.RD] = PowerPC::ppcState.cr.Get();
}

void Interpreter::mtcrf(Interpreter& interpreter, UGeckoInstruction inst)
{
  const u32 crm = inst.CRM;
  if (crm == 0xFF)
  {
    PowerPC::ppcState.cr.Set(PowerPC::ppcState.gpr[inst.RS]);
  }
  else
  {
    // TODO: use lookup table? probably not worth it
    u32 mask = 0;
    for (u32 i = 0; i < 8; i++)
    {
      if ((crm & (1U << i)) != 0)
        mask |= 0xFU << (i * 4);
    }

    PowerPC::ppcState.cr.Set((PowerPC::ppcState.cr.Get() & ~mask) |
                             (PowerPC::ppcState.gpr[inst.RS] & mask));
  }
}

void Interpreter::mfmsr(Interpreter& interpreter, UGeckoInstruction inst)
{
  if (PowerPC::ppcState.msr.PR)
  {
    GenerateProgramException(ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  PowerPC::ppcState.gpr[inst.RD] = PowerPC::ppcState.msr.Hex;
}

void Interpreter::mfsr(Interpreter& interpreter, UGeckoInstruction inst)
{
  if (PowerPC::ppcState.msr.PR)
  {
    GenerateProgramException(ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  PowerPC::ppcState.gpr[inst.RD] = PowerPC::ppcState.sr[inst.SR];
}

void Interpreter::mfsrin(Interpreter& interpreter, UGeckoInstruction inst)
{
  if (PowerPC::ppcState.msr.PR)
  {
    GenerateProgramException(ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  const u32 index = (PowerPC::ppcState.gpr[inst.RB] >> 28) & 0xF;
  PowerPC::ppcState.gpr[inst.RD] = PowerPC::ppcState.sr[index];
}

void Interpreter::mtmsr(Interpreter& interpreter, UGeckoInstruction inst)
{
  if (PowerPC::ppcState.msr.PR)
  {
    GenerateProgramException(ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  PowerPC::ppcState.msr.Hex = PowerPC::ppcState.gpr[inst.RS];

  // FE0/FE1 may have been set
  CheckFPExceptions(PowerPC::ppcState.fpscr);

  PowerPC::CheckExceptions();
  m_end_block = true;
}

// Segment registers. MMU control.

void Interpreter::mtsr(Interpreter& interpreter, UGeckoInstruction inst)
{
  if (PowerPC::ppcState.msr.PR)
  {
    GenerateProgramException(ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  const u32 index = inst.SR;
  const u32 value = PowerPC::ppcState.gpr[inst.RS];
  PowerPC::ppcState.SetSR(index, value);
}

void Interpreter::mtsrin(Interpreter& interpreter, UGeckoInstruction inst)
{
  if (PowerPC::ppcState.msr.PR)
  {
    GenerateProgramException(ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  const u32 index = (PowerPC::ppcState.gpr[inst.RB] >> 28) & 0xF;
  const u32 value = PowerPC::ppcState.gpr[inst.RS];
  PowerPC::ppcState.SetSR(index, value);
}

void Interpreter::mftb(Interpreter& interpreter, UGeckoInstruction inst)
{
  [[maybe_unused]] const u32 index = (inst.TBR >> 5) | ((inst.TBR & 0x1F) << 5);
  DEBUG_ASSERT_MSG(POWERPC, (index == SPR_TL) || (index == SPR_TU), "Invalid mftb");
  mfspr(interpreter, inst);
}

void Interpreter::mfspr(Interpreter& interpreter, UGeckoInstruction inst)
{
  const u32 index = ((inst.SPR & 0x1F) << 5) + ((inst.SPR >> 5) & 0x1F);

  // XER, LR, CTR, and timebase halves are the only ones available in user mode.
  if (PowerPC::ppcState.msr.PR && index != SPR_XER && index != SPR_LR && index != SPR_CTR &&
      index != SPR_TL && index != SPR_TU)
  {
    GenerateProgramException(ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  switch (index)
  {
  case SPR_DEC:
    if ((PowerPC::ppcState.spr[index] & 0x80000000) == 0)  // We are still decrementing
    {
      PowerPC::ppcState.spr[index] = SystemTimers::GetFakeDecrementer();
    }
    break;

  case SPR_TL:
  case SPR_TU:
    PowerPC::WriteFullTimeBaseValue(SystemTimers::GetFakeTimeBase());
    break;

  case SPR_WPAR:
  {
    // The bottom, read-only bit checks if the buffer is not empty.
    // GXRedirectWriteGatherPipe and GXRestoreWriteGatherPipe (used for display lists) wait for
    // this bit to be cleared before writing to SPR_WPAR again (with a value of 0x0c00800 (aka
    // GPFifo::GATHER_PIPE_PHYSICAL_ADDRESS)).
    // Currently, we always treat the buffer as not empty, as the exact behavior is unclear
    // (and games that use display lists will hang if the bit doesn't eventually become zero).
    if (Core::System::GetInstance().GetGPFifo().IsBNE())
      PowerPC::ppcState.spr[index] |= 1;
    else
      PowerPC::ppcState.spr[index] &= ~1;
  }
  break;

  case SPR_XER:
    PowerPC::ppcState.spr[index] = PowerPC::ppcState.GetXER().Hex;
    break;

  case SPR_UPMC1:
    PowerPC::ppcState.spr[index] = PowerPC::ppcState.spr[SPR_PMC1];
    break;

  case SPR_UPMC2:
    PowerPC::ppcState.spr[index] = PowerPC::ppcState.spr[SPR_PMC2];
    break;

  case SPR_UPMC3:
    PowerPC::ppcState.spr[index] = PowerPC::ppcState.spr[SPR_PMC3];
    break;

  case SPR_UPMC4:
    PowerPC::ppcState.spr[index] = PowerPC::ppcState.spr[SPR_PMC4];
    break;

  case SPR_IABR:
    // A strange quirk: reading back this register on hardware will always have the TE (Translation
    // enabled) bit set to 0 (despite the bit appearing to function normally when set). This does
    // not apply to the DABR.
    PowerPC::ppcState.gpr[inst.RD] = PowerPC::ppcState.spr[index] & ~1;
    return;
  }
  PowerPC::ppcState.gpr[inst.RD] = PowerPC::ppcState.spr[index];
}

void Interpreter::mtspr(Interpreter& interpreter, UGeckoInstruction inst)
{
  const u32 index = (inst.SPRU << 5) | (inst.SPRL & 0x1F);

  // XER, LR, and CTR are the only ones available to be written to in user mode
  if (PowerPC::ppcState.msr.PR && index != SPR_XER && index != SPR_LR && index != SPR_CTR)
  {
    GenerateProgramException(ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  const u32 old_value = PowerPC::ppcState.spr[index];
  PowerPC::ppcState.spr[index] = PowerPC::ppcState.gpr[inst.RD];

  // Our DMA emulation is highly inaccurate - instead of properly emulating the queue
  // and so on, we simply make all DMA:s complete instantaneously.

  switch (index)
  {
  case SPR_TL:
  case SPR_TU:
    PanicAlertFmt("Illegal Write to TL/TU");
    break;

  case SPR_TL_W:
    TL(PowerPC::ppcState) = PowerPC::ppcState.gpr[inst.RD];
    SystemTimers::TimeBaseSet();
    break;

  case SPR_TU_W:
    TU(PowerPC::ppcState) = PowerPC::ppcState.gpr[inst.RD];
    SystemTimers::TimeBaseSet();
    break;

  case SPR_PVR:
    // PVR is a read-only register so maintain its value.
    PowerPC::ppcState.spr[index] = old_value;
    break;

  case SPR_HID0:  // HID0
  {
    UReg_HID0 old_hid0;
    old_hid0.Hex = old_value;
    if (HID0(PowerPC::ppcState).ICE != old_hid0.ICE)
    {
      INFO_LOG_FMT(POWERPC, "Instruction Cache Enable (HID0.ICE) = {}",
                   HID0(PowerPC::ppcState).ICE);
    }
    if (HID0(PowerPC::ppcState).ILOCK != old_hid0.ILOCK)
    {
      INFO_LOG_FMT(POWERPC, "Instruction Cache Lock (HID0.ILOCK) = {}",
                   HID0(PowerPC::ppcState).ILOCK);
    }
    if (HID0(PowerPC::ppcState).ICFI)
    {
      HID0(PowerPC::ppcState).ICFI = 0;
      INFO_LOG_FMT(POWERPC, "Flush Instruction Cache! ICE={}", HID0(PowerPC::ppcState).ICE);
      // this is rather slow
      // most games do it only once during initialization
      PowerPC::ppcState.iCache.Reset();
    }
  }
  break;

  case SPR_HID1:
    // Despite being documented as a read-only register, it actually isn't. Bits
    // 0-4 (27-31 from a little endian perspective) are modifiable. The rest are not
    // affected, as those bits are reserved and ignore writes to them.
    PowerPC::ppcState.spr[index] &= 0xF8000000;
    break;

  case SPR_HID2:
    // TODO: disable write gather pipe if WPE not set
    // TODO: emulate locked cache and DMA bits.
    // Only the lower half of the register (upper half from a little endian perspective)
    // is modifiable, except for the DMAQL field.
    PowerPC::ppcState.spr[index] =
        (PowerPC::ppcState.spr[index] & 0xF0FF0000) | (old_value & 0x0F000000);
    break;

  case SPR_HID4:
    if (old_value != PowerPC::ppcState.spr[index])
    {
      INFO_LOG_FMT(POWERPC, "HID4 updated {:x} {:x}", old_value, PowerPC::ppcState.spr[index]);
      PowerPC::IBATUpdated();
      PowerPC::DBATUpdated();
    }
    break;

  case SPR_WPAR:
    ASSERT_MSG(POWERPC, PowerPC::ppcState.spr[SPR_WPAR] == GPFifo::GATHER_PIPE_PHYSICAL_ADDRESS,
               "Gather pipe changed to unexpected address {:08x} @ PC {:08x}",
               PowerPC::ppcState.spr[SPR_WPAR], PowerPC::ppcState.pc);
    Core::System::GetInstance().GetGPFifo().ResetGatherPipe();
    break;

  // Graphics Quantization Registers
  case SPR_GQR0:
  case SPR_GQR0 + 1:
  case SPR_GQR0 + 2:
  case SPR_GQR0 + 3:
  case SPR_GQR0 + 4:
  case SPR_GQR0 + 5:
  case SPR_GQR0 + 6:
  case SPR_GQR0 + 7:
    break;

  case SPR_DMAL:
    // Locked cache<->Memory DMA
    // Total fake, we ignore that DMAs take time.
    if (DMAL(PowerPC::ppcState).DMA_T)
    {
      const u32 mem_address = DMAU(PowerPC::ppcState).MEM_ADDR << 5;
      const u32 cache_address = DMAL(PowerPC::ppcState).LC_ADDR << 5;
      u32 length = ((DMAU(PowerPC::ppcState).DMA_LEN_U << 2) | DMAL(PowerPC::ppcState).DMA_LEN_L);

      if (length == 0)
        length = 128;
      if (DMAL(PowerPC::ppcState).DMA_LD)
        PowerPC::DMA_MemoryToLC(cache_address, mem_address, length);
      else
        PowerPC::DMA_LCToMemory(mem_address, cache_address, length);
    }
    DMAL(PowerPC::ppcState).DMA_T = 0;
    break;

  case SPR_L2CR:
    break;

  case SPR_DEC:
    // Top bit from 0 to 1
    if ((old_value >> 31) == 0 && (PowerPC::ppcState.gpr[inst.RD] >> 31) != 0)
    {
      INFO_LOG_FMT(POWERPC, "Software triggered Decrementer exception");
      PowerPC::ppcState.Exceptions |= EXCEPTION_DECREMENTER;
    }
    SystemTimers::DecrementerSet();
    break;

  // Page table base etc
  case SPR_SDR:
    PowerPC::SDRUpdated();
    break;

  case SPR_XER:
    PowerPC::ppcState.SetXER(UReg_XER{PowerPC::ppcState.spr[index]});
    break;

  case SPR_DBAT0L:
  case SPR_DBAT0U:
  case SPR_DBAT1L:
  case SPR_DBAT1U:
  case SPR_DBAT2L:
  case SPR_DBAT2U:
  case SPR_DBAT3L:
  case SPR_DBAT3U:
  case SPR_DBAT4L:
  case SPR_DBAT4U:
  case SPR_DBAT5L:
  case SPR_DBAT5U:
  case SPR_DBAT6L:
  case SPR_DBAT6U:
  case SPR_DBAT7L:
  case SPR_DBAT7U:
    if (old_value != PowerPC::ppcState.spr[index])
    {
      INFO_LOG_FMT(POWERPC, "DBAT updated {} {:x} {:x}", index, old_value,
                   PowerPC::ppcState.spr[index]);
      PowerPC::DBATUpdated();
    }
    break;

  case SPR_IBAT0L:
  case SPR_IBAT0U:
  case SPR_IBAT1L:
  case SPR_IBAT1U:
  case SPR_IBAT2L:
  case SPR_IBAT2U:
  case SPR_IBAT3L:
  case SPR_IBAT3U:
  case SPR_IBAT4L:
  case SPR_IBAT4U:
  case SPR_IBAT5L:
  case SPR_IBAT5U:
  case SPR_IBAT6L:
  case SPR_IBAT6U:
  case SPR_IBAT7L:
  case SPR_IBAT7U:
    if (old_value != PowerPC::ppcState.spr[index])
    {
      INFO_LOG_FMT(POWERPC, "IBAT updated {} {:x} {:x}", index, old_value,
                   PowerPC::ppcState.spr[index]);
      PowerPC::IBATUpdated();
    }
    break;

  case SPR_THRM1:
  case SPR_THRM2:
  case SPR_THRM3:
  {
    // We update both THRM1 and THRM2 when either of the 3 thermal control
    // registers are updated. THRM1 and THRM2 are independent, but THRM3 has
    // settings that impact both.
    //
    // TODO: Support thermal interrupts when enabled.
    constexpr u32 SIMULATED_TEMP = 42;  // °C

    auto UpdateThermalReg = [](UReg_THRM12* reg) {
      if (!THRM3(PowerPC::ppcState).E || !reg->V)
      {
        reg->TIV = 0;
      }
      else
      {
        reg->TIV = 1;
        if (reg->TID)
          reg->TIN = SIMULATED_TEMP < reg->THRESHOLD;
        else
          reg->TIN = SIMULATED_TEMP > reg->THRESHOLD;
      }
    };

    UpdateThermalReg(&THRM1(PowerPC::ppcState));
    UpdateThermalReg(&THRM2(PowerPC::ppcState));
    break;
  }
  }
}

void Interpreter::crand(Interpreter& interpreter, UGeckoInstruction inst)
{
  const u32 a = PowerPC::ppcState.cr.GetBit(inst.CRBA);
  const u32 b = PowerPC::ppcState.cr.GetBit(inst.CRBB);

  PowerPC::ppcState.cr.SetBit(inst.CRBD, a & b);
}

void Interpreter::crandc(Interpreter& interpreter, UGeckoInstruction inst)
{
  const u32 a = PowerPC::ppcState.cr.GetBit(inst.CRBA);
  const u32 b = PowerPC::ppcState.cr.GetBit(inst.CRBB);

  PowerPC::ppcState.cr.SetBit(inst.CRBD, a & (1 ^ b));
}

void Interpreter::creqv(Interpreter& interpreter, UGeckoInstruction inst)
{
  const u32 a = PowerPC::ppcState.cr.GetBit(inst.CRBA);
  const u32 b = PowerPC::ppcState.cr.GetBit(inst.CRBB);

  PowerPC::ppcState.cr.SetBit(inst.CRBD, 1 ^ (a ^ b));
}

void Interpreter::crnand(Interpreter& interpreter, UGeckoInstruction inst)
{
  const u32 a = PowerPC::ppcState.cr.GetBit(inst.CRBA);
  const u32 b = PowerPC::ppcState.cr.GetBit(inst.CRBB);

  PowerPC::ppcState.cr.SetBit(inst.CRBD, 1 ^ (a & b));
}

void Interpreter::crnor(Interpreter& interpreter, UGeckoInstruction inst)
{
  const u32 a = PowerPC::ppcState.cr.GetBit(inst.CRBA);
  const u32 b = PowerPC::ppcState.cr.GetBit(inst.CRBB);

  PowerPC::ppcState.cr.SetBit(inst.CRBD, 1 ^ (a | b));
}

void Interpreter::cror(Interpreter& interpreter, UGeckoInstruction inst)
{
  const u32 a = PowerPC::ppcState.cr.GetBit(inst.CRBA);
  const u32 b = PowerPC::ppcState.cr.GetBit(inst.CRBB);

  PowerPC::ppcState.cr.SetBit(inst.CRBD, a | b);
}

void Interpreter::crorc(Interpreter& interpreter, UGeckoInstruction inst)
{
  const u32 a = PowerPC::ppcState.cr.GetBit(inst.CRBA);
  const u32 b = PowerPC::ppcState.cr.GetBit(inst.CRBB);

  PowerPC::ppcState.cr.SetBit(inst.CRBD, a | (1 ^ b));
}

void Interpreter::crxor(Interpreter& interpreter, UGeckoInstruction inst)
{
  const u32 a = PowerPC::ppcState.cr.GetBit(inst.CRBA);
  const u32 b = PowerPC::ppcState.cr.GetBit(inst.CRBB);

  PowerPC::ppcState.cr.SetBit(inst.CRBD, a ^ b);
}

void Interpreter::mcrf(Interpreter& interpreter, UGeckoInstruction inst)
{
  const u32 cr_f = PowerPC::ppcState.cr.GetField(inst.CRFS);
  PowerPC::ppcState.cr.SetField(inst.CRFD, cr_f);
}

void Interpreter::isync(Interpreter& interpreter, UGeckoInstruction inst)
{
  // shouldn't do anything
}

// the following commands read from FPSCR

void Interpreter::mcrfs(Interpreter& interpreter, UGeckoInstruction inst)
{
  const u32 shift = 4 * (7 - inst.CRFS);
  const u32 fpflags = (PowerPC::ppcState.fpscr.Hex >> shift) & 0xF;

  // If any exception bits were read, clear them
  PowerPC::ppcState.fpscr.Hex &= ~((0xF << shift) & (FPSCR_FX | FPSCR_ANY_X));
  FPSCRUpdated(&PowerPC::ppcState.fpscr);

  PowerPC::ppcState.cr.SetField(inst.CRFD, fpflags);
}

void Interpreter::mffsx(Interpreter& interpreter, UGeckoInstruction inst)
{
  PowerPC::ppcState.ps[inst.FD].SetPS0(UINT64_C(0xFFF8000000000000) | PowerPC::ppcState.fpscr.Hex);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}
