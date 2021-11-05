// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cinttypes>
#include <cstddef>
#include <string>

#include "Common/BitSet.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

#include "Core/HW/Memmap.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/Jit_Util.h"
#include "Core/PowerPC/JitArmCommon/BackPatch.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

using namespace Arm64Gen;

void JitArm64::DoBacktrace(uintptr_t access_address, SContext* ctx)
{
  for (int i = 0; i < 30; i += 2)
  {
    ERROR_LOG_FMT(DYNA_REC, "R{}: {:#018x}\tR{}: {:#018x}", i, ctx->CTX_REG(i), i + 1,
                  ctx->CTX_REG(i + 1));
  }

  ERROR_LOG_FMT(DYNA_REC, "R30: {:#018x}\tSP: {:#018x}", ctx->CTX_LR, ctx->CTX_SP);

  ERROR_LOG_FMT(DYNA_REC, "Access Address: {:#018x}", access_address);
  ERROR_LOG_FMT(DYNA_REC, "PC: {:#018x}", ctx->CTX_PC);

  ERROR_LOG_FMT(DYNA_REC, "Memory Around PC");

  std::string pc_memory;
  for (u64 pc = (ctx->CTX_PC - 32); pc < (ctx->CTX_PC + 32); pc += 16)
  {
    pc_memory += fmt::format("{:08x}{:08x}{:08x}{:08x}", Common::swap32(*(u32*)pc),
                             Common::swap32(*(u32*)(pc + 4)), Common::swap32(*(u32*)(pc + 8)),
                             Common::swap32(*(u32*)(pc + 12)));

    ERROR_LOG_FMT(DYNA_REC, "{:#018x}: {:08x} {:08x} {:08x} {:08x}", pc, *(u32*)pc, *(u32*)(pc + 4),
                  *(u32*)(pc + 8), *(u32*)(pc + 12));
  }

  ERROR_LOG_FMT(DYNA_REC, "Full block: {}", pc_memory);
}

