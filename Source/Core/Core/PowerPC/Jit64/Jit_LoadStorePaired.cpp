// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// TODO(ector): Tons of pshufb optimization of the loads/stores, for SSSE3+, possibly SSE4, only.
// Should give a very noticeable speed boost to paired single heavy code.

#include "Core/PowerPC/Jit64/Jit.h"

#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/Jit64/RegCache/JitRegCache.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"
#include "Core/PowerPC/JitCommon/JitAsmCommon.h"
#include "Core/PowerPC/PowerPC.h"

using namespace Gen;

// The big problem is likely instructions that set the quantizers in the same block.
// We will have to break block after quantizers are written to.
void Jit64::psq_stXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStorePairedOff);

  // For performance, the AsmCommon routines assume address translation is on.
  FALLBACK_IF(!(m_ppc_state.feature_flags & FEATURE_FLAG_MSR_DR));

  s32 offset = inst.SIMM_12;
  bool indexed = inst.OPCD == 4;
  bool update = (inst.OPCD == 61 && offset) || (inst.OPCD == 4 && !!(inst.SUBOP6 & 32));
  int a = inst.RA;
  int b = indexed ? inst.RB : a;
  int s = inst.FS;
  int i = indexed ? inst.Ix : inst.I;
  int w = indexed ? inst.Wx : inst.W;
  FALLBACK_IF(!a);

  RCX64Reg scratch_guard = gpr.Scratch(RSCRATCH_EXTRA);
  RCOpArg Ra = update ? gpr.Bind(a, RCMode::ReadWrite) : gpr.Use(a, RCMode::Read);
  RCOpArg Rb = indexed ? gpr.Use(b, RCMode::Read) : RCOpArg::Imm32((u32)offset);
  RCOpArg Rs = fpr.Use(s, RCMode::Read);
  RegCache::Realize(scratch_guard, Ra, Rb, Rs);

  MOV_sum(32, RSCRATCH_EXTRA, Ra, Rb);

  // In memcheck mode, don't update the address until the exception check
  if (update && !jo.memcheck)
    MOV(32, Ra, R(RSCRATCH_EXTRA));

  if (w)
    CVTSD2SS(XMM0, Rs);  // one
  else
    CVTPD2PS(XMM0, Rs);  // pair

  const bool gqrIsConstant = js.constantGqrValid[i];
  if (gqrIsConstant)
  {
    const u32 gqrValue = js.constantGqr[i] & 0xffff;
    int type = gqrValue & 0x7;

    // Paired stores (other than w/type zero) don't yield any real change in
    // performance right now, but if we can improve fastmem support this might change
    if (gqrValue == 0)
    {
      if (w)
        GenQuantizedStore(true, static_cast<EQuantizeType>(type), (gqrValue & 0x3F00) >> 8);
      else
        GenQuantizedStore(false, static_cast<EQuantizeType>(type), (gqrValue & 0x3F00) >> 8);
    }
    else
    {
      // Stash PC in case asm routine needs to call into C++
      MOV(32, PPCSTATE(pc), Imm32(js.compilerPC));
      // We know what GQR is here, so we can load RSCRATCH2 and call into the store method directly
      // with just the scale bits.
      MOV(32, R(RSCRATCH2), Imm32(gqrValue & 0x3F00));

      if (w)
        CALL(asm_routines.single_store_quantized[type]);
      else
        CALL(asm_routines.paired_store_quantized[type]);
    }
  }
  else
  {
    // Stash PC in case asm routine needs to call into C++
    MOV(32, PPCSTATE(pc), Imm32(js.compilerPC));
    // Some games (e.g. Dirt 2) incorrectly set the unused bits which breaks the lookup table code.
    // Hence, we need to mask out the unused bits. The layout of the GQR register is
    // UU[SCALE]UUUUU[TYPE] where SCALE is 6 bits and TYPE is 3 bits, so we have to AND with
    // 0b0011111100000111, or 0x3F07.
    MOV(32, R(RSCRATCH2), Imm32(0x3F07));
    AND(32, R(RSCRATCH2), PPCSTATE_SPR(SPR_GQR0 + i));
    LEA(64, RSCRATCH,
        M(w ? asm_routines.single_store_quantized : asm_routines.paired_store_quantized));
    // 8-bit operations do not zero upper 32-bits of 64-bit registers.
    // Here we know that RSCRATCH's least significant byte is zero.
    OR(8, R(RSCRATCH), R(RSCRATCH2));
    SHL(8, R(RSCRATCH), Imm8(3));
    CALLptr(MatR(RSCRATCH));
  }

  if (update && jo.memcheck)
  {
    ADD(32, Ra, Rb);
  }
}

void Jit64::psq_lXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStorePairedOff);

  // For performance, the AsmCommon routines assume address translation is on.
  FALLBACK_IF(!(m_ppc_state.feature_flags & FEATURE_FLAG_MSR_DR));

  s32 offset = inst.SIMM_12;
  bool indexed = inst.OPCD == 4;
  bool update = (inst.OPCD == 57 && offset) || (inst.OPCD == 4 && !!(inst.SUBOP6 & 32));
  int a = inst.RA;
  int b = indexed ? inst.RB : a;
  int s = inst.FS;
  int i = indexed ? inst.Ix : inst.I;
  int w = indexed ? inst.Wx : inst.W;
  FALLBACK_IF(!a);

  RCX64Reg scratch_guard = gpr.Scratch(RSCRATCH_EXTRA);
  RCX64Reg Ra = gpr.Bind(a, update ? RCMode::ReadWrite : RCMode::Read);
  RCOpArg Rb = indexed ? gpr.Use(b, RCMode::Read) : RCOpArg::Imm32((u32)offset);
  RCX64Reg Rs = fpr.Bind(s, RCMode::Write);
  RegCache::Realize(scratch_guard, Ra, Rb, Rs);

  MOV_sum(32, RSCRATCH_EXTRA, Ra, Rb);

  // In memcheck mode, don't update the address until the exception check
  if (update && !jo.memcheck)
    MOV(32, Ra, R(RSCRATCH_EXTRA));

  const bool gqrIsConstant = js.constantGqrValid[i];
  if (gqrIsConstant)
  {
    const u32 gqrValue = js.constantGqr[i] >> 16;
    GenQuantizedLoad(w == 1, static_cast<EQuantizeType>(gqrValue & 0x7), (gqrValue & 0x3F00) >> 8);
  }
  else
  {
    // Stash PC in case asm routine needs to call into C++
    MOV(32, PPCSTATE(pc), Imm32(js.compilerPC));
    // Get the high part of the GQR register
    OpArg gqr = PPCSTATE_SPR(SPR_GQR0 + i);
    gqr.AddMemOffset(2);
    MOV(32, R(RSCRATCH2), Imm32(0x3F07));
    AND(32, R(RSCRATCH2), gqr);
    LEA(64, RSCRATCH,
        M(w ? asm_routines.single_load_quantized : asm_routines.paired_load_quantized));
    // 8-bit operations do not zero upper 32-bits of 64-bit registers.
    // Here we know that RSCRATCH's least significant byte is zero.
    OR(8, R(RSCRATCH), R(RSCRATCH2));
    SHL(8, R(RSCRATCH), Imm8(3));
    CALLptr(MatR(RSCRATCH));
  }

  CVTPS2PD(Rs, R(XMM0));
  if (update && jo.memcheck)
  {
    ADD(32, Ra, Rb);
  }
}
