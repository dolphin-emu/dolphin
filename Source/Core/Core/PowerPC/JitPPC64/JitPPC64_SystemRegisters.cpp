// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitPPC64/Jit.h"

#include "Common/CommonTypes.h"
#include "Common/PPC64Emitter.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

using namespace PPC64Gen;

// SPRs whose reads and writes in the interpreter have no side effects are accessed directly in
// PowerPCState, as are the time base halves, from CoreTiming; the interpreter handles the others.
// It reads them without flushing the registers; mtspr ends the block, which flushes them anyway.
// As in the other JITs, accesses to privileged SPRs in user mode don't raise exceptions.

namespace
{
bool HasPlainRead(u32 spr)
{
  switch (spr)
  {
  case SPR_DEC:
  case SPR_TL:
  case SPR_TU:
  case SPR_WPAR:
  case SPR_XER:
  case SPR_UPMC1:
  case SPR_UPMC2:
  case SPR_UPMC3:
  case SPR_UPMC4:
  case SPR_IABR:
    return false;
  default:
    return true;
  }
}

bool HasPlainWrite(u32 spr)
{
  switch (spr)
  {
  case SPR_SRR0:
  case SPR_SRR1:
  case SPR_SPRG0:
  case SPR_SPRG1:
  case SPR_SPRG2:
  case SPR_SPRG3:
  case SPR_DAR:
  case SPR_DSISR:
    return true;
  default:
    return spr >= SPR_GQR0 && spr < SPR_GQR0 + 8;
  }
}
}  // namespace

void JitPPC64::EmitTimeBase(GPR out)
{
  // As SystemTimersManager::GetFakeTimeBase, inline, since timer-heavy games read it often:
  // fake_TB_start_value + (CoreTiming::GetTicks() - fake_TB_start_ticks) / TIMER_RATIO, where
  // GetTicks() is global_timer + slice_length - int(float(downcount) * last_OC_factor_inverted).
  // Like JitArm64, this doesn't add the cycles of the block so far.
  auto& globals = m_system.GetCoreTiming().GetGlobals();
  const auto base = gpr.GetScopedReg();
  const auto tmp = gpr.GetScopedReg();
  const auto cycles = fpr.GetScopedReg();
  const auto factor = fpr.GetScopedReg();
  MOVP2R(base, &globals);

  MTVSRD(cycles, DOWNCOUNT_REG);
  XSCVSXDDP(cycles, cycles);
  XSRSP(cycles, cycles);
  LI(tmp, static_cast<s16>(offsetof(CoreTiming::Globals, last_OC_factor_inverted)));
  LXSSPX(factor, base, tmp);
  XSMULSP(cycles, cycles, factor);
  XSCVDPSXDS(cycles, cycles);
  MFVSRD(out, cycles);

  LWA(tmp, base, static_cast<s16>(offsetof(CoreTiming::Globals, slice_length)));
  SUBF(out, out, tmp);
  LD(tmp, base, static_cast<s16>(offsetof(CoreTiming::Globals, global_timer)));
  ADD(out, out, tmp);
  LD(tmp, base, static_cast<s16>(offsetof(CoreTiming::Globals, fake_TB_start_ticks)));
  SUBF(out, tmp, out);

  // x / 12 = (x * 0xAAAAAAAAAAAAAAAB) >> 67
  static_assert(SystemTimers::TIMER_RATIO == 12);
  MOVI2R(tmp, 0xAAAAAAAAAAAAAAABULL);
  MULHDU(out, out, tmp);
  SRDI(out, out, 3);
  LD(tmp, base, static_cast<s16>(offsetof(CoreTiming::Globals, fake_TB_start_value)));
  ADD(out, out, tmp);
}

