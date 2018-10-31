// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Arm64Emitter.h"
#include "Common/CommonTypes.h"
#include "Common/JitRegister.h"
#include "Common/MathUtil.h"
#include "Core/CoreTiming.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitCommon/JitAsmCommon.h"
#include "Core/PowerPC/JitCommon/JitCache.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

using namespace Arm64Gen;

void JitArm64::GenerateAsm()
{
  // This value is all of the callee saved registers that we are required to save.
  // According to the AACPS64 we need to save R19 ~ R30 and Q8 ~ Q15.
  const u32 ALL_CALLEE_SAVED = 0x7FF80000;
  const u32 ALL_CALLEE_SAVED_FPR = 0x0000FF00;
  BitSet32 regs_to_save(ALL_CALLEE_SAVED);
  BitSet32 regs_to_save_fpr(ALL_CALLEE_SAVED_FPR);
  enter_code = GetCodePtr();

  ABI_PushRegisters(regs_to_save);
  m_float_emit.ABI_PushRegisters(regs_to_save_fpr, X30);

  MOVP2R(PPC_REG, &PowerPC::ppcState);

  // Swap the stack pointer, so we have proper guard pages.
  ADD(X0, SP, 0);
  MOVP2R(X1, &m_saved_stack_pointer);
  STR(INDEX_UNSIGNED, X0, X1, 0);
  MOVP2R(X1, &m_stack_pointer);
  LDR(INDEX_UNSIGNED, X0, X1, 0);
  FixupBranch no_fake_stack = CBZ(X0);
  ADD(SP, X0, 0);
  SetJumpTarget(no_fake_stack);

  // Push {nullptr; -1} as invalid destination on the stack.
  MOVI2R(X0, 0xFFFFFFFF);
  STP(INDEX_PRE, ZR, X0, SP, -16);

  // Store the stack pointer, so we can reset it if the BLR optimization fails.
  ADD(X0, SP, 0);
  STR(INDEX_UNSIGNED, X0, PPC_REG, PPCSTATE_OFF(stored_stack_pointer));

  // The PC will be loaded into DISPATCHER_PC after the call to CoreTiming::Advance().
  // Advance() does an exception check so we don't know what PC to use until afterwards.
  FixupBranch to_start_of_timing_slice = B();

  // If we align the dispatcher to a page then we can load its location with one ADRP instruction
  // do
  // {
  //   CoreTiming::Advance();  // <-- Checks for exceptions (changes PC)
  //   DISPATCHER_PC = PC;
  //   do
  //   {
  // dispatcher_no_check:
  //     ExecuteBlock(JitBase::Dispatch());
  // dispatcher:
  //   } while (PowerPC::ppcState.downcount > 0);
  // do_timing:
  //   NPC = PC = DISPATCHER_PC;
  // } while (CPU::GetState() == CPU::State::Running);
  AlignCodePage();
  dispatcher = GetCodePtr();
  WARN_LOG(DYNA_REC, "Dispatcher is %p", dispatcher);

  // Downcount Check
  // The result of slice decrementation should be in flags if somebody jumped here
  // IMPORTANT - We jump on negative, not carry!!!
  FixupBranch bail = B(CC_MI);

  dispatcher_no_check = GetCodePtr();

  bool assembly_dispatcher = true;

  if (assembly_dispatcher)
  {
    // set the mem_base based on MSR flags
    LDR(INDEX_UNSIGNED, ARM64Reg::W28, PPC_REG, PPCSTATE_OFF(msr));
    FixupBranch physmem = TBNZ(ARM64Reg::W28, 31 - 27);
    MOVP2R(MEM_REG, Memory::physical_base);
    FixupBranch membaseend = B();
    SetJumpTarget(physmem);
    MOVP2R(MEM_REG, Memory::logical_base);
    SetJumpTarget(membaseend);

    // iCache[(address >> 2) & iCache_Mask];
    ARM64Reg pc_masked = W25;
    ARM64Reg cache_base = X27;
    ARM64Reg block = X30;
    ORRI2R(pc_masked, WZR, JitBaseBlockCache::FAST_BLOCK_MAP_MASK << 3);
    AND(pc_masked, pc_masked, DISPATCHER_PC, ArithOption(DISPATCHER_PC, ST_LSL, 1));
    MOVP2R(cache_base, GetBlockCache()->GetFastBlockMap());
    LDR(block, cache_base, EncodeRegTo64(pc_masked));
    FixupBranch not_found = CBZ(block);

    // b.effectiveAddress != addr || b.msrBits != msr
    ARM64Reg pc_and_msr = W25;
    ARM64Reg pc_and_msr2 = W24;
    LDR(INDEX_UNSIGNED, pc_and_msr, block, offsetof(JitBlock, effectiveAddress));
    CMP(pc_and_msr, DISPATCHER_PC);
    FixupBranch pc_missmatch = B(CC_NEQ);

    LDR(INDEX_UNSIGNED, pc_and_msr2, PPC_REG, PPCSTATE_OFF(msr));
    ANDI2R(pc_and_msr2, pc_and_msr2, JitBaseBlockCache::JIT_CACHE_MSR_MASK);
    LDR(INDEX_UNSIGNED, pc_and_msr, block, offsetof(JitBlock, msrBits));
    CMP(pc_and_msr, pc_and_msr2);
    FixupBranch msr_missmatch = B(CC_NEQ);

    // return blocks[block_num].normalEntry;
    LDR(INDEX_UNSIGNED, block, block, offsetof(JitBlock, normalEntry));
    BR(block);
    SetJumpTarget(not_found);
    SetJumpTarget(pc_missmatch);
    SetJumpTarget(msr_missmatch);
  }

  // Call C version of Dispatch().
  STR(INDEX_UNSIGNED, DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(pc));
  MOVP2R(X0, this);
  MOVP2R(X30, reinterpret_cast<void*>(&JitBase::Dispatch));
  BLR(X30);

  FixupBranch no_block_available = CBZ(X0);

  // set the mem_base based on MSR flags and jump to next block.
  LDR(INDEX_UNSIGNED, ARM64Reg::W28, PPC_REG, PPCSTATE_OFF(msr));
  FixupBranch physmem = TBNZ(ARM64Reg::W28, 31 - 27);
  MOVP2R(MEM_REG, Memory::physical_base);
  BR(X0);
  SetJumpTarget(physmem);
  MOVP2R(MEM_REG, Memory::logical_base);
  BR(X0);

  // Call JIT
  SetJumpTarget(no_block_available);
  ResetStack();
  MOVP2R(X0, this);
  MOV(W1, DISPATCHER_PC);
  MOVP2R(X30, reinterpret_cast<void*>(&JitTrampoline));
  BLR(X30);
  LDR(INDEX_UNSIGNED, DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(pc));
  B(dispatcher_no_check);

  SetJumpTarget(bail);
  do_timing = GetCodePtr();
  // Write the current PC out to PPCSTATE
  STR(INDEX_UNSIGNED, DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(pc));
  STR(INDEX_UNSIGNED, DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(npc));

  // Check the state pointer to see if we are exiting
  // Gets checked on at the end of every slice
  MOVP2R(X0, CPU::GetStatePtr());
  LDR(INDEX_UNSIGNED, W0, X0, 0);

  CMP(W0, 0);
  FixupBranch Exit = B(CC_NEQ);

  SetJumpTarget(to_start_of_timing_slice);
  MOVP2R(X30, &CoreTiming::Advance);
  BLR(X30);

  // Load the PC back into DISPATCHER_PC (the exception handler might have changed it)
  LDR(INDEX_UNSIGNED, DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(pc));

  // We can safely assume that downcount >= 1
  B(dispatcher_no_check);

  SetJumpTarget(Exit);

  // Reset the stack pointer, as the BLR optimization have touched it.
  MOVP2R(X1, &m_saved_stack_pointer);
  LDR(INDEX_UNSIGNED, X0, X1, 0);
  ADD(SP, X0, 0);

  m_float_emit.ABI_PopRegisters(regs_to_save_fpr, X30);
  ABI_PopRegisters(regs_to_save);
  RET(X30);

  JitRegister::Register(enter_code, GetCodePtr(), "JIT_Dispatcher");

  GenerateCommonAsm();

  FlushIcache();
}

