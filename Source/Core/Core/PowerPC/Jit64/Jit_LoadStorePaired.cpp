// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// TODO(ector): Tons of pshufb optimization of the loads/stores, for SSSE3+, possibly SSE4, only.
// Should give a very noticeable speed boost to paired single heavy code.

#include "Core/PowerPC/Jit64/Jit.h"
#include "Common/BitSet.h"
#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Core/ConfigManager.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"
#include "Core/PowerPC/JitCommon/JitAsmCommon.h"
#include "Core/PowerPC/JitCommon/Jit_Util.h"
#include "Core/PowerPC/PowerPC.h"

using namespace Gen;

// The big problem is likely instructions that set the quantizers in the same block.
// We will have to break block after quantizers are written to.
void Jit64::psq_stXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStorePairedOff);

  s32 offset = inst.SIMM_12;
  bool indexed = inst.OPCD == 4;
  bool update = (inst.OPCD == 61 && offset) || (inst.OPCD == 4 && !!(inst.SUBOP6 & 32));
  int a = inst.RA;
  int b = indexed ? inst.RB : a;
  int s = inst.FS;
  int i = indexed ? inst.Ix : inst.I;
  int w = indexed ? inst.Wx : inst.W;

  auto it = js.constantGqr.find(i);
  bool gqrIsConstant = it != js.constantGqr.end();
  u32 gqrValue = gqrIsConstant ? it->second & 0xffff : 0;

  auto ra = a ? regs.gpr.Lock(a) : regs.gpr.Zero();
  auto rb = indexed ? regs.gpr.Lock(b) : regs.gpr.Imm32((u32)offset);
  auto rs = regs.fpu.Lock(s);

  // We can take a fast path here if there's no quantization (slightly less
  // overhead as it doesn't require us to bump RSCRATCH_EXTRA and we can
  // take advantage of constant memory writes in some cases)
  if (gqrIsConstant && gqrValue == 0)
  {
    auto scratch = regs.gpr.Borrow();
    auto xmm = regs.fpu.Borrow();

    if (w)
    {
      CVTSD2SS(xmm, rs);  // one
      MOVD_xmm(R(scratch), xmm);
    }
    else
    {
      CVTPD2PS(xmm, rs);  // pair
      MOVQ_xmm(R(scratch), xmm);
      ROL(64, scratch, Imm8(32));
    }

    SafeWrite(scratch, ra, rb, w ? 32 : 64, true, update);
    return;
  }

  // ABI for quantized load/store routines
  auto xmm0 = regs.fpu.Borrow(XMM0);

  if (w)
    CVTSD2SS(xmm0, rs);  // one
  else
    CVTPD2PS(xmm0, rs);  // pair

  auto scratch_extra = regs.gpr.Borrow(RSCRATCH_EXTRA);
  MOV_sum(32, scratch_extra, ra, rb);

  if (update)
  {
    auto xa = ra.Bind(BindMode::WriteTransactional);
    MOV(32, xa, scratch_extra);
  }

  if (gqrIsConstant)
  {
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
      auto scratch2 = regs.gpr.Borrow(RSCRATCH2);

      // We know what GQR is here, so we can load RSCRATCH2 and call into the store method directly
      // with just the scale bits.
      MOV(32, scratch2, Imm32(gqrValue & 0x3F00));

      if (w)
        CALL(asm_routines.singleStoreQuantized[type]);
      else
        CALL(asm_routines.pairedStoreQuantized[type]);
    }
  }
  else
  {
    auto scratch = regs.gpr.Borrow();
    auto scratch2 = regs.gpr.Borrow(RSCRATCH2);

    // Some games (e.g. Dirt 2) incorrectly set the unused bits which breaks the lookup table code.
    // Hence, we need to mask out the unused bits. The layout of the GQR register is
    // UU[SCALE]UUUUU[TYPE] where SCALE is 6 bits and TYPE is 3 bits, so we have to AND with
    // 0b0011111100000111, or 0x3F07.
    MOV(32, scratch2, Imm32(0x3F07));
    AND(32, scratch2, PPCSTATE(spr[SPR_GQR0 + i]));
    MOVZX(32, 8, scratch, scratch2);

    if (w)
      CALLptr(MScaled(scratch, SCALE_8, (u32)(u64)asm_routines.singleStoreQuantized));
    else
      CALLptr(MScaled(scratch, SCALE_8, (u32)(u64)asm_routines.pairedStoreQuantized));
  }
}

void Jit64::psq_lXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStorePairedOff);

  s32 offset = inst.SIMM_12;
  bool indexed = inst.OPCD == 4;
  bool update = (inst.OPCD == 57 && offset) || (inst.OPCD == 4 && !!(inst.SUBOP6 & 32));
  int a = inst.RA;
  int b = indexed ? inst.RB : a;
  int s = inst.FS;
  int i = indexed ? inst.Ix : inst.I;
  int w = indexed ? inst.Wx : inst.W;

  auto it = js.constantGqr.find(i);
  bool gqrIsConstant = it != js.constantGqr.end();
  u32 gqrValue = gqrIsConstant ? it->second >> 16 : 0;

  auto ra = a ? regs.gpr.Lock(a) : regs.gpr.Zero();
  auto rb = indexed ? regs.gpr.Lock(b) : Imm32((u32)offset);

  // ABI for quantized load/store routines
  auto xmm0 = regs.fpu.Borrow(XMM0);
  auto scratch_extra = regs.gpr.Borrow(RSCRATCH_EXTRA);

  MOV_sum(32, scratch_extra, ra, rb);

  if (update)
  {
    auto xa = ra.Bind(BindMode::WriteTransactional);
    MOV(32, xa, scratch_extra);
  }

  if (gqrIsConstant)
  {
    // TODO: pass it a scratch register it can use
    GenQuantizedLoad(w == 1, static_cast<EQuantizeType>(gqrValue & 0x7), (gqrValue & 0x3F00) >> 8);
  }
  else
  {
    auto scratch = regs.gpr.Borrow();
    auto scratch2 = regs.gpr.Borrow(RSCRATCH2);

    MOV(32, scratch2, Imm32(0x3F07));

    // Get the high part of the GQR register
    OpArg gqr = PPCSTATE(spr[SPR_GQR0 + i]);
    gqr.AddMemOffset(2);

    AND(32, scratch2, gqr);
    MOVZX(32, 8, scratch, scratch2);

    // Explicitly uses RSCRATCH2 and RSCRATCH_EXTRA
    CALLptr(MScaled(scratch, SCALE_8, (u32)(u64)(&asm_routines.pairedLoadQuantized[w * 8])));
  }

  auto rs = regs.fpu.Lock(s).Bind(BindMode::Write);
  CVTPS2PD(rs, xmm0);
}