void JitPPC64::mfspr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  const u32 spr = ((inst.SPR & 0x1F) << 5) | ((inst.SPR >> 5) & 0x1F);
  switch (spr)
  {
  case SPR_LR:
  case SPR_CTR:
    LWZ(gpr.RW(inst.RD), PPC_REG, PPCSTATE_OFF_SPR(spr));
    break;

  case SPR_TL:
  case SPR_TU:
  {
    // The interpreter also updates both halves in PowerPCState.
    const auto tb = gpr.GetScopedReg();
    EmitTimeBase(tb);
    static_assert(PPCSTATE_OFF_SPR(SPR_TU) == PPCSTATE_OFF_SPR(SPR_TL) + 4);
    STD(tb, PPC_REG, PPCSTATE_OFF_SPR(SPR_TL));
    if (spr == SPR_TU)
      SRDI(gpr.RW(inst.RD), tb, 32);
    else
      MR(gpr.RW(inst.RD), tb);
    break;
  }

  case SPR_XER:
  {
    // XER = stringctrl | CA << 29 | (SO << 1 | OV) << 30
    const GPR d = gpr.RW(inst.RD);
    const auto tmp = gpr.GetScopedReg();
    LHZ(d, PPC_REG, PPCSTATE_OFF(xer_stringctrl));
    LBZ(tmp, PPC_REG, PPCSTATE_OFF(xer_ca));
    RLWIMI(d, tmp, XER_CA_SHIFT, 31 - XER_CA_SHIFT, 31 - XER_CA_SHIFT);
    LBZ(tmp, PPC_REG, PPCSTATE_OFF(xer_so_ov));
    RLWIMI(d, tmp, XER_OV_SHIFT, 0, 31 - XER_OV_SHIFT);
    break;
  }

  default:
    if (HasPlainRead(spr))
      LWZ(gpr.RW(inst.RD), PPC_REG, PPCSTATE_OFF_SPR(spr));
    else
      InterpretInstruction(inst);
    break;
  }
}

void JitPPC64::mtspr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  const u32 spr = ((inst.SPR & 0x1F) << 5) | ((inst.SPR >> 5) & 0x1F);
  switch (spr)
  {
  case SPR_LR:
  case SPR_CTR:
    STW(gpr.R(inst.RD), PPC_REG, PPCSTATE_OFF_SPR(spr));
    break;

  case SPR_XER:
  {
    // As PowerPCState::SetXER: stringctrl is the byte count and compare byte, then CA, SO and OV.
    const GPR s = gpr.R(inst.RD);
    const auto tmp = gpr.GetScopedReg();
    RLWINM(tmp, s, 0, 16, 31);  // Bits 0-6 are the byte count, 8-15 the compare byte
    ANDI_(tmp, tmp, 0xFF7F);
    STH(tmp, PPC_REG, PPCSTATE_OFF(xer_stringctrl));
    RLWINM(tmp, s, 32 - XER_CA_SHIFT, 31, 31);
    STB(tmp, PPC_REG, PPCSTATE_OFF(xer_ca));
    RLWINM(tmp, s, 32 - XER_OV_SHIFT, 30, 31);
    STB(tmp, PPC_REG, PPCSTATE_OFF(xer_so_ov));
    break;
  }

  default:
    if (HasPlainWrite(spr))
      STW(gpr.R(inst.RD), PPC_REG, PPCSTATE_OFF_SPR(spr));
    else
      FallBackToInterpreter(inst);
    break;
  }
}

void JitPPC64::mfcr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  const GPR d = gpr.RW(inst.RD);
  const auto field = gpr.GetScopedReg();
  for (u32 i = 0; i < 8; i++)
  {
    GetCRField(field, i);
    if (i == 0)
      SLWI(d, field, 28);
    else
      RLWIMI(d, field, 28 - 4 * i, 4 * i, 4 * i + 3);
  }
}

void JitPPC64::mtcrf(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  const GPR s = gpr.R(inst.RS);
  const auto field = gpr.GetScopedReg();
  for (u32 i = 0; i < 8; i++)
  {
    if ((inst.CRM & (0x80 >> i)) == 0)
      continue;

    RLWINM(field, s, (4 * i + 4) % 32, 28, 31);
    SetCRField(i, field);
  }
}

void JitPPC64::mcrf(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  if (inst.CRFS == inst.CRFD)
    return;

  const GPR s = gpr.CR(inst.CRFS);
  MR(gpr.CRW(inst.CRFD), s);
}

