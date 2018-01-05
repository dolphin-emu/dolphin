// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/Arm64Emitter.h"
#include "Common/BitSet.h"
#include "Common/CommonTypes.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/GPFifo.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/PowerPC.h"

using namespace Arm64Gen;

void JitArm64::lfXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreFloatingOff);
  FALLBACK_IF(jo.memcheck);

  u32 a = inst.RA, b = inst.RB;

  s32 offset = inst.SIMM_16;
  u32 flags = BackPatchInfo::FLAG_LOAD;
  bool update = false;
  s32 offset_reg = -1;

  switch (inst.OPCD)
  {
  case 31:
    switch (inst.SUBOP10)
    {
    case 567:  // lfsux
      flags |= BackPatchInfo::FLAG_SIZE_F32;
      update = true;
      offset_reg = b;
      break;
    case 535:  // lfsx
      flags |= BackPatchInfo::FLAG_SIZE_F32;
      offset_reg = b;
      break;
    case 631:  // lfdux
      flags |= BackPatchInfo::FLAG_SIZE_F64;
      update = true;
      offset_reg = b;
      break;
    case 599:  // lfdx
      flags |= BackPatchInfo::FLAG_SIZE_F64;
      offset_reg = b;
      break;
    }
    break;
  case 49:  // lfsu
    flags |= BackPatchInfo::FLAG_SIZE_F32;
    update = true;
    break;
  case 48:  // lfs
    flags |= BackPatchInfo::FLAG_SIZE_F32;
    break;
  case 51:  // lfdu
    flags |= BackPatchInfo::FLAG_SIZE_F64;
    update = true;
    break;
  case 50:  // lfd
    flags |= BackPatchInfo::FLAG_SIZE_F64;
    break;
  }

  u32 imm_addr = 0;
  bool is_immediate = false;

  RegType type = !!(flags & BackPatchInfo::FLAG_SIZE_F64) ? REG_LOWER_PAIR : REG_DUP_SINGLE;

  gpr.Lock(W0, W30);
  fpr.Lock(Q0);

  ARM64Reg VD = fpr.RW(inst.FD, type);
  ARM64Reg addr_reg = W0;

  if (update)
  {
    // Always uses RA
    if (gpr.IsImm(a) && offset_reg == -1)
    {
      is_immediate = true;
      imm_addr = offset + gpr.GetImm(a);
    }
    else if (gpr.IsImm(a) && offset_reg != -1 && gpr.IsImm(offset_reg))
    {
      is_immediate = true;
      imm_addr = gpr.GetImm(a) + gpr.GetImm(offset_reg);
    }
    else
    {
      if (offset_reg == -1)
      {
        ADDI2R(addr_reg, gpr.R(a), offset, addr_reg);
      }
      else
      {
        ADD(addr_reg, gpr.R(offset_reg), gpr.R(a));
      }
    }
  }
  else
  {
    if (offset_reg == -1)
    {
      if (a && gpr.IsImm(a))
      {
        is_immediate = true;
        imm_addr = gpr.GetImm(a) + offset;
      }
      else if (a)
      {
        ADDI2R(addr_reg, gpr.R(a), offset, addr_reg);
      }
      else
      {
        is_immediate = true;
        imm_addr = offset;
      }
    }
    else
    {
      if (a && gpr.IsImm(a) && gpr.IsImm(offset_reg))
      {
        is_immediate = true;
        imm_addr = gpr.GetImm(a) + gpr.GetImm(offset_reg);
      }
      else if (!a && gpr.IsImm(offset_reg))
      {
        is_immediate = true;
        imm_addr = gpr.GetImm(offset_reg);
      }
      else if (a)
      {
        ADD(addr_reg, gpr.R(a), gpr.R(offset_reg));
      }
      else
      {
        MOV(addr_reg, gpr.R(offset_reg));
      }
    }
  }

  ARM64Reg XA = EncodeRegTo64(addr_reg);

  if (is_immediate)
    MOVI2R(XA, imm_addr);

  if (update)
  {
    gpr.BindToRegister(a, false);
    MOV(gpr.R(a), addr_reg);
  }

  BitSet32 regs_in_use = gpr.GetCallerSavedUsed();
  BitSet32 fprs_in_use = fpr.GetCallerSavedUsed();
  regs_in_use[W0] = 0;
  fprs_in_use[0] = 0;  // Q0
  fprs_in_use[VD - Q0] = 0;

  if (is_immediate && PowerPC::IsOptimizableRAMAddress(imm_addr))
  {
    EmitBackpatchRoutine(flags, true, false, VD, XA, BitSet32(0), BitSet32(0));
  }
  else
  {
    EmitBackpatchRoutine(flags, jo.fastmem, jo.fastmem, VD, XA, regs_in_use, fprs_in_use);
  }

  gpr.Unlock(W0, W30);
  fpr.Unlock(Q0);
}

