// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Interpreter/Interpreter.h"

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/FPURoundMode.h"
#include "Common/Logging/Log.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/SystemTimers.h"
#include "Core/PowerPC/Interpreter/ExceptionUtils.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

/*

Most of these are together with fctiwx
mffsx: 800c3624
mffsx: 80043c98
mffsx: 8003dd48
mffsx: 8003dd9c
mffsx: 80036608
mffsx: 80036650 (huh?)

*/
// TODO(ector): More proper handling of SSE state.
// That is, set rounding mode etc when entering jit code or the interpreter loop
// Restore rounding mode when calling anything external

static void FPSCRtoFPUSettings(UReg_FPSCR fp)
{
  FPURoundMode::SetRoundMode(fp.RN);

  if (fp.VE || fp.OE || fp.UE || fp.ZE || fp.XE)
  {
    // PanicAlert("FPSCR - exceptions enabled. Please report. VE=%i OE=%i UE=%i ZE=%i XE=%i",
    // fp.VE, fp.OE, fp.UE, fp.ZE, fp.XE);
    // Pokemon Colosseum does this. Gah.
  }

  // Set SSE rounding mode and denormal handling
  FPURoundMode::SetSIMDMode(fp.RN, fp.NI);
}

static void UpdateFPSCR(UReg_FPSCR* fpscr)
{
  fpscr->VX = (fpscr->Hex & FPSCR_VX_ANY) != 0;
  fpscr->FEX = (fpscr->VX & fpscr->VE) | (fpscr->OX & fpscr->OE) | (fpscr->UX & fpscr->UE) |
               (fpscr->ZX & fpscr->ZE) | (fpscr->XX & fpscr->XE);
}