void JitArm64::GenerateCommonAsm()
{
  // X0 is the scale
  // X1 is address
  // X2 is a temporary on stores
  // X30 is LR
  // Q0 is the return for loads
  //    is the register for stores
  // Q1 is a temporary
  ARM64Reg addr_reg = X1;
  ARM64Reg scale_reg = X0;
  ARM64FloatEmitter float_emit(this);

  const u8* start = GetCodePtr();
  const u8* loadPairedIllegal = GetCodePtr();
  BRK(100);
  const u8* loadPairedFloatTwo = GetCodePtr();
  {
    ADD(addr_reg, addr_reg, MEM_REG);
    float_emit.LD1(32, 1, D0, addr_reg);
    float_emit.REV32(8, D0, D0);
    RET(X30);
  }
  const u8* loadPairedU8Two = GetCodePtr();
  {
    ADD(addr_reg, addr_reg, MEM_REG);
    float_emit.LDR(16, INDEX_UNSIGNED, D0, addr_reg, 0);
    float_emit.UXTL(8, D0, D0);
    float_emit.UXTL(16, D0, D0);
    float_emit.UCVTF(32, D0, D0);

    MOVP2R(addr_reg, &m_dequantizeTableS);
    ADD(scale_reg, addr_reg, scale_reg, ArithOption(scale_reg, ST_LSL, 3));
    float_emit.LDR(32, INDEX_UNSIGNED, D1, scale_reg, 0);
    float_emit.FMUL(32, D0, D0, D1, 0);
    RET(X30);
  }
  const u8* loadPairedS8Two = GetCodePtr();
  {
    ADD(addr_reg, addr_reg, MEM_REG);
    float_emit.LDR(16, INDEX_UNSIGNED, D0, addr_reg, 0);
    float_emit.SXTL(8, D0, D0);
    float_emit.SXTL(16, D0, D0);
    float_emit.SCVTF(32, D0, D0);

    MOVP2R(addr_reg, &m_dequantizeTableS);
    ADD(scale_reg, addr_reg, scale_reg, ArithOption(scale_reg, ST_LSL, 3));
    float_emit.LDR(32, INDEX_UNSIGNED, D1, scale_reg, 0);
    float_emit.FMUL(32, D0, D0, D1, 0);
    RET(X30);
  }
  const u8* loadPairedU16Two = GetCodePtr();
  {
    ADD(addr_reg, addr_reg, MEM_REG);
    float_emit.LD1(16, 1, D0, addr_reg);
    float_emit.REV16(8, D0, D0);
    float_emit.UXTL(16, D0, D0);
    float_emit.UCVTF(32, D0, D0);

    MOVP2R(addr_reg, &m_dequantizeTableS);
    ADD(scale_reg, addr_reg, scale_reg, ArithOption(scale_reg, ST_LSL, 3));
    float_emit.LDR(32, INDEX_UNSIGNED, D1, scale_reg, 0);
    float_emit.FMUL(32, D0, D0, D1, 0);
    RET(X30);
  }
  const u8* loadPairedS16Two = GetCodePtr();
  {
    ADD(addr_reg, addr_reg, MEM_REG);
    float_emit.LD1(16, 1, D0, addr_reg);
    float_emit.REV16(8, D0, D0);
    float_emit.SXTL(16, D0, D0);
    float_emit.SCVTF(32, D0, D0);

    MOVP2R(addr_reg, &m_dequantizeTableS);
    ADD(scale_reg, addr_reg, scale_reg, ArithOption(scale_reg, ST_LSL, 3));
    float_emit.LDR(32, INDEX_UNSIGNED, D1, scale_reg, 0);
    float_emit.FMUL(32, D0, D0, D1, 0);
    RET(X30);
  }

  const u8* loadPairedFloatOne = GetCodePtr();
  {
    ADD(addr_reg, addr_reg, MEM_REG);
    float_emit.LDR(32, INDEX_UNSIGNED, D0, addr_reg, 0);
    float_emit.REV32(8, D0, D0);
    RET(X30);
  }
  const u8* loadPairedU8One = GetCodePtr();
  {
    ADD(addr_reg, addr_reg, MEM_REG);
    float_emit.LDR(8, INDEX_UNSIGNED, D0, addr_reg, 0);
    float_emit.UXTL(8, D0, D0);
    float_emit.UXTL(16, D0, D0);
    float_emit.UCVTF(32, D0, D0);

    MOVP2R(addr_reg, &m_dequantizeTableS);
    ADD(scale_reg, addr_reg, scale_reg, ArithOption(scale_reg, ST_LSL, 3));
    float_emit.LDR(32, INDEX_UNSIGNED, D1, scale_reg, 0);
    float_emit.FMUL(32, D0, D0, D1, 0);
    RET(X30);
  }
  const u8* loadPairedS8One = GetCodePtr();
  {
    ADD(addr_reg, addr_reg, MEM_REG);
    float_emit.LDR(8, INDEX_UNSIGNED, D0, addr_reg, 0);
    float_emit.SXTL(8, D0, D0);
    float_emit.SXTL(16, D0, D0);
    float_emit.SCVTF(32, D0, D0);

    MOVP2R(addr_reg, &m_dequantizeTableS);
    ADD(scale_reg, addr_reg, scale_reg, ArithOption(scale_reg, ST_LSL, 3));
    float_emit.LDR(32, INDEX_UNSIGNED, D1, scale_reg, 0);
    float_emit.FMUL(32, D0, D0, D1, 0);
    RET(X30);
  }
  const u8* loadPairedU16One = GetCodePtr();
  {
    ADD(addr_reg, addr_reg, MEM_REG);
    float_emit.LDR(16, INDEX_UNSIGNED, D0, addr_reg, 0);
    float_emit.REV16(8, D0, D0);
    float_emit.UXTL(16, D0, D0);
    float_emit.UCVTF(32, D0, D0);

    MOVP2R(addr_reg, &m_dequantizeTableS);
    ADD(scale_reg, addr_reg, scale_reg, ArithOption(scale_reg, ST_LSL, 3));
    float_emit.LDR(32, INDEX_UNSIGNED, D1, scale_reg, 0);
    float_emit.FMUL(32, D0, D0, D1, 0);
    RET(X30);
  }
  const u8* loadPairedS16One = GetCodePtr();
  {
    ADD(addr_reg, addr_reg, MEM_REG);
    float_emit.LDR(16, INDEX_UNSIGNED, D0, addr_reg, 0);
    float_emit.REV16(8, D0, D0);
    float_emit.SXTL(16, D0, D0);
    float_emit.SCVTF(32, D0, D0);

    MOVP2R(addr_reg, &m_dequantizeTableS);
    ADD(scale_reg, addr_reg, scale_reg, ArithOption(scale_reg, ST_LSL, 3));
    float_emit.LDR(32, INDEX_UNSIGNED, D1, scale_reg, 0);
    float_emit.FMUL(32, D0, D0, D1, 0);
    RET(X30);
  }

  JitRegister::Register(start, GetCodePtr(), "JIT_QuantizedLoad");

  paired_load_quantized = reinterpret_cast<const u8**>(AlignCode16());
  ReserveCodeSpace(8 * sizeof(u8*));

  paired_load_quantized[0] = loadPairedFloatTwo;
  paired_load_quantized[1] = loadPairedIllegal;
  paired_load_quantized[2] = loadPairedIllegal;
  paired_load_quantized[3] = loadPairedIllegal;
  paired_load_quantized[4] = loadPairedU8Two;
  paired_load_quantized[5] = loadPairedU16Two;
  paired_load_quantized[6] = loadPairedS8Two;
  paired_load_quantized[7] = loadPairedS16Two;

  single_load_quantized = reinterpret_cast<const u8**>(AlignCode16());
  ReserveCodeSpace(8 * sizeof(u8*));

  single_load_quantized[0] = loadPairedFloatOne;
  single_load_quantized[1] = loadPairedIllegal;
  single_load_quantized[2] = loadPairedIllegal;
  single_load_quantized[3] = loadPairedIllegal;
  single_load_quantized[4] = loadPairedU8One;
  single_load_quantized[5] = loadPairedU16One;
  single_load_quantized[6] = loadPairedS8One;
  single_load_quantized[7] = loadPairedS16One;

  // Stores
  start = GetCodePtr();
  const u8* storePairedIllegal = GetCodePtr();
  BRK(0x101);
  const u8* storePairedFloat;
  const u8* storePairedFloatSlow;
  {
    storePairedFloat = GetCodePtr();
    float_emit.REV32(8, D0, D0);
    ADD(addr_reg, addr_reg, MEM_REG);
    float_emit.ST1(64, Q0, 0, addr_reg, SP);
    RET(X30);

    storePairedFloatSlow = GetCodePtr();
    float_emit.UMOV(64, X0, Q0, 0);
    ROR(X0, X0, 32);
    MOVP2R(X2, &PowerPC::Write_U64);
    BR(X2);
  }

  const u8* storePairedU8;
  const u8* storePairedU8Slow;
  {
    auto emit_quantize = [this, &float_emit, scale_reg]() {
      MOVP2R(X2, &m_quantizeTableS);
      ADD(scale_reg, X2, scale_reg, ArithOption(scale_reg, ST_LSL, 3));
      float_emit.LDR(32, INDEX_UNSIGNED, D1, scale_reg, 0);
      float_emit.FMUL(32, D0, D0, D1, 0);

      float_emit.FCVTZU(32, D0, D0);
      float_emit.UQXTN(16, D0, D0);
      float_emit.UQXTN(8, D0, D0);
    };

    storePairedU8 = GetCodePtr();
    emit_quantize();
    ADD(addr_reg, addr_reg, MEM_REG);
    float_emit.ST1(16, Q0, 0, addr_reg, SP);
    RET(X30);

    storePairedU8Slow = GetCodePtr();
    emit_quantize();
    float_emit.UMOV(16, W0, Q0, 0);
    REV16(W0, W0);
    MOVP2R(X2, &PowerPC::Write_U16);
    BR(X2);
  }
  const u8* storePairedS8;
  const u8* storePairedS8Slow;
  {
    auto emit_quantize = [this, &float_emit, scale_reg]() {
      MOVP2R(X2, &m_quantizeTableS);
      ADD(scale_reg, X2, scale_reg, ArithOption(scale_reg, ST_LSL, 3));
      float_emit.LDR(32, INDEX_UNSIGNED, D1, scale_reg, 0);
      float_emit.FMUL(32, D0, D0, D1, 0);

      float_emit.FCVTZS(32, D0, D0);
      float_emit.SQXTN(16, D0, D0);
      float_emit.SQXTN(8, D0, D0);
    };

    storePairedS8 = GetCodePtr();
    emit_quantize();
    ADD(addr_reg, addr_reg, MEM_REG);
    float_emit.ST1(16, Q0, 0, addr_reg, SP);
    RET(X30);

    storePairedS8Slow = GetCodePtr();
    emit_quantize();
    float_emit.UMOV(16, W0, Q0, 0);
    REV16(W0, W0);
    MOVP2R(X2, &PowerPC::Write_U16);
    BR(X2);
  }

  const u8* storePairedU16;
  const u8* storePairedU16Slow;
  {
    auto emit_quantize = [this, &float_emit, scale_reg]() {
      MOVP2R(X2, &m_quantizeTableS);
      ADD(scale_reg, X2, scale_reg, ArithOption(scale_reg, ST_LSL, 3));
      float_emit.LDR(32, INDEX_UNSIGNED, D1, scale_reg, 0);
      float_emit.FMUL(32, D0, D0, D1, 0);

      float_emit.FCVTZU(32, D0, D0);
      float_emit.UQXTN(16, D0, D0);
      float_emit.REV16(8, D0, D0);
    };

    storePairedU16 = GetCodePtr();
    emit_quantize();
    ADD(addr_reg, addr_reg, MEM_REG);
    float_emit.ST1(32, Q0, 0, addr_reg, SP);
    RET(X30);

    storePairedU16Slow = GetCodePtr();
    emit_quantize();
    float_emit.REV32(8, D0, D0);
    float_emit.UMOV(32, W0, Q0, 0);
    MOVP2R(X2, &PowerPC::Write_U32);
    BR(X2);
  }
  const u8* storePairedS16;  // Used by Viewtiful Joe's intro movie
  const u8* storePairedS16Slow;
  {
    auto emit_quantize = [this, &float_emit, scale_reg]() {
      MOVP2R(X2, &m_quantizeTableS);
      ADD(scale_reg, X2, scale_reg, ArithOption(scale_reg, ST_LSL, 3));
      float_emit.LDR(32, INDEX_UNSIGNED, D1, scale_reg, 0);
      float_emit.FMUL(32, D0, D0, D1, 0);

      float_emit.FCVTZS(32, D0, D0);
      float_emit.SQXTN(16, D0, D0);
      float_emit.REV16(8, D0, D0);
    };

    storePairedS16 = GetCodePtr();
    emit_quantize();
    ADD(addr_reg, addr_reg, MEM_REG);
    float_emit.ST1(32, Q0, 0, addr_reg, SP);
    RET(X30);

    storePairedS16Slow = GetCodePtr();
    emit_quantize();
    float_emit.REV32(8, D0, D0);
    float_emit.UMOV(32, W0, Q0, 0);
    MOVP2R(X2, &PowerPC::Write_U32);
    BR(X2);
  }

  const u8* storeSingleFloat;
  const u8* storeSingleFloatSlow;
  {
    storeSingleFloat = GetCodePtr();
    float_emit.REV32(8, D0, D0);
    ADD(addr_reg, addr_reg, MEM_REG);
    float_emit.STR(32, INDEX_UNSIGNED, D0, addr_reg, 0);
    RET(X30);

    storeSingleFloatSlow = GetCodePtr();
    float_emit.UMOV(32, W0, Q0, 0);
    MOVP2R(X2, &PowerPC::Write_U32);
    BR(X2);
  }
  const u8* storeSingleU8;  // Used by MKWii
  const u8* storeSingleU8Slow;
  {
    auto emit_quantize = [this, &float_emit, scale_reg]() {
      MOVP2R(X2, &m_quantizeTableS);
      ADD(scale_reg, X2, scale_reg, ArithOption(scale_reg, ST_LSL, 3));
      float_emit.LDR(32, INDEX_UNSIGNED, D1, scale_reg, 0);
      float_emit.FMUL(32, D0, D0, D1);

      float_emit.FCVTZU(32, D0, D0);
      float_emit.UQXTN(16, D0, D0);
      float_emit.UQXTN(8, D0, D0);
    };

    storeSingleU8 = GetCodePtr();
    emit_quantize();
    ADD(addr_reg, addr_reg, MEM_REG);
    float_emit.ST1(8, Q0, 0, addr_reg);
    RET(X30);

    storeSingleU8Slow = GetCodePtr();
    emit_quantize();
    float_emit.UMOV(8, W0, Q0, 0);
    MOVP2R(X2, &PowerPC::Write_U8);
    BR(X2);
  }
  const u8* storeSingleS8;
  const u8* storeSingleS8Slow;
  {
    auto emit_quantize = [this, &float_emit, scale_reg]() {
      MOVP2R(X2, &m_quantizeTableS);
      ADD(scale_reg, X2, scale_reg, ArithOption(scale_reg, ST_LSL, 3));
      float_emit.LDR(32, INDEX_UNSIGNED, D1, scale_reg, 0);
      float_emit.FMUL(32, D0, D0, D1);

      float_emit.FCVTZS(32, D0, D0);
      float_emit.SQXTN(16, D0, D0);
      float_emit.SQXTN(8, D0, D0);
    };

    storeSingleS8 = GetCodePtr();
    emit_quantize();
    ADD(addr_reg, addr_reg, MEM_REG);
    float_emit.ST1(8, Q0, 0, addr_reg);
    RET(X30);

    storeSingleS8Slow = GetCodePtr();
    emit_quantize();
    float_emit.SMOV(8, W0, Q0, 0);
    MOVP2R(X2, &PowerPC::Write_U8);
    BR(X2);
  }
  const u8* storeSingleU16;  // Used by MKWii
  const u8* storeSingleU16Slow;
  {
    auto emit_quantize = [this, &float_emit, scale_reg]() {
      MOVP2R(X2, &m_quantizeTableS);
      ADD(scale_reg, X2, scale_reg, ArithOption(scale_reg, ST_LSL, 3));
      float_emit.LDR(32, INDEX_UNSIGNED, D1, scale_reg, 0);
      float_emit.FMUL(32, D0, D0, D1);

      float_emit.FCVTZU(32, D0, D0);
      float_emit.UQXTN(16, D0, D0);
    };

    storeSingleU16 = GetCodePtr();
    emit_quantize();
    ADD(addr_reg, addr_reg, MEM_REG);
    float_emit.REV16(8, D0, D0);
    float_emit.ST1(16, Q0, 0, addr_reg);
    RET(X30);

    storeSingleU16Slow = GetCodePtr();
    emit_quantize();
    float_emit.UMOV(16, W0, Q0, 0);
    MOVP2R(X2, &PowerPC::Write_U16);
    BR(X2);
  }
  const u8* storeSingleS16;
  const u8* storeSingleS16Slow;
  {
    auto emit_quantize = [this, &float_emit, scale_reg]() {
      MOVP2R(X2, &m_quantizeTableS);
      ADD(scale_reg, X2, scale_reg, ArithOption(scale_reg, ST_LSL, 3));
      float_emit.LDR(32, INDEX_UNSIGNED, D1, scale_reg, 0);
      float_emit.FMUL(32, D0, D0, D1);

      float_emit.FCVTZS(32, D0, D0);
      float_emit.SQXTN(16, D0, D0);
    };

    storeSingleS16 = GetCodePtr();
    emit_quantize();
    ADD(addr_reg, addr_reg, MEM_REG);
    float_emit.REV16(8, D0, D0);
    float_emit.ST1(16, Q0, 0, addr_reg);
    RET(X30);

    storeSingleS16Slow = GetCodePtr();
    emit_quantize();
    float_emit.SMOV(16, W0, Q0, 0);
    MOVP2R(X2, &PowerPC::Write_U16);
    BR(X2);
  }

  JitRegister::Register(start, GetCodePtr(), "JIT_QuantizedStore");

  paired_store_quantized = reinterpret_cast<const u8**>(AlignCode16());
  ReserveCodeSpace(32 * sizeof(u8*));

  // Fast
  paired_store_quantized[0] = storePairedFloat;
  paired_store_quantized[1] = storePairedIllegal;
  paired_store_quantized[2] = storePairedIllegal;
  paired_store_quantized[3] = storePairedIllegal;
  paired_store_quantized[4] = storePairedU8;
  paired_store_quantized[5] = storePairedU16;
  paired_store_quantized[6] = storePairedS8;
  paired_store_quantized[7] = storePairedS16;

  paired_store_quantized[8] = storeSingleFloat;
  paired_store_quantized[9] = storePairedIllegal;
  paired_store_quantized[10] = storePairedIllegal;
  paired_store_quantized[11] = storePairedIllegal;
  paired_store_quantized[12] = storeSingleU8;
  paired_store_quantized[13] = storeSingleU16;
  paired_store_quantized[14] = storeSingleS8;
  paired_store_quantized[15] = storeSingleS16;

  // Slow
  paired_store_quantized[16] = storePairedFloatSlow;
  paired_store_quantized[17] = storePairedIllegal;
  paired_store_quantized[18] = storePairedIllegal;
  paired_store_quantized[19] = storePairedIllegal;
  paired_store_quantized[20] = storePairedU8Slow;
  paired_store_quantized[21] = storePairedU16Slow;
  paired_store_quantized[22] = storePairedS8Slow;
  paired_store_quantized[23] = storePairedS16Slow;

  paired_store_quantized[24] = storeSingleFloatSlow;
  paired_store_quantized[25] = storePairedIllegal;
  paired_store_quantized[26] = storePairedIllegal;
  paired_store_quantized[27] = storePairedIllegal;
  paired_store_quantized[28] = storeSingleU8Slow;
  paired_store_quantized[29] = storeSingleU16Slow;
  paired_store_quantized[30] = storeSingleS8Slow;
  paired_store_quantized[31] = storeSingleS16Slow;

  GetAsmRoutines()->mfcr = nullptr;
}