void JitPPC64::crXXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  // Work on the four PowerPC bits of each field, as the interpreter does.
  const auto a = gpr.GetScopedReg();
  const auto b = gpr.GetScopedReg();
  const auto d = gpr.GetScopedReg();

  // Bit n of a field (LT = 0) is bit 3 - n of its four bits.
  GetCRField(a, inst.CRBA >> 2);
  RLWINM(a, a, (32 - (3 - (inst.CRBA & 3))) % 32, 31, 31);
  GetCRField(b, inst.CRBB >> 2);
  RLWINM(b, b, (32 - (3 - (inst.CRBB & 3))) % 32, 31, 31);

  switch (inst.SUBOP10)
  {
  case 257:  // crand
    AND(a, a, b);
    break;
  case 129:  // crandc
    ANDC(a, a, b);
    break;
  case 289:  // creqv
    EQV(a, a, b);
    break;
  case 225:  // crnand
    NAND(a, a, b);
    break;
  case 33:  // crnor
    NOR(a, a, b);
    break;
  case 449:  // cror
    OR(a, a, b);
    break;
  case 417:  // crorc
    ORC(a, a, b);
    break;
  case 193:  // crxor
    XOR(a, a, b);
    break;
  }

  const u32 bit = 3 - (inst.CRBD & 3);
  GetCRField(d, inst.CRBD >> 2);
  RLWIMI(d, a, bit, 31 - bit, 31 - bit);
  SetCRField(inst.CRBD >> 2, d);
}

void JitPPC64::mcrxr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  // CR field = XER[SO, OV, CA, 0], which are then cleared.
  const auto bits = gpr.GetScopedReg();
  const auto tmp = gpr.GetScopedReg();
  LBZ(bits, PPC_REG, PPCSTATE_OFF(xer_so_ov));
  RLWINM(bits, bits, 2, 28, 29);
  LBZ(tmp, PPC_REG, PPCSTATE_OFF(xer_ca));
  RLWIMI(bits, tmp, 1, 30, 30);
  SetCRField(inst.CRFD, bits);
  LI(tmp, 0);
  STB(tmp, PPC_REG, PPCSTATE_OFF(xer_ca));
  STB(tmp, PPC_REG, PPCSTATE_OFF(xer_so_ov));
}

void JitPPC64::mfsr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  // mfsr, or mfsrin with the segment from rB. As with the SPRs, user mode raises no exception.
  if (inst.SUBOP10 == 595)
  {
    LWZ(gpr.RW(inst.RD), PPC_REG, PPCSTATE_OFF_ARRAY(sr, inst.SR));
    return;
  }
  const auto offset = gpr.GetScopedReg();
  RLWINM(offset, gpr.R(inst.RB), 4 + 2, 26, 29);
  ADD(offset, offset, PPC_REG);
  LWZ(gpr.RW(inst.RD), offset, PPCSTATE_OFF(sr));
}

void JitPPC64::EmitFPSCRUpdated(GPR fpscr, bool rounding_mode_changed)
{
  // As FPSCRUpdated: VX and FEX summarize the exception bits, then FPSCR is stored and the host's
  // rounding mode follows RN. Bit 11 is reserved, and always clear. As in the other JITs, a set FEX
  // raises no exception: with floating-point exceptions enabled, the interpreter runs these
  // instructions instead.
  RLWINM(fpscr, fpscr, 0, 21, 19);
  {
    const auto tmp = gpr.GetScopedReg();
    const auto tmp2 = gpr.GetScopedReg();
    MOVI2R(tmp, FPSCR_VX_ANY);
    AND(tmp, fpscr, tmp);
    NEG(tmp2, tmp);
    OR(tmp, tmp, tmp2);
    SRDI(tmp, tmp, 63);
    RLWIMI(fpscr, tmp, 31 - 2, 2, 2);  // VX
    SRWI(tmp, fpscr, 22);
    AND(tmp, tmp, fpscr);
    ANDI_(tmp, tmp, FPSCR_ANY_E);
    NEG(tmp2, tmp);
    OR(tmp, tmp, tmp2);
    SRDI(tmp, tmp, 63);
    RLWIMI(fpscr, tmp, 31 - 1, 1, 1);  // FEX
  }
  STW(fpscr, PPC_REG, PPCSTATE_OFF(fpscr));

  if (rounding_mode_changed)
  {
    const BitSet32 saved = SaveScratchRegs();
    ABI_CallFunction(&PowerPC::RoundingModeUpdated, &m_ppc_state);
    RestoreScratchRegs(saved);
    // And NI, which CR4 holds.
    m_ni_loaded = false;
  }
}