void JitArm64::stfXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreFloatingOff);
  FALLBACK_IF(jo.memcheck);

  u32 a = inst.RA, b = inst.RB;

  s32 offset = inst.SIMM_16;
  u32 flags = BackPatchInfo::FLAG_STORE;
  bool update = false;
  s32 offset_reg = -1;

  switch (inst.OPCD)
  {
  case 31:
    switch (inst.SUBOP10)
    {
    case 663:  // stfsx
      flags |= BackPatchInfo::FLAG_SIZE_F32;
      offset_reg = b;
      break;
    case 695:  // stfsux
      flags |= BackPatchInfo::FLAG_SIZE_F32;
      update = true;
      offset_reg = b;
      break;
    case 727:  // stfdx
      flags |= BackPatchInfo::FLAG_SIZE_F64;
      offset_reg = b;
      break;
    case 759:  // stfdux
      flags |= BackPatchInfo::FLAG_SIZE_F64;
      update = true;
      offset_reg = b;
      break;
    case 983:  // stfiwx
      flags |= BackPatchInfo::FLAG_SIZE_F32I;
      offset_reg = b;
      break;
    }
    break;
  case 53:  // stfsu
    flags |= BackPatchInfo::FLAG_SIZE_F32;
    update = true;
    break;
  case 52:  // stfs
    flags |= BackPatchInfo::FLAG_SIZE_F32;
    break;
  case 55:  // stfdu
    flags |= BackPatchInfo::FLAG_SIZE_F64;
    update = true;
    break;
  case 54:  // stfd
    flags |= BackPatchInfo::FLAG_SIZE_F64;
    break;
  }

  u32 imm_addr = 0;
  bool is_immediate = false;

  gpr.Lock(W0, W1, W30);
  fpr.Lock(Q0);

  bool single = (flags & BackPatchInfo::FLAG_SIZE_F32) && fpr.IsSingle(inst.FS, true);

  ARM64Reg V0 = fpr.R(inst.FS, single ? REG_LOWER_PAIR_SINGLE : REG_LOWER_PAIR);

  if (single)
  {
    flags &= ~BackPatchInfo::FLAG_SIZE_F32;
    flags |= BackPatchInfo::FLAG_SIZE_F32I;
  }

  ARM64Reg addr_reg = W1;

  if (update)
  {
    // Always uses RA
    if (gpr.IsImm(a) && offset_reg == -1)
    {
      is_immediate = true;
      imm_addr = offset + gpr.GetImm(a);
    }
    else if (gpr.IsImm(a) && offset_reg != -1 && gpr.IsImm(offset_reg))
    {
      is_immediate = true;
      imm_addr = gpr.GetImm(a) + gpr.GetImm(offset_reg);
    }
    else
    {
      if (offset_reg == -1)
      {
        ADDI2R(addr_reg, gpr.R(a), offset, addr_reg);
      }
      else
      {
        ADD(addr_reg, gpr.R(offset_reg), gpr.R(a));
      }
    }
  }
  else
  {
    if (offset_reg == -1)
    {
      if (a && gpr.IsImm(a))
      {
        is_immediate = true;
        imm_addr = gpr.GetImm(a) + offset;
      }
      else if (a)
      {
        ADDI2R(addr_reg, gpr.R(a), offset, addr_reg);
      }
      else
      {
        is_immediate = true;
        imm_addr = offset;
      }
    }
    else
    {
      if (a && gpr.IsImm(a) && gpr.IsImm(offset_reg))
      {
        is_immediate = true;
        imm_addr = gpr.GetImm(a) + gpr.GetImm(offset_reg);
      }
      else if (!a && gpr.IsImm(offset_reg))
      {
        is_immediate = true;
        imm_addr = gpr.GetImm(offset_reg);
      }
      else if (a)
      {
        ADD(addr_reg, gpr.R(a), gpr.R(offset_reg));
      }
      else
      {
        MOV(addr_reg, gpr.R(offset_reg));
      }
    }
  }

  ARM64Reg XA = EncodeRegTo64(addr_reg);

  if (is_immediate && !(jo.optimizeGatherPipe && PowerPC::IsOptimizableGatherPipeWrite(imm_addr)))
  {
    MOVI2R(XA, imm_addr);

    if (update)
    {
      gpr.BindToRegister(a, false);
      MOV(gpr.R(a), addr_reg);
    }
  }
  else if (!is_immediate && update)
  {
    gpr.BindToRegister(a, false);
    MOV(gpr.R(a), addr_reg);
  }

  BitSet32 regs_in_use = gpr.GetCallerSavedUsed();
  BitSet32 fprs_in_use = fpr.GetCallerSavedUsed();
  regs_in_use[W0] = 0;
  regs_in_use[W1] = 0;
  fprs_in_use[0] = 0;  // Q0

  if (is_immediate)
  {
    if (jo.optimizeGatherPipe && PowerPC::IsOptimizableGatherPipeWrite(imm_addr))
    {
      int accessSize;
      if (flags & BackPatchInfo::FLAG_SIZE_F64)
        accessSize = 64;
      else
        accessSize = 32;

      MOVP2R(X1, &GPFifo::g_gather_pipe_ptr);
      LDR(INDEX_UNSIGNED, X0, X1, 0);
      if (flags & BackPatchInfo::FLAG_SIZE_F64)
      {
        m_float_emit.REV64(8, Q0, V0);
      }
      else if (flags & BackPatchInfo::FLAG_SIZE_F32)
      {
        m_float_emit.FCVT(32, 64, D0, EncodeRegToDouble(V0));
        m_float_emit.REV32(8, D0, D0);
      }
      else if (flags & BackPatchInfo::FLAG_SIZE_F32I)
      {
        m_float_emit.REV32(8, D0, V0);
      }

      m_float_emit.STR(accessSize, INDEX_POST, accessSize == 64 ? Q0 : D0, X0, accessSize >> 3);

      STR(INDEX_UNSIGNED, X0, X1, 0);
      js.fifoBytesSinceCheck += accessSize >> 3;

      if (update)
      {
        // Chance of this happening is fairly low, but support it
        gpr.BindToRegister(a, false);
        MOVI2R(gpr.R(a), imm_addr);
      }
    }
    else if (PowerPC::IsOptimizableRAMAddress(imm_addr))
    {
      EmitBackpatchRoutine(flags, true, false, V0, XA, BitSet32(0), BitSet32(0));
    }
    else
    {
      EmitBackpatchRoutine(flags, false, false, V0, XA, regs_in_use, fprs_in_use);
    }
  }
  else
  {
    EmitBackpatchRoutine(flags, jo.fastmem, jo.fastmem, V0, XA, regs_in_use, fprs_in_use);
  }
  gpr.Unlock(W0, W1, W30);
  fpr.Unlock(Q0);
}
