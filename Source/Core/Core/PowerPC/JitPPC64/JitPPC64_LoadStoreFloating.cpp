// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Floating-point loads and stores. The memory access is done in a GPR, by the same code as integer
// loads and stores, so it gets fastmem and its slow path.

#include "Core/PowerPC/JitPPC64/Jit.h"

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/PPC64Emitter.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PowerPC.h"

using namespace PPC64Gen;

void JitPPC64::ConvertDoubleToSingle(GPR value)
{
  // As ConvertToSingle. POWER's own conversion (stfs, xscvdpspn) matches it except for values too
  // small to be normal singles, which POWER flushes to zero, so do it in GPRs.
  const auto exp = gpr.GetScopedReg();
  const auto tmp = gpr.GetScopedReg();

  RLDICL(exp, value, 12, 53);
  CMPLWI(CRF::CR1, exp, 896);
  const FixupBranch small = BCFar(CCFlags::LE, CRF::CR1);

  // Normal singles, infinities, NaNs and zeroes:
  // ((x >> 32) & 0xc0000000) | ((x >> 29) & 0x3fffffff)
  const u8* bits = GetCodePtr();
  SRDI(tmp, value, 32);
  SRDI(value, value, 29);
  RLWIMI(value, tmp, 0, 0, 1);
  const u8* done = GetCodePtr();

  SwitchToFarCode();
  SetJumpTarget(small);
  // Zeroes, and exponents below 874 (which are said to be undefined; this is from hardware tests),
  // take the bits as above.
  CLRLDI(tmp, value, 1);
  CMPDI(CRF::CR1, tmp, 0);
  const FixupBranch zero = BC(CCFlags::EQ, CRF::CR1);
  CMPLWI(CRF::CR1, exp, 874);
  const FixupBranch tiny = BC(CCFlags::LT, CRF::CR1);
  // Denormal singles:
  // ((0x80000000 | ((x & DOUBLE_FRAC) >> 21)) >> (905 - exp)) | ((x >> 32) & 0x80000000)
  CLRLDI(tmp, value, 12);
  SRDI(tmp, tmp, 21);
  ORIS(tmp, tmp, 0x8000);
  SUBFIC(exp, exp, 905);
  SRW(tmp, tmp, exp);
  SRDI(exp, value, 32);
  RLWIMI(tmp, exp, 0, 0, 0);
  MR(value, tmp);
  B(done);
  SetJumpTarget(zero);
  SetJumpTarget(tiny);
  B(bits);
  SwitchToNearCode();
}

void JitPPC64::lfXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreFloatingOff);

  bool single = false;
  bool indexed = false;
  bool update = false;
  switch (inst.OPCD)
  {
  case 31:
    indexed = true;
    switch (inst.SUBOP10)
    {
    case 567:  // lfsux
      update = true;
      [[fallthrough]];
    case 535:  // lfsx
      single = true;
      break;
    case 631:  // lfdux
      update = true;
      [[fallthrough]];
    case 599:  // lfdx
      break;
    default:
      ASSERT_MSG(DYNA_REC, false, "Unexpected load {:#010x}", inst.hex);
      break;
    }
    break;
  case 49:  // lfsu
    update = true;
    [[fallthrough]];
  case 48:  // lfs
    single = true;
    break;
  case 51:  // lfdu
    update = true;
    [[fallthrough]];
  case 50:  // lfd
    break;
  default:
    ASSERT_MSG(DYNA_REC, false, "Unexpected load {:#010x}", inst.hex);
    break;
  }

  // The invalid form is left to the interpreter.
  FALLBACK_IF(update && inst.RA == 0);

  const auto addr = gpr.GetScopedReg();
  const auto value = gpr.GetScopedReg();
  ComputeAddress(addr, inst, indexed);
  const GPR a = update ? gpr.RW(inst.RA, true) : GPR::R0;

  // Into VS0, which isn't allocated: FD is bound after the access, which memory checks require.
  EmitLoadToVSR(single ? 32 : 64, VSR::VS0, value, addr);

  // lfd only writes ps0.
  const VSR d = fpr.RW(inst.FD, !single);
  if (single)
  {
    // As ConvertToDouble, into both halves.
    XSCVSPDPN(d, VSR::VS0);
    XXPERMDI(d, d, d, 0);
  }
  else
  {
    XXPERMDI(d, VSR::VS0, d, 1);
  }

  if (update)
    MR(a, addr);
}

void JitPPC64::stfXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreFloatingOff);

  bool single = false;
  bool integer = false;
  bool indexed = false;
  bool update = false;
  switch (inst.OPCD)
  {
  case 31:
    indexed = true;
    switch (inst.SUBOP10)
    {
    case 695:  // stfsux
      update = true;
      [[fallthrough]];
    case 663:  // stfsx
      single = true;
      break;
    case 759:  // stfdux
      update = true;
      [[fallthrough]];
    case 727:  // stfdx
      break;
    case 983:  // stfiwx
      integer = true;
      break;
    default:
      ASSERT_MSG(DYNA_REC, false, "Unexpected store {:#010x}", inst.hex);
      break;
    }
    break;
  case 53:  // stfsu
    update = true;
    [[fallthrough]];
  case 52:  // stfs
    single = true;
    break;
  case 55:  // stfdu
    update = true;
    [[fallthrough]];
  case 54:  // stfd
    break;
  default:
    ASSERT_MSG(DYNA_REC, false, "Unexpected store {:#010x}", inst.hex);
    break;
  }

  // The invalid form is left to the interpreter.
  FALLBACK_IF(update && inst.RA == 0);

  const auto addr = gpr.GetScopedReg();
  const auto value = gpr.GetScopedReg();
  ComputeAddress(addr, inst, indexed);
  const VSR s = fpr.R(inst.FS);
  const GPR a = update ? gpr.RW(inst.RA, true) : GPR::R0;

  if (single)
  {
    MFVSRD(value, s);
    ConvertDoubleToSingle(value);
    EmitMemoryAccess({.store = true, .size = 32}, value, addr);
  }
  else
  {
    // stfd, or stfiwx, which stores the low word.
    EmitStoreFromVSR(integer ? 32 : 64, s, value, addr);
  }

  if (update)
    MR(a, addr);
}
