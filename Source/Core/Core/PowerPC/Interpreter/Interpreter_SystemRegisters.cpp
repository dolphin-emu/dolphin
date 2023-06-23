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

static void FPSCRUpdated(PowerPC::PowerPCState& ppc_state)
{
  UpdateFPExceptionSummary(ppc_state);
  PowerPC::RoundingModeUpdated(ppc_state);
}

void Interpreter::mtfsb0x(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  u32 b = 0x80000000 >> inst.CRBD;

  ppc_state.fpscr.Hex &= ~b;
  FPSCRUpdated(ppc_state);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

// This instruction can affect FX
void Interpreter::mtfsb1x(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 bit = inst.CRBD;
  const u32 b = 0x80000000 >> bit;

  if ((b & FPSCR_ANY_X) != 0)
    SetFPException(ppc_state, b);
  else
    ppc_state.fpscr |= b;

  FPSCRUpdated(ppc_state);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::mtfsfix(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 field = inst.CRFD;
  const u32 pre_shifted_mask = 0xF0000000;
  const u32 mask = (pre_shifted_mask >> (4 * field));
  const u32 imm = (inst.hex << 16) & pre_shifted_mask;

  ppc_state.fpscr = (ppc_state.fpscr.Hex & ~mask) | (imm >> (4 * field));

  FPSCRUpdated(ppc_state);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::mtfsfx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 fm = inst.FM;
  u32 m = 0;
  for (u32 i = 0; i < 8; i++)
  {
    if ((fm & (1U << i)) != 0)
      m |= (0xFU << (i * 4));
  }

  ppc_state.fpscr =
      (ppc_state.fpscr.Hex & ~m) | (static_cast<u32>(ppc_state.ps[inst.FB].PS0AsU64()) & m);
  FPSCRUpdated(ppc_state);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::mcrxr(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  ppc_state.cr.SetField(inst.CRFD, ppc_state.GetXER().Hex >> 28);
  ppc_state.xer_ca = 0;
  ppc_state.xer_so_ov = 0;
}

void Interpreter::mfcr(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  ppc_state.gpr[inst.RD] = ppc_state.cr.Get();
}

void Interpreter::mtcrf(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 crm = inst.CRM;
  if (crm == 0xFF)
  {
    ppc_state.cr.Set(ppc_state.gpr[inst.RS]);
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

    ppc_state.cr.Set((ppc_state.cr.Get() & ~mask) | (ppc_state.gpr[inst.RS] & mask));
  }
}

void Interpreter::mfmsr(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  if (ppc_state.msr.PR)
  {
    GenerateProgramException(ppc_state, ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  ppc_state.gpr[inst.RD] = ppc_state.msr.Hex;
}

void Interpreter::mfsr(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  if (ppc_state.msr.PR)
  {
    GenerateProgramException(ppc_state, ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  ppc_state.gpr[inst.RD] = ppc_state.sr[inst.SR];
}

void Interpreter::mfsrin(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  if (ppc_state.msr.PR)
  {
    GenerateProgramException(ppc_state, ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  const u32 index = (ppc_state.gpr[inst.RB] >> 28) & 0xF;
  ppc_state.gpr[inst.RD] = ppc_state.sr[index];
}

void Interpreter::mtmsr(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  if (ppc_state.msr.PR)
  {
    GenerateProgramException(ppc_state, ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  ppc_state.msr.Hex = ppc_state.gpr[inst.RS];

  PowerPC::MSRUpdated(ppc_state);

  // FE0/FE1 may have been set
  CheckFPExceptions(ppc_state);

  interpreter.m_system.GetPowerPC().CheckExceptions();
  interpreter.m_end_block = true;
}

// Segment registers. MMU control.

void Interpreter::mtsr(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  if (ppc_state.msr.PR)
  {
    GenerateProgramException(ppc_state, ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  const u32 index = inst.SR;
  const u32 value = ppc_state.gpr[inst.RS];
  ppc_state.SetSR(index, value);
}

void Interpreter::mtsrin(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  if (ppc_state.msr.PR)
  {
    GenerateProgramException(ppc_state, ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  const u32 index = (ppc_state.gpr[inst.RB] >> 28) & 0xF;
  const u32 value = ppc_state.gpr[inst.RS];
  ppc_state.SetSR(index, value);
}

void Interpreter::mftb(Interpreter& interpreter, UGeckoInstruction inst)
{
  [[maybe_unused]] const u32 index = (inst.TBR >> 5) | ((inst.TBR & 0x1F) << 5);
  DEBUG_ASSERT_MSG(POWERPC, (index == SPR_TL) || (index == SPR_TU), "Invalid mftb");
  mfspr(interpreter, inst);
}

void Interpreter::mfspr(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 index = ((inst.SPR & 0x1F) << 5) + ((inst.SPR >> 5) & 0x1F);

  // XER, LR, CTR, and timebase halves are the only ones available in user mode.
  if (ppc_state.msr.PR && index != SPR_XER && index != SPR_LR && index != SPR_CTR &&
      index != SPR_TL && index != SPR_TU)
  {
    GenerateProgramException(ppc_state, ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  switch (index)
  {
  case SPR_DEC:
    if ((ppc_state.spr[index] & 0x80000000) == 0)  // We are still decrementing
    {
      ppc_state.spr[index] = SystemTimers::GetFakeDecrementer();
    }
    break;

  case SPR_TL:
  case SPR_TU:
    interpreter.m_system.GetPowerPC().WriteFullTimeBaseValue(SystemTimers::GetFakeTimeBase());
    break;

  case SPR_WPAR:
  {
    // The bottom, read-only bit checks if the buffer is not empty.
    // GXRedirectWriteGatherPipe and GXRestoreWriteGatherPipe (used for display lists) wait for
    // this bit to be cleared before writing to SPR_WPAR again (with a value of 0x0c00800 (aka
    // GPFifo::GATHER_PIPE_PHYSICAL_ADDRESS)).
    // Currently, we always treat the buffer as not empty, as the exact behavior is unclear
    // (and games that use display lists will hang if the bit doesn't eventually become zero).
    if (interpreter.m_system.GetGPFifo().IsBNE())
      ppc_state.spr[index] |= 1;
    else
      ppc_state.spr[index] &= ~1;
  }
  break;

  case SPR_XER:
    ppc_state.spr[index] = ppc_state.GetXER().Hex;
    break;

  case SPR_UPMC1:
    ppc_state.spr[index] = ppc_state.spr[SPR_PMC1];
    break;

  case SPR_UPMC2:
    ppc_state.spr[index] = ppc_state.spr[SPR_PMC2];
    break;

  case SPR_UPMC3:
    ppc_state.spr[index] = ppc_state.spr[SPR_PMC3];
    break;

  case SPR_UPMC4:
    ppc_state.spr[index] = ppc_state.spr[SPR_PMC4];
    break;

  case SPR_IABR:
    // A strange quirk: reading back this register on hardware will always have the TE (Translation
    // enabled) bit set to 0 (despite the bit appearing to function normally when set). This does
    // not apply to the DABR.
    ppc_state.gpr[inst.RD] = ppc_state.spr[index] & ~1;
    return;
  }
  ppc_state.gpr[inst.RD] = ppc_state.spr[index];
}

void Interpreter::mtspr(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 index = (inst.SPRU << 5) | (inst.SPRL & 0x1F);

  // XER, LR, and CTR are the only ones available to be written to in user mode
  if (ppc_state.msr.PR && index != SPR_XER && index != SPR_LR && index != SPR_CTR)
  {
    GenerateProgramException(ppc_state, ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  const u32 old_value = ppc_state.spr[index];
  ppc_state.spr[index] = ppc_state.gpr[inst.RD];

  // Our DMA emulation is highly inaccurate - instead of properly emulating the queue
  // and so on, we simply make all DMA:s complete instantaneously.

  switch (index)
  {
  case SPR_TL:
  case SPR_TU:
    PanicAlertFmt("Illegal Write to TL/TU");
    break;

  case SPR_TL_W:
    TL(ppc_state) = ppc_state.gpr[inst.RD];
    SystemTimers::TimeBaseSet();
    break;

  case SPR_TU_W:
    TU(ppc_state) = ppc_state.gpr[inst.RD];
    SystemTimers::TimeBaseSet();
    break;

  case SPR_PVR:
    // PVR is a read-only register so maintain its value.
    ppc_state.spr[index] = old_value;
    break;

  case SPR_HID0:  // HID0
  {
    UReg_HID0 old_hid0;
    old_hid0.Hex = old_value;
    if (HID0(ppc_state).ICE != old_hid0.ICE)
    {
      INFO_LOG_FMT(POWERPC, "Instruction Cache Enable (HID0.ICE) = {}", HID0(ppc_state).ICE);
    }
    if (HID0(ppc_state).ILOCK != old_hid0.ILOCK)
    {
      INFO_LOG_FMT(POWERPC, "Instruction Cache Lock (HID0.ILOCK) = {}", HID0(ppc_state).ILOCK);
    }
    if (HID0(ppc_state).ICFI)
    {
      HID0(ppc_state).ICFI = 0;
      INFO_LOG_FMT(POWERPC, "Flush Instruction Cache! ICE={}", HID0(ppc_state).ICE);
      // this is rather slow
      // most games do it only once during initialization
      ppc_state.iCache.Reset();
    }
  }
  break;

  case SPR_HID1:
    // Despite being documented as a read-only register, it actually isn't. Bits
    // 0-4 (27-31 from a little endian perspective) are modifiable. The rest are not
    // affected, as those bits are reserved and ignore writes to them.
    ppc_state.spr[index] &= 0xF8000000;
    break;

  case SPR_HID2:
    // TODO: disable write gather pipe if WPE not set
    // TODO: emulate locked cache and DMA bits.
    // Only the lower half of the register (upper half from a little endian perspective)
    // is modifiable, except for the DMAQL field.
    ppc_state.spr[index] = (ppc_state.spr[index] & 0xF0FF0000) | (old_value & 0x0F000000);
    break;

  case SPR_HID4:
    if (old_value != ppc_state.spr[index])
    {
      INFO_LOG_FMT(POWERPC, "HID4 updated {:x} {:x}", old_value, ppc_state.spr[index]);
      interpreter.m_mmu.IBATUpdated();
      interpreter.m_mmu.DBATUpdated();
    }
    break;

  case SPR_WPAR:
    ASSERT_MSG(POWERPC, ppc_state.spr[SPR_WPAR] == GPFifo::GATHER_PIPE_PHYSICAL_ADDRESS,
               "Gather pipe changed to unexpected address {:08x} @ PC {:08x}",
               ppc_state.spr[SPR_WPAR], ppc_state.pc);
    interpreter.m_system.GetGPFifo().ResetGatherPipe();
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
    if (DMAL(ppc_state).DMA_T)
    {
      const u32 mem_address = DMAU(ppc_state).MEM_ADDR << 5;
      const u32 cache_address = DMAL(ppc_state).LC_ADDR << 5;
      u32 length = ((DMAU(ppc_state).DMA_LEN_U << 2) | DMAL(ppc_state).DMA_LEN_L);

      if (length == 0)
        length = 128;
      if (DMAL(ppc_state).DMA_LD)
        interpreter.m_mmu.DMA_MemoryToLC(cache_address, mem_address, length);
      else
        interpreter.m_mmu.DMA_LCToMemory(mem_address, cache_address, length);
    }
    DMAL(ppc_state).DMA_T = 0;
    break;

  case SPR_L2CR:
    break;

  case SPR_DEC:
    // Top bit from 0 to 1
    if ((old_value >> 31) == 0 && (ppc_state.gpr[inst.RD] >> 31) != 0)
    {
      INFO_LOG_FMT(POWERPC, "Software triggered Decrementer exception");
      ppc_state.Exceptions |= EXCEPTION_DECREMENTER;
    }
    SystemTimers::DecrementerSet();
    break;

  // Page table base etc
  case SPR_SDR:
    interpreter.m_mmu.SDRUpdated();
    break;

  case SPR_XER:
    ppc_state.SetXER(UReg_XER{ppc_state.spr[index]});
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
    if (old_value != ppc_state.spr[index])
    {
      INFO_LOG_FMT(POWERPC, "DBAT updated {} {:x} {:x}", index, old_value, ppc_state.spr[index]);
      interpreter.m_mmu.DBATUpdated();
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
    if (old_value != ppc_state.spr[index])
    {
      INFO_LOG_FMT(POWERPC, "IBAT updated {} {:x} {:x}", index, old_value, ppc_state.spr[index]);
      interpreter.m_mmu.IBATUpdated();
    }
    break;

  case SPR_MMCR0:
  case SPR_MMCR1:
    MMCRUpdated(ppc_state);
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

    auto UpdateThermalReg = [&ppc_state](UReg_THRM12* reg) {
      if (!THRM3(ppc_state).E || !reg->V)
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

    UpdateThermalReg(&THRM1(ppc_state));
    UpdateThermalReg(&THRM2(ppc_state));
    break;
  }
  }
}

void Interpreter::crand(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 a = ppc_state.cr.GetBit(inst.CRBA);
  const u32 b = ppc_state.cr.GetBit(inst.CRBB);

  ppc_state.cr.SetBit(inst.CRBD, a & b);
}

void Interpreter::crandc(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 a = ppc_state.cr.GetBit(inst.CRBA);
  const u32 b = ppc_state.cr.GetBit(inst.CRBB);

  ppc_state.cr.SetBit(inst.CRBD, a & (1 ^ b));
}

void Interpreter::creqv(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 a = ppc_state.cr.GetBit(inst.CRBA);
  const u32 b = ppc_state.cr.GetBit(inst.CRBB);

  ppc_state.cr.SetBit(inst.CRBD, 1 ^ (a ^ b));
}

void Interpreter::crnand(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 a = ppc_state.cr.GetBit(inst.CRBA);
  const u32 b = ppc_state.cr.GetBit(inst.CRBB);

  ppc_state.cr.SetBit(inst.CRBD, 1 ^ (a & b));
}

void Interpreter::crnor(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 a = ppc_state.cr.GetBit(inst.CRBA);
  const u32 b = ppc_state.cr.GetBit(inst.CRBB);

  ppc_state.cr.SetBit(inst.CRBD, 1 ^ (a | b));
}

void Interpreter::cror(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 a = ppc_state.cr.GetBit(inst.CRBA);
  const u32 b = ppc_state.cr.GetBit(inst.CRBB);

  ppc_state.cr.SetBit(inst.CRBD, a | b);
}

void Interpreter::crorc(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 a = ppc_state.cr.GetBit(inst.CRBA);
  const u32 b = ppc_state.cr.GetBit(inst.CRBB);

  ppc_state.cr.SetBit(inst.CRBD, a | (1 ^ b));
}

void Interpreter::crxor(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 a = ppc_state.cr.GetBit(inst.CRBA);
  const u32 b = ppc_state.cr.GetBit(inst.CRBB);

  ppc_state.cr.SetBit(inst.CRBD, a ^ b);
}

void Interpreter::mcrf(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 cr_f = ppc_state.cr.GetField(inst.CRFS);
  ppc_state.cr.SetField(inst.CRFD, cr_f);
}

void Interpreter::isync(Interpreter& interpreter, UGeckoInstruction inst)
{
  // shouldn't do anything
}

// the following commands read from FPSCR

void Interpreter::mcrfs(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 shift = 4 * (7 - inst.CRFS);
  const u32 fpflags = (ppc_state.fpscr.Hex >> shift) & 0xF;

  // If any exception bits were read, clear them
  ppc_state.fpscr.Hex &= ~((0xF << shift) & (FPSCR_FX | FPSCR_ANY_X));
  FPSCRUpdated(ppc_state);

  ppc_state.cr.SetField(inst.CRFD, fpflags);
}

void Interpreter::mffsx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  ppc_state.ps[inst.FD].SetPS0(UINT64_C(0xFFF8000000000000) | ppc_state.fpscr.Hex);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}