void JitPPC64::mcrfs(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  INTERPRET_IF(jo.fp_exceptions);

  // The CR field gets the FPSCR field, whose exception bits are then cleared.
  const u32 shift = 4 * (7 - inst.CRFS);
  const u32 clear = (0xFu << shift) & (FPSCR_FX | FPSCR_ANY_X);
  const auto fpscr = gpr.GetScopedReg();
  LWZ(fpscr, PPC_REG, PPCSTATE_OFF(fpscr));
  {
    const auto bits = gpr.GetScopedReg();
    RLWINM(bits, fpscr, (32 - shift) % 32, 28, 31);
    SetCRField(inst.CRFD, bits);
    if (clear != 0)
    {
      MOVI2R(bits, clear);
      ANDC(fpscr, fpscr, bits);
    }
  }
  EmitFPSCRUpdated(fpscr, false);
}

void JitPPC64::mffsx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  INTERPRET_IF(inst.Rc);

  // ps0 = 0xFFF8000000000000 | FPSCR, and ps1 is kept.
  const auto value = gpr.GetScopedReg();
  const auto high = gpr.GetScopedReg();
  LWZ(value, PPC_REG, PPCSTATE_OFF(fpscr));
  LI(high, -8);
  SLDI(high, high, 48);
  OR(value, value, high);
  const auto v = fpr.GetScopedReg();
  MTVSRD(v, value);
  const VSR d = fpr.RW(inst.FD, true);
  XXPERMDI(d, v, d, 1);
}

void JitPPC64::mtfsb0x(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  INTERPRET_IF(inst.Rc || jo.fp_exceptions);

  const auto fpscr = gpr.GetScopedReg();
  LWZ(fpscr, PPC_REG, PPCSTATE_OFF(fpscr));
  RLWINM(fpscr, fpscr, 0, (inst.CRBD + 1) % 32, (inst.CRBD + 31) % 32);
  EmitFPSCRUpdated(fpscr, inst.CRBD >= 29);
}

void JitPPC64::mtfsb1x(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  INTERPRET_IF(inst.Rc || jo.fp_exceptions);

  const u32 bit = inst.CRBD;
  const u32 mask = 0x80000000u >> bit;
  const auto fpscr = gpr.GetScopedReg();
  LWZ(fpscr, PPC_REG, PPCSTATE_OFF(fpscr));
  if ((mask & FPSCR_ANY_X) != 0)
  {
    // As SetFPException: setting an exception bit that is clear sets FX.
    const auto fx = gpr.GetScopedReg();
    RLWINM(fx, fpscr, bit + 1, 31, 31);
    XORI(fx, fx, 1);
    SLWI(fx, fx, 31);
    OR(fpscr, fpscr, fx);
  }
  if (mask >> 16)
    ORIS(fpscr, fpscr, static_cast<u16>(mask >> 16));
  else
    ORI(fpscr, fpscr, static_cast<u16>(mask));
  EmitFPSCRUpdated(fpscr, bit >= 29);
}

void JitPPC64::mtfsfix(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  INTERPRET_IF(inst.Rc || jo.fp_exceptions);

  const u32 field = inst.CRFD;
  const auto fpscr = gpr.GetScopedReg();
  LWZ(fpscr, PPC_REG, PPCSTATE_OFF(fpscr));
  {
    const auto imm = gpr.GetScopedReg();
    LI(imm, static_cast<s16>((inst.hex >> 12) & 0xF));
    RLWIMI(fpscr, imm, 28 - 4 * field, 4 * field, 4 * field + 3);
  }
  // Field 7 has RN and NI.
  EmitFPSCRUpdated(fpscr, field == 7);
}