void Interpreter::mtfsb0x(UGeckoInstruction inst)
{
  u32 b = 0x80000000 >> inst.CRBD;

  FPSCR.Hex &= ~b;
  FPSCRtoFPUSettings(FPSCR);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

// This instruction can affect FX
void Interpreter::mtfsb1x(UGeckoInstruction inst)
{
  const u32 bit = inst.CRBD;
  const u32 b = 0x80000000 >> bit;

  if (b & FPSCR_ANY_X)
    SetFPException(&FPSCR, b);
  else
    FPSCR |= b;

  FPSCRtoFPUSettings(FPSCR);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::mtfsfix(UGeckoInstruction inst)
{
  const u32 field = inst.CRFD;
  const u32 pre_shifted_mask = 0xF0000000;
  const u32 mask = (pre_shifted_mask >> (4 * field));
  const u32 imm = (inst.hex << 16) & pre_shifted_mask;

  FPSCR = (FPSCR.Hex & ~mask) | (imm >> (4 * field));

  FPSCRtoFPUSettings(FPSCR);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::mtfsfx(UGeckoInstruction inst)
{
  const u32 fm = inst.FM;
  u32 m = 0;
  for (u32 i = 0; i < 8; i++)
  {
    if (fm & (1U << i))
      m |= (0xFU << (i * 4));
  }

  FPSCR = (FPSCR.Hex & ~m) | (static_cast<u32>(rPS(inst.FB).PS0AsU64()) & m);
  FPSCRtoFPUSettings(FPSCR);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::mcrxr(UGeckoInstruction inst)
{
  PowerPC::ppcState.cr.SetField(inst.CRFD, PowerPC::GetXER().Hex >> 28);
  PowerPC::ppcState.xer_ca = 0;
  PowerPC::ppcState.xer_so_ov = 0;
}

void Interpreter::mfcr(UGeckoInstruction inst)
{
  rGPR[inst.RD] = PowerPC::ppcState.cr.Get();
}

void Interpreter::mtcrf(UGeckoInstruction inst)
{
  const u32 crm = inst.CRM;
  if (crm == 0xFF)
  {
    PowerPC::ppcState.cr.Set(rGPR[inst.RS]);
  }
  else
  {
    // TODO: use lookup table? probably not worth it
    u32 mask = 0;
    for (u32 i = 0; i < 8; i++)
    {
      if (crm & (1U << i))
        mask |= 0xFU << (i * 4);
    }

    PowerPC::ppcState.cr.Set((PowerPC::ppcState.cr.Get() & ~mask) | (rGPR[inst.RS] & mask));
  }
}

void Interpreter::mfmsr(UGeckoInstruction inst)
{
  if (MSR.PR)
  {
    GenerateProgramException();
    return;
  }

  rGPR[inst.RD] = MSR.Hex;
}

void Interpreter::mfsr(UGeckoInstruction inst)
{
  if (MSR.PR)
  {
    GenerateProgramException();
    return;
  }

  rGPR[inst.RD] = PowerPC::ppcState.sr[inst.SR];
}

void Interpreter::mfsrin(UGeckoInstruction inst)
{
  if (MSR.PR)
  {
    GenerateProgramException();
    return;
  }

  const u32 index = (rGPR[inst.RB] >> 28) & 0xF;
  rGPR[inst.RD] = PowerPC::ppcState.sr[index];
}

void Interpreter::mtmsr(UGeckoInstruction inst)
{
  if (MSR.PR)
  {
    GenerateProgramException();
    return;
  }

  MSR.Hex = rGPR[inst.RS];
  PowerPC::CheckExceptions();
  m_end_block = true;
}

// Segment registers. MMU control.

void Interpreter::mtsr(UGeckoInstruction inst)
{
  if (MSR.PR)
  {
    GenerateProgramException();
    return;
  }

  const u32 index = inst.SR;
  const u32 value = rGPR[inst.RS];
  PowerPC::ppcState.SetSR(index, value);
}

void Interpreter::mtsrin(UGeckoInstruction inst)
{
  if (MSR.PR)
  {
    GenerateProgramException();
    return;
  }

  const u32 index = (rGPR[inst.RB] >> 28) & 0xF;
  const u32 value = rGPR[inst.RS];
  PowerPC::ppcState.SetSR(index, value);
}

void Interpreter::mftb(UGeckoInstruction inst)
{
  const u32 index = (inst.TBR >> 5) | ((inst.TBR & 0x1F) << 5);
  DEBUG_ASSERT_MSG(POWERPC, (index == SPR_TL) || (index == SPR_TU), "Invalid mftb");
  (void)index;
  mfspr(inst);
}

void Interpreter::mfspr(UGeckoInstruction inst)
{
  const u32 index = ((inst.SPR & 0x1F) << 5) + ((inst.SPR >> 5) & 0x1F);

  // XER, LR, CTR, and timebase halves are the only ones available in user mode.
  if (MSR.PR && index != SPR_XER && index != SPR_LR && index != SPR_CTR && index != SPR_TL &&
      index != SPR_TU)
  {
    GenerateProgramException();
    return;
  }

  switch (index)
  {
  case SPR_DEC:
    if ((rSPR(index) & 0x80000000) == 0)  // We are still decrementing
    {
      rSPR(index) = SystemTimers::GetFakeDecrementer();
    }
    break;

  case SPR_TL:
  case SPR_TU:
    PowerPC::WriteFullTimeBaseValue(SystemTimers::GetFakeTimeBase());
    break;

  case SPR_WPAR:
  {
    // TODO: If wpar_empty ever is false, Paper Mario hangs. Strange.
    // Maybe WPAR is automatically flushed after a certain amount of time?
    bool wpar_empty = true;  // GPFifo::IsEmpty();
    if (!wpar_empty)
      rSPR(index) |= 1;  // BNE = buffer not empty
    else
      rSPR(index) &= ~1;
  }
  break;
  case SPR_XER:
    rSPR(index) = PowerPC::GetXER().Hex;
    break;
  }
  rGPR[inst.RD] = rSPR(index);
}

void Interpreter::mtspr(UGeckoInstruction inst)
{
  const u32 index = (inst.SPRU << 5) | (inst.SPRL & 0x1F);

  // XER, LR, and CTR are the only ones available to be written to in user mode
  if (MSR.PR && index != SPR_XER && index != SPR_LR && index != SPR_CTR)
  {
    GenerateProgramException();
    return;
  }

  const u32 old_value = rSPR(index);
  rSPR(index) = rGPR[inst.RD];

  // Our DMA emulation is highly inaccurate - instead of properly emulating the queue
  // and so on, we simply make all DMA:s complete instantaneously.

  switch (index)
  {
  case SPR_TL:
  case SPR_TU:
    PanicAlert("Illegal Write to TL/TU");
    break;

  case SPR_TL_W:
    TL = rGPR[inst.RD];
    SystemTimers::TimeBaseSet();
    break;

  case SPR_TU_W:
    TU = rGPR[inst.RD];
    SystemTimers::TimeBaseSet();
    break;

  case SPR_PVR:
    // PVR is a read-only register so maintain its value.
    rSPR(index) = old_value;
    break;

  case SPR_HID0:  // HID0
  {
    UReg_HID0 old_hid0;
    old_hid0.Hex = old_value;
    if (HID0.ICE != old_hid0.ICE)
    {
      INFO_LOG(POWERPC, "Instruction Cache Enable (HID0.ICE) = %d", (int)HID0.ICE);
    }
    if (HID0.ILOCK != old_hid0.ILOCK)
    {
      INFO_LOG(POWERPC, "Instruction Cache Lock (HID0.ILOCK) = %d", (int)HID0.ILOCK);
    }
    if (HID0.ICFI)
    {
      HID0.ICFI = 0;
      INFO_LOG(POWERPC, "Flush Instruction Cache! ICE=%d", (int)HID0.ICE);
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
    rSPR(index) &= 0xF8000000;
    break;

  case SPR_HID2:
    // TODO: disable write gather pipe if WPE not set
    // TODO: emulate locked cache and DMA bits.
    // Only the lower half of the register (upper half from a little endian perspective)
    // is modifiable, except for the DMAQL field.
    rSPR(index) = (rSPR(index) & 0xF0FF0000) | (old_value & 0x0F000000);
    break;

  case SPR_HID4:
    if (old_value != rSPR(index))
    {
      INFO_LOG(POWERPC, "HID4 updated %x %x", old_value, rSPR(index));
      PowerPC::IBATUpdated();
      PowerPC::DBATUpdated();
    }
    break;

  case SPR_WPAR:
    ASSERT_MSG(POWERPC, rGPR[inst.RD] == 0x0C008000, "Gather pipe @ %08x", PC);
    GPFifo::ResetGatherPipe();
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
    if (DMAL.DMA_T)
    {
      const u32 mem_address = DMAU.MEM_ADDR << 5;
      const u32 cache_address = DMAL.LC_ADDR << 5;
      u32 length = ((DMAU.DMA_LEN_U << 2) | DMAL.DMA_LEN_L);

      if (length == 0)
        length = 128;
      if (DMAL.DMA_LD)
        PowerPC::DMA_MemoryToLC(cache_address, mem_address, length);
      else
        PowerPC::DMA_LCToMemory(mem_address, cache_address, length);
    }
    DMAL.DMA_T = 0;
    break;

  case SPR_L2CR:
    break;

  case SPR_DEC:
    if (!(old_value >> 31) && (rGPR[inst.RD] >> 31))  // top bit from 0 to 1
    {
      INFO_LOG(POWERPC, "Software triggered Decrementer exception");
      PowerPC::ppcState.Exceptions |= EXCEPTION_DECREMENTER;
    }
    SystemTimers::DecrementerSet();
    break;

  // Page table base etc
  case SPR_SDR:
    PowerPC::SDRUpdated();
    break;

  case SPR_XER:
    PowerPC::SetXER(UReg_XER{rSPR(index)});
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
    if (old_value != rSPR(index))
    {
      INFO_LOG(POWERPC, "DBAT updated %u %x %x", index, old_value, rSPR(index));
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
    if (old_value != rSPR(index))
    {
      INFO_LOG(POWERPC, "IBAT updated %u %x %x", index, old_value, rSPR(index));
      PowerPC::IBATUpdated();
    }
    break;
  }
}

void Interpreter::crand(UGeckoInstruction inst)
{
  PowerPC::ppcState.cr.SetBit(inst.CRBD, PowerPC::ppcState.cr.GetBit(inst.CRBA) &
                                             PowerPC::ppcState.cr.GetBit(inst.CRBB));
}

void Interpreter::crandc(UGeckoInstruction inst)
{
  PowerPC::ppcState.cr.SetBit(inst.CRBD, PowerPC::ppcState.cr.GetBit(inst.CRBA) &
                                             (1 ^ PowerPC::ppcState.cr.GetBit(inst.CRBB)));
}

void Interpreter::creqv(UGeckoInstruction inst)
{
  PowerPC::ppcState.cr.SetBit(inst.CRBD, 1 ^ (PowerPC::ppcState.cr.GetBit(inst.CRBA) ^
                                              PowerPC::ppcState.cr.GetBit(inst.CRBB)));
}

void Interpreter::crnand(UGeckoInstruction inst)
{
  PowerPC::ppcState.cr.SetBit(inst.CRBD, 1 ^ (PowerPC::ppcState.cr.GetBit(inst.CRBA) &
                                              PowerPC::ppcState.cr.GetBit(inst.CRBB)));
}

void Interpreter::crnor(UGeckoInstruction inst)
{
  PowerPC::ppcState.cr.SetBit(inst.CRBD, 1 ^ (PowerPC::ppcState.cr.GetBit(inst.CRBA) |
                                              PowerPC::ppcState.cr.GetBit(inst.CRBB)));
}

void Interpreter::cror(UGeckoInstruction inst)
{
  PowerPC::ppcState.cr.SetBit(
      inst.CRBD, (PowerPC::ppcState.cr.GetBit(inst.CRBA) | PowerPC::ppcState.cr.GetBit(inst.CRBB)));
}

void Interpreter::crorc(UGeckoInstruction inst)
{
  PowerPC::ppcState.cr.SetBit(inst.CRBD, (PowerPC::ppcState.cr.GetBit(inst.CRBA) |
                                          (1 ^ PowerPC::ppcState.cr.GetBit(inst.CRBB))));
}

void Interpreter::crxor(UGeckoInstruction inst)
{
  PowerPC::ppcState.cr.SetBit(
      inst.CRBD, (PowerPC::ppcState.cr.GetBit(inst.CRBA) ^ PowerPC::ppcState.cr.GetBit(inst.CRBB)));
}

void Interpreter::mcrf(UGeckoInstruction inst)
{
  const u32 cr_f = PowerPC::ppcState.cr.GetField(inst.CRFS);
  PowerPC::ppcState.cr.SetField(inst.CRFD, cr_f);
}

void Interpreter::isync(UGeckoInstruction inst)
{
  // shouldn't do anything
}

// the following commands read from FPSCR

void Interpreter::mcrfs(UGeckoInstruction inst)
{
  UpdateFPSCR(&FPSCR);
  u32 fpflags = ((FPSCR.Hex >> (4 * (7 - inst.CRFS))) & 0xF);
  switch (inst.CRFS)
  {
  case 0:
    FPSCR.FX = 0;
    FPSCR.OX = 0;
    break;
  case 1:
    FPSCR.UX = 0;
    FPSCR.ZX = 0;
    FPSCR.XX = 0;
    FPSCR.VXSNAN = 0;
    break;
  case 2:
    FPSCR.VXISI = 0;
    FPSCR.VXIDI = 0;
    FPSCR.VXZDZ = 0;
    FPSCR.VXIMZ = 0;
    break;
  case 3:
    FPSCR.VXVC = 0;
    break;
  case 5:
    FPSCR.VXSOFT = 0;
    FPSCR.VXSQRT = 0;
    FPSCR.VXCVI = 0;
    break;
  }
  PowerPC::ppcState.cr.SetField(inst.CRFD, fpflags);
}

void Interpreter::mffsx(UGeckoInstruction inst)
{
  // load from FPSCR
  // TODO(ector): grab all overflow flags etc and set them in FPSCR

  UpdateFPSCR(&FPSCR);
  rPS(inst.FD).SetPS0(UINT64_C(0xFFF8000000000000) | FPSCR.Hex);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}