void JitArm64::EmitBackpatchRoutine(u32 flags, bool fastmem, bool do_farcode, ARM64Reg RS,
                                    ARM64Reg addr, BitSet32 gprs_to_push, BitSet32 fprs_to_push)
{
  bool in_far_code = false;
  const u8* fastmem_start = GetCodePtr();

  if (fastmem)
  {
    if (flags & BackPatchInfo::FLAG_STORE && flags & BackPatchInfo::FLAG_MASK_FLOAT)
    {
      if (flags & BackPatchInfo::FLAG_SIZE_F32)
      {
        m_float_emit.REV32(8, ARM64Reg::D0, RS);
        m_float_emit.STR(32, ARM64Reg::D0, MEM_REG, addr);
      }
      else if (flags & BackPatchInfo::FLAG_SIZE_F32X2)
      {
        m_float_emit.REV32(8, ARM64Reg::D0, RS);
        m_float_emit.STR(64, ARM64Reg::Q0, MEM_REG, addr);
      }
      else
      {
        m_float_emit.REV64(8, ARM64Reg::Q0, RS);
        m_float_emit.STR(64, ARM64Reg::Q0, MEM_REG, addr);
      }
    }
    else if (flags & BackPatchInfo::FLAG_LOAD && flags & BackPatchInfo::FLAG_MASK_FLOAT)
    {
      if (flags & BackPatchInfo::FLAG_SIZE_F32)
      {
        m_float_emit.LDR(32, EncodeRegToDouble(RS), MEM_REG, addr);
        m_float_emit.REV32(8, EncodeRegToDouble(RS), EncodeRegToDouble(RS));
      }
      else
      {
        m_float_emit.LDR(64, EncodeRegToDouble(RS), MEM_REG, addr);
        m_float_emit.REV64(8, EncodeRegToDouble(RS), EncodeRegToDouble(RS));
      }
    }
    else if (flags & BackPatchInfo::FLAG_STORE)
    {
      ARM64Reg temp = ARM64Reg::W0;
      temp = ByteswapBeforeStore(this, temp, RS, flags, true);

      if (flags & BackPatchInfo::FLAG_SIZE_32)
        STR(temp, MEM_REG, addr);
      else if (flags & BackPatchInfo::FLAG_SIZE_16)
        STRH(temp, MEM_REG, addr);
      else
        STRB(temp, MEM_REG, addr);
    }
    else if (flags & BackPatchInfo::FLAG_ZERO_256)
    {
      // This literally only stores 32bytes of zeros to the target address
      ARM64Reg temp = ARM64Reg::X30;
      ADD(temp, addr, MEM_REG);
      STP(IndexType::Signed, ARM64Reg::ZR, ARM64Reg::ZR, temp, 0);
      STP(IndexType::Signed, ARM64Reg::ZR, ARM64Reg::ZR, temp, 16);
    }
    else
    {
      if (flags & BackPatchInfo::FLAG_SIZE_32)
        LDR(RS, MEM_REG, addr);
      else if (flags & BackPatchInfo::FLAG_SIZE_16)
        LDRH(RS, MEM_REG, addr);
      else if (flags & BackPatchInfo::FLAG_SIZE_8)
        LDRB(RS, MEM_REG, addr);

      ByteswapAfterLoad(this, RS, RS, flags, true, false);
    }
  }
  const u8* fastmem_end = GetCodePtr();

  if (!fastmem || do_farcode)
  {
    if (fastmem && do_farcode)
    {
      SlowmemHandler handler;
      handler.dest_reg = RS;
      handler.addr_reg = addr;
      handler.gprs = gprs_to_push;
      handler.fprs = fprs_to_push;
      handler.flags = flags;

      FastmemArea* fastmem_area = &m_fault_to_handler[fastmem_end];
      auto handler_loc_iter = m_handler_to_loc.find(handler);

      if (handler_loc_iter == m_handler_to_loc.end())
      {
        in_far_code = true;
        SwitchToFarCode();
        const u8* handler_loc = GetCodePtr();
        m_handler_to_loc[handler] = handler_loc;
        fastmem_area->fastmem_code = fastmem_start;
        fastmem_area->slowmem_code = handler_loc;
      }
      else
      {
        const u8* handler_loc = handler_loc_iter->second;
        fastmem_area->fastmem_code = fastmem_start;
        fastmem_area->slowmem_code = handler_loc;
        return;
      }
    }

    ABI_PushRegisters(gprs_to_push);
    m_float_emit.ABI_PushRegisters(fprs_to_push, ARM64Reg::X30);

    if (flags & BackPatchInfo::FLAG_STORE && flags & BackPatchInfo::FLAG_MASK_FLOAT)
    {
      if (flags & BackPatchInfo::FLAG_SIZE_F32)
      {
        m_float_emit.UMOV(32, ARM64Reg::W0, RS, 0);
        MOVP2R(ARM64Reg::X8, &PowerPC::Write_U32);
        BLR(ARM64Reg::X8);
      }
      else if (flags & BackPatchInfo::FLAG_SIZE_F32X2)
      {
        m_float_emit.UMOV(64, ARM64Reg::X0, RS, 0);
        MOVP2R(ARM64Reg::X8, &PowerPC::Write_U64);
        ROR(ARM64Reg::X0, ARM64Reg::X0, 32);
        BLR(ARM64Reg::X8);
      }
      else
      {
        m_float_emit.UMOV(64, ARM64Reg::X0, RS, 0);
        MOVP2R(ARM64Reg::X8, &PowerPC::Write_U64);
        BLR(ARM64Reg::X8);
      }
    }
    else if (flags & BackPatchInfo::FLAG_LOAD && flags & BackPatchInfo::FLAG_MASK_FLOAT)
    {
      if (flags & BackPatchInfo::FLAG_SIZE_F32)
      {
        MOVP2R(ARM64Reg::X8, &PowerPC::Read_U32);
        BLR(ARM64Reg::X8);
        m_float_emit.INS(32, RS, 0, ARM64Reg::X0);
      }
      else
      {
        MOVP2R(ARM64Reg::X8, &PowerPC::Read_F64);
        BLR(ARM64Reg::X8);
        m_float_emit.INS(64, RS, 0, ARM64Reg::X0);
      }
    }
    else if (flags & BackPatchInfo::FLAG_STORE)
    {
      MOV(ARM64Reg::W0, RS);

      const bool reverse = (flags & BackPatchInfo::FLAG_REVERSE) != 0;

      if (flags & BackPatchInfo::FLAG_SIZE_32)
        MOVP2R(ARM64Reg::X8, reverse ? &PowerPC::Write_U32_Swap : &PowerPC::Write_U32);
      else if (flags & BackPatchInfo::FLAG_SIZE_16)
        MOVP2R(ARM64Reg::X8, reverse ? &PowerPC::Write_U16_Swap : &PowerPC::Write_U16);
      else
        MOVP2R(ARM64Reg::X8, &PowerPC::Write_U8);

      BLR(ARM64Reg::X8);
    }
    else if (flags & BackPatchInfo::FLAG_ZERO_256)
    {
      MOVP2R(ARM64Reg::X8, &PowerPC::ClearCacheLine);
      BLR(ARM64Reg::X8);
    }
    else
    {
      if (flags & BackPatchInfo::FLAG_SIZE_32)
        MOVP2R(ARM64Reg::X8, &PowerPC::Read_U32);
      else if (flags & BackPatchInfo::FLAG_SIZE_16)
        MOVP2R(ARM64Reg::X8, &PowerPC::Read_U16);
      else if (flags & BackPatchInfo::FLAG_SIZE_8)
        MOVP2R(ARM64Reg::X8, &PowerPC::Read_U8);

      BLR(ARM64Reg::X8);

      ByteswapAfterLoad(this, RS, ARM64Reg::W0, flags, false, false);
    }

    m_float_emit.ABI_PopRegisters(fprs_to_push, ARM64Reg::X30);
    ABI_PopRegisters(gprs_to_push);
  }

  if (in_far_code)
  {
    RET(ARM64Reg::X30);
    SwitchToNearCode();
  }
}