void JitPPC64::mtfsfx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  INTERPRET_IF(inst.Rc || jo.fp_exceptions);

  // The fields that FM selects, from the low word of ps0 of frB.
  u32 mask = 0;
  for (u32 i = 0; i < 8; i++)
  {
    if ((inst.FM & (1u << i)) != 0)
      mask |= 0xFu << (4 * i);
  }
  const auto fpscr = gpr.GetScopedReg();
  MFVSRD(fpscr, fpr.R(inst.FB));
  if (mask != 0xFFFFFFFF)
  {
    const auto old = gpr.GetScopedReg();
    const auto m = gpr.GetScopedReg();
    LWZ(old, PPC_REG, PPCSTATE_OFF(fpscr));
    MOVI2R(m, mask);
    AND(fpscr, fpscr, m);
    ANDC(old, old, m);
    OR(fpscr, fpscr, old);
  }
  EmitFPSCRUpdated(fpscr, (inst.FM & 1) != 0);
}

void JitPPC64::mfmsr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  LWZ(gpr.RW(inst.RD), PPC_REG, PPCSTATE_OFF(msr));
}

void JitPPC64::mtmsr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  // Enabling floating-point exceptions (FE0, FE1) may raise one.
  FALLBACK_IF(jo.fp_exceptions);

  const GPR s = gpr.R(inst.RS);
  STW(s, PPC_REG, PPCSTATE_OFF(msr));
  EmitMSRUpdated(s);
  FlushRegisters(FlushMode::All);
  EmitPendingPageTableUpdate();

  // Enabling external interrupts takes a pending one.
  WriteExceptionExit(js.compilerPC + 4, true);
}

void JitPPC64::EmitMSRUpdated(GPR msr)
{
  // As PowerPCManager::MSRUpdated, without calls: switch the arena to MSR[DR]'s, and the block
  // cache's feature flags to MSR[DR] and MSR[IR]. EmitPendingPageTableUpdate does the rest.
  auto& memory = m_system.GetMemory();
  const auto physical = gpr.GetScopedReg();
  MOVP2R(MEM_REG, jo.fastmem_arena ? memory.GetLogicalBase() : memory.GetLogicalPageMappingsBase());
  MOVP2R(physical,
         jo.fastmem_arena ? memory.GetPhysicalBase() : memory.GetPhysicalPageMappingsBase());
  ANDI_(GPR::R0, msr, 1 << UReg_MSR{}.DR.StartBit());
  ISEL(MEM_REG, physical, MEM_REG, CRBit(CRF::CR0, CCFlags::EQ));
  STD(MEM_REG, PPC_REG, PPCSTATE_OFF(mem_ptr));

  static_assert(UReg_MSR{}.DR.StartBit() == 4);
  static_assert(UReg_MSR{}.IR.StartBit() == 5);
  static_assert(FEATURE_FLAG_MSR_DR == 1 << 0);
  static_assert(FEATURE_FLAG_MSR_IR == 1 << 1);
  const u32 other_feature_flags = m_ppc_state.feature_flags & ~0x3;
  RLWINM(physical, msr, 32 - 4, 30, 31);
  if (other_feature_flags != 0)
    ORI(physical, physical, static_cast<u16>(other_feature_flags));
  STW(physical, PPC_REG, PPCSTATE_OFF(feature_flags));
}

void JitPPC64::EmitPendingPageTableUpdate()
{
  // Turning on data translation applies page table updates made while it was off. This calls
  // C++, so the registers must have been flushed.
  LWZ(GPR::R3, PPC_REG, PPCSTATE_OFF(msr));
  ANDI_(GPR::R3, GPR::R3, 1 << UReg_MSR{}.DR.StartBit());
  const FixupBranch dr_off = BC(CCFlags::EQ, CRF::CR0);
  LBZ(GPR::R3, PPC_REG, PPCSTATE_OFF(pagetable_update_pending));
  CMPWI(CRF::CR1, GPR::R3, 0);
  const FixupBranch not_pending = BC(CCFlags::EQ, CRF::CR1);
  ABI_CallFunction(&PowerPC::MMU::PageTableUpdatedFromJit, &m_system.GetMMU());
  LD(MEM_REG, PPC_REG, PPCSTATE_OFF(mem_ptr));
  SetJumpTarget(not_pending);
  SetJumpTarget(dr_off);
}