bool JitArm64::HandleFastmemFault(uintptr_t access_address, SContext* ctx)
{
  if (!(access_address >= (uintptr_t)Memory::physical_base &&
        access_address < (uintptr_t)Memory::physical_base + 0x100010000) &&
      !(access_address >= (uintptr_t)Memory::logical_base &&
        access_address < (uintptr_t)Memory::logical_base + 0x100010000))
  {
    ERROR_LOG_FMT(DYNA_REC,
                  "Exception handler - access below memory space. PC: {:#018x} {:#018x} < {:#018x}",
                  ctx->CTX_PC, access_address, (uintptr_t)Memory::physical_base);
    return false;
  }

  const u8* pc = reinterpret_cast<const u8*>(ctx->CTX_PC);
  auto slow_handler_iter = m_fault_to_handler.upper_bound(pc);

  // no fastmem area found
  if (slow_handler_iter == m_fault_to_handler.end())
    return false;

  const u8* fastmem_area_start = slow_handler_iter->second.fastmem_code;
  const u8* fastmem_area_end = slow_handler_iter->first;

  // no overlapping fastmem area found
  if (pc < fastmem_area_start)
    return false;

  const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;
  ARM64XEmitter emitter(const_cast<u8*>(fastmem_area_start));

  emitter.BL(slow_handler_iter->second.slowmem_code);

  while (emitter.GetCodePtr() < fastmem_area_end)
    emitter.NOP();

  m_fault_to_handler.erase(slow_handler_iter);

  emitter.FlushIcache();

  ctx->CTX_PC = reinterpret_cast<std::uintptr_t>(fastmem_area_start);
  return true;
}
