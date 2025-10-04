// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/DSP/Jit/x64/DSPEmitter.h"

#include "Common/CommonTypes.h"

#include "Core/DSP/DSPAnalyzer.h"
#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPTables.h"

using namespace Gen;

namespace DSP::JIT::x64
{
void DSPEmitter::ReJitConditional(
    const UDSPInstruction opc, void (DSPEmitter::*conditional_fn)(UDSPInstruction))
{
  u8 cond = opc & 0xf;
  if (cond == 0xf)  // Always true.
  {
    (this->*conditional_fn)(opc);
    return;
  }

  dsp_op_read_reg(DSP_REG_SR, EAX);

  switch (cond)
  {
  case 0x0:  // GE - Greater Equal
  case 0x1:  // L - Less
    LEA(16, EDX, MScaled(EAX, SCALE_4, 0));
    XOR(16, R(EAX), R(EDX));
    TEST(16, R(EAX), Imm16(8));
    break;
  case 0x2:  // G - Greater
  case 0x3:  // LE - Less Equal
    LEA(16, EDX, MScaled(EAX, SCALE_4, 0));
    XOR(16, R(EAX), R(EDX));
    ADD(16, R(EAX), R(EAX));
    OR(16, R(EAX), R(EDX));
    TEST(16, R(EAX), Imm16(0x10));
    break;
  case 0x4:  // NZ - Not Zero
  case 0x5:  // Z - Zero
    TEST(16, R(EAX), Imm16(SR_ARITH_ZERO));
    break;
  case 0x6:  // NC - Not carry
  case 0x7:  // C - Carry
    TEST(16, R(EAX), Imm16(SR_CARRY));
    break;
  case 0x8:  // ? - Not over s32
  case 0x9:  // ? - Over s32
    TEST(16, R(EAX), Imm16(SR_OVER_S32));
    break;
  case 0xa:  // ?
  case 0xb:  // ?
    // We want to test this expression, which corresponds to xB:
    // (!(IsSRFlagSet(SR_OVER_S32) || IsSRFlagSet(SR_TOP2BITS))) || IsSRFlagSet(SR_ARITH_ZERO)
    // The xB expression is used due to even instructions (i.e. xA) looking for the expression to
    // evaluate to false, while odd ones look for it to be true.

    // Since SR_OVER_S32 is bit 4 (0x10) and SR_TOP2BITS is bit 5 (0x20),
    // set EDX to 2*EAX, so that SR_OVER_S32 is in bit 5 of EDX.
    LEA(16, EDX, MRegSum(EAX, EAX));
    // Now OR them together, so bit 5 of EDX is
    // (IsSRFlagSet(SR_OVER_S32) || IsSRFlagSet(SR_TOP2BITS))
    OR(16, R(EDX), R(EAX));
    // EDX bit 5 is !(IsSRFlagSet(SR_OVER_S32) || IsSRFlagSet(SR_TOP2BITS))
    NOT(16, R(EDX));
    // SR_ARITH_ZERO is bit 2 (0x04).  We want that in bit 5, so shift left by 3.
    SHL(16, R(EAX), Imm8(3));
    // Bit 5 of EAX is IsSRFlagSet(SR_OVER_S32), so or-ing EDX with EAX gives our target expression.
    OR(16, R(EDX), R(EAX));
    // Test bit 5
    TEST(16, R(EDX), Imm16(0x20));
    break;
  case 0xc:  // LNZ  - Logic Not Zero
  case 0xd:  // LZ - Logic Zero
    TEST(16, R(EAX), Imm16(SR_LOGIC_ZERO));
    break;
  case 0xe:  // O - Overflow
    TEST(16, R(EAX), Imm16(SR_OVERFLOW));
    break;
  }
  DSPJitRegCache c1(m_gpr);
  CCFlags flag;
  if (cond == 0xe)  // Overflow, special case as there is no inverse case
    flag = CC_Z;
  else if ((cond & 1) == 0)  // Even conditions run if the bit is zero, so jump if it IS NOT zero
    flag = CC_NZ;
  else  // Odd conditions run if the bit IS NOT zero, so jump if it IS zero
    flag = CC_Z;
  FixupBranch skip_code = J_CC(flag, Jump::Near);
  (this->*conditional_fn)(opc);
  m_gpr.FlushRegs(c1);
  SetJumpTarget(skip_code);
}

void DSPEmitter::WriteBranchExit()
{
  DSPJitRegCache c(m_gpr);
  m_gpr.SaveRegs();
  if (m_dsp_core.DSPState().GetAnalyzer().IsIdleSkip(m_start_address))
  {
    MOV(16, R(EAX), Imm16(0x1000));
  }
  else
  {
    MOV(16, R(EAX), Imm16(m_block_size[m_start_address]));
  }
  JMP(m_return_dispatcher, Jump::Near);
  m_gpr.LoadRegs(false);
  m_gpr.FlushRegs(c, false);
}

void DSPEmitter::WriteBlockLink(u16 dest)
{
  // Jump directly to the called block if it has already been compiled.
  if (!(dest >= m_start_address && dest <= m_compile_pc))
  {
    if (m_block_links[dest] != nullptr)
    {
      m_gpr.FlushRegs();
      // Check if we have enough cycles to execute the next block
      MOV(64, R(RAX), ImmPtr(&m_cycles_left));
      MOV(16, R(ECX), MatR(RAX));
      CMP(16, R(ECX), Imm16(m_block_size[m_start_address] + m_block_size[dest]));
      FixupBranch notEnoughCycles = J_CC(CC_BE);

      SUB(16, R(ECX), Imm16(m_block_size[m_start_address]));
      MOV(16, MatR(RAX), R(ECX));
      JMP(m_block_links[dest], Jump::Near);
      SetJumpTarget(notEnoughCycles);
    }
    else
    {
      // The destination has not been compiled yet.  Add it to the list
      // of blocks that this block is waiting on.
      m_unresolved_jumps[m_start_address].push_back(dest);
    }
  }
}

void DSPEmitter::r_jcc(const UDSPInstruction opc)
{
  const u16 dest = m_dsp_core.DSPState().ReadIMEM(m_compile_pc + 1);
  const DSPOPCTemplate* opcode = GetOpTemplate(opc);

  // If the block is unconditional, attempt to link block
  if (opcode->uncond_branch)
    WriteBlockLink(dest);
  MOV(16, M_SDSP_pc(), Imm16(dest));
  WriteBranchExit();
}
// Generic jmp implementation
// Jcc addressA
// 0000 0010 1001 cccc
// aaaa aaaa aaaa aaaa
// Jump to addressA if condition cc has been met. Set program counter to
// address represented by value that follows this "jmp" instruction.
// NOTE: Cannot use FallBackToInterpreter(opc) here because of the need to write branch exit
void DSPEmitter::jcc(const UDSPInstruction opc)
{
  MOV(16, M_SDSP_pc(), Imm16(m_compile_pc + 2));
  ReJitConditional(opc, &DSPEmitter::r_jcc);
}

void DSPEmitter::r_jmprcc(const UDSPInstruction opc)
{
  u8 reg = (opc >> 5) & 0x7;
  // reg can only be DSP_REG_ARx and DSP_REG_IXx now,
  // no need to handle DSP_REG_STx.
  dsp_op_read_reg(reg, RAX);
  MOV(16, M_SDSP_pc(), R(EAX));
  WriteBranchExit();
}
// Generic jmpr implementation
// JRcc $R
// 0001 0111 rrr0 cccc
// Jump to address if condition cc has been met. Set program counter to
// a value from register $R.
// NOTE: Cannot use FallBackToInterpreter(opc) here because of the need to write branch exit
void DSPEmitter::jmprcc(const UDSPInstruction opc)
{
  MOV(16, M_SDSP_pc(), Imm16(m_compile_pc + 1));
  ReJitConditional(opc, &DSPEmitter::r_jmprcc);
}

void DSPEmitter::r_call(const UDSPInstruction opc)
{
  MOV(16, R(DX), Imm16(m_compile_pc + 2));
  dsp_reg_store_stack(StackRegister::Call);
  const u16 dest = m_dsp_core.DSPState().ReadIMEM(m_compile_pc + 1);
  const DSPOPCTemplate* opcode = GetOpTemplate(opc);

  // If the block is unconditional, attempt to link block
  if (opcode->uncond_branch)
    WriteBlockLink(dest);
  MOV(16, M_SDSP_pc(), Imm16(dest));
  WriteBranchExit();
}
// Generic call implementation
// CALLcc addressA
// 0000 0010 1011 cccc
// aaaa aaaa aaaa aaaa
// Call function if condition cc has been met. Push program counter of
// instruction following "call" to $st0. Set program counter to address
// represented by value that follows this "call" instruction.
// NOTE: Cannot use FallBackToInterpreter(opc) here because of the need to write branch exit
void DSPEmitter::call(const UDSPInstruction opc)
{
  MOV(16, M_SDSP_pc(), Imm16(m_compile_pc + 2));
  ReJitConditional(opc, &DSPEmitter::r_call);
}

void DSPEmitter::r_callr(const UDSPInstruction opc)
{
  u8 reg = (opc >> 5) & 0x7;
  MOV(16, R(DX), Imm16(m_compile_pc + 1));
  dsp_reg_store_stack(StackRegister::Call);
  dsp_op_read_reg(reg, RAX);
  MOV(16, M_SDSP_pc(), R(EAX));
  WriteBranchExit();
}
// Generic callr implementation
// CALLRcc $R
// 0001 0111 rrr1 cccc
// Call function if condition cc has been met. Push program counter of
// instruction following "call" to call stack $st0. Set program counter to
// register $R.
// NOTE: Cannot use FallBackToInterpreter(opc) here because of the need to write branch exit
void DSPEmitter::callr(const UDSPInstruction opc)
{
  MOV(16, M_SDSP_pc(), Imm16(m_compile_pc + 1));
  ReJitConditional(opc, &DSPEmitter::r_callr);
}

void DSPEmitter::r_ifcc(const UDSPInstruction opc)
{
  MOV(16, M_SDSP_pc(), Imm16(m_compile_pc + 1));
}
// Generic if implementation
// IFcc
// 0000 0010 0111 cccc
// Execute following opcode if the condition has been met.
// NOTE: Cannot use FallBackToInterpreter(opc) here because of the need to write branch exit
void DSPEmitter::ifcc(const UDSPInstruction opc)
{
  const auto& state = m_dsp_core.DSPState();
  const u16 address = m_compile_pc + 1;
  const DSPOPCTemplate* const op_template = GetOpTemplate(state.ReadIMEM(address));

  MOV(16, M_SDSP_pc(), Imm16(address + op_template->size));
  ReJitConditional(opc, &DSPEmitter::r_ifcc);
  WriteBranchExit();
}

void DSPEmitter::r_ret(const UDSPInstruction opc)
{
  dsp_reg_load_stack(StackRegister::Call);
  MOV(16, M_SDSP_pc(), R(DX));
  WriteBranchExit();
}

// Generic ret implementation
// RETcc
// 0000 0010 1101 cccc
// Return from subroutine if condition cc has been met. Pops stored PC
// from call stack $st0 and sets $pc to this location.
// NOTE: Cannot use FallBackToInterpreter(opc) here because of the need to write branch exit
void DSPEmitter::ret(const UDSPInstruction opc)
{
  MOV(16, M_SDSP_pc(), Imm16(m_compile_pc + 1));
  ReJitConditional(opc, &DSPEmitter::r_ret);
}

void DSPEmitter::r_rti(const UDSPInstruction opc)
{
  //	g_dsp.r[DSP_REG_SR] = dsp_reg_load_stack(StackRegister::Data);
  dsp_reg_load_stack(StackRegister::Data);
  dsp_op_write_reg(DSP_REG_SR, RDX);
  //	g_dsp.pc = dsp_reg_load_stack(StackRegister::Call);
  dsp_reg_load_stack(StackRegister::Call);
  MOV(16, M_SDSP_pc(), R(DX));
  WriteBranchExit();
}

// RTIcc
// 0000 0010 1111 1111
// Return from exception. Pops stored status register $sr from data stack
// $st1 and program counter PC from call stack $st0 and sets $pc to this
// location.
// This instruction has a conditional form, but it is not used by any official ucode.
// NOTE: Cannot use FallBackToInterpreter(opc) here because of the need to write branch exit
void DSPEmitter::rti(const UDSPInstruction opc)
{
  MOV(16, M_SDSP_pc(), Imm16(m_compile_pc + 1));
  ReJitConditional(opc, &DSPEmitter::r_rti);
}

// HALT
// 0000 0000 0010 0001
// Stops execution of DSP code. Sets bit DSP_CR_HALT in register DREG_CR.
void DSPEmitter::halt(const UDSPInstruction)
{
  OR(16, M_SDSP_control_reg(), Imm16(CR_HALT));
  //	g_dsp.pc = dsp_reg_load_stack(StackRegister::Call);
  dsp_reg_load_stack(StackRegister::Call);
  MOV(16, M_SDSP_pc(), R(DX));
}

// LOOP handling: Loop stack is used to control execution of repeated blocks of
// instructions. Whenever there is value on stack $st2 and current PC is equal
// value at $st2, then value at stack $st3 is decremented. If value is not zero
// then PC is modified with value from call stack $st0. Otherwise values from
// call stack $st0 and both loop stacks $st2 and $st3 are popped and execution
// continues at next opcode.
void DSPEmitter::HandleLoop()
{
  MOVZX(32, 16, EAX, M_SDSP_r_st(2));
  MOVZX(32, 16, ECX, M_SDSP_r_st(3));

  TEST(32, R(RCX), R(RCX));
  FixupBranch rLoopCntG = J_CC(CC_E, Jump::Near);
  CMP(16, R(RAX), Imm16(m_compile_pc - 1));
  FixupBranch rLoopAddrG = J_CC(CC_NE, Jump::Near);

  SUB(16, M_SDSP_r_st(3), Imm16(1));
  CMP(16, M_SDSP_r_st(3), Imm16(0));

  FixupBranch loadStack = J_CC(CC_E, Jump::Near);
  MOVZX(32, 16, ECX, M_SDSP_r_st(0));
  MOV(16, M_SDSP_pc(), R(RCX));
  FixupBranch loopUpdated = J(Jump::Near);

  SetJumpTarget(loadStack);
  DSPJitRegCache c(m_gpr);
  dsp_reg_load_stack(StackRegister::Call);
  dsp_reg_load_stack(StackRegister::LoopAddress);
  dsp_reg_load_stack(StackRegister::LoopCounter);
  m_gpr.FlushRegs(c);

  SetJumpTarget(loopUpdated);
  SetJumpTarget(rLoopAddrG);
  SetJumpTarget(rLoopCntG);
}

// LOOP $R
// 0000 0000 010r rrrr
// Repeatedly execute following opcode until counter specified by value
// from register $R reaches zero. Each execution decrement counter. Register
// $R remains unchanged. If register $R is set to zero at the beginning of loop
// then looped instruction will not get executed.
// Actually, this instruction simply prepares the loop stacks for the above.
// The looping hardware takes care of the rest.
void DSPEmitter::loop(const UDSPInstruction opc)
{
  u16 reg = opc & 0x1f;
  //	u16 cnt = g_dsp.r[reg];
  dsp_op_read_reg(reg, RDX, RegisterExtension::Zero);
  u16 loop_pc = m_compile_pc + 1;

  TEST(16, R(EDX), R(EDX));
  DSPJitRegCache c(m_gpr);
  FixupBranch cnt = J_CC(CC_Z, Jump::Near);
  dsp_reg_store_stack(StackRegister::LoopCounter);
  MOV(16, R(RDX), Imm16(m_compile_pc + 1));
  dsp_reg_store_stack(StackRegister::Call);
  MOV(16, R(RDX), Imm16(loop_pc));
  dsp_reg_store_stack(StackRegister::LoopAddress);
  m_gpr.FlushRegs(c);
  MOV(16, M_SDSP_pc(), Imm16(m_compile_pc + 1));
  FixupBranch exit = J(Jump::Near);

  SetJumpTarget(cnt);
  // dsp_skip_inst();
  const auto& state = m_dsp_core.DSPState();
  MOV(16, M_SDSP_pc(), Imm16(loop_pc + GetOpTemplate(state.ReadIMEM(loop_pc))->size));
  WriteBranchExit();
  m_gpr.FlushRegs(c, false);
  SetJumpTarget(exit);
}

// LOOPI #I
// 0001 0000 iiii iiii
// Repeatedly execute following opcode until counter specified by
// immediate value I reaches zero. Each execution decrement counter. If
// immediate value I is set to zero at the beginning of loop then looped
// instruction will not get executed.
// Actually, this instruction simply prepares the loop stacks for the above.
// The looping hardware takes care of the rest.
void DSPEmitter::loopi(const UDSPInstruction opc)
{
  u16 cnt = opc & 0xff;
  u16 loop_pc = m_compile_pc + 1;

  if (cnt)
  {
    MOV(16, R(RDX), Imm16(m_compile_pc + 1));
    dsp_reg_store_stack(StackRegister::Call);
    MOV(16, R(RDX), Imm16(loop_pc));
    dsp_reg_store_stack(StackRegister::LoopAddress);
    MOV(16, R(RDX), Imm16(cnt));
    dsp_reg_store_stack(StackRegister::LoopCounter);

    MOV(16, M_SDSP_pc(), Imm16(m_compile_pc + 1));
  }
  else
  {
    // dsp_skip_inst();
    const auto& state = m_dsp_core.DSPState();
    MOV(16, M_SDSP_pc(), Imm16(loop_pc + GetOpTemplate(state.ReadIMEM(loop_pc))->size));
    WriteBranchExit();
  }
}

// BLOOP $R, addrA
// 0000 0000 011r rrrr
// aaaa aaaa aaaa aaaa
// Repeatedly execute block of code starting at following opcode until
// counter specified by value from register $R reaches zero. Block ends at
// specified address addrA inclusive, ie. opcode at addrA is the last opcode
// included in loop. Counter is pushed on loop stack $st3, end of block address
// is pushed on loop stack $st2 and repeat address is pushed on call stack $st0.
// Up to 4 nested loops are allowed.
void DSPEmitter::bloop(const UDSPInstruction opc)
{
  const u16 reg = opc & 0x1f;
  //	u16 cnt = g_dsp.r[reg];
  dsp_op_read_reg(reg, RDX, RegisterExtension::Zero);
  const u16 loop_pc = m_dsp_core.DSPState().ReadIMEM(m_compile_pc + 1);

  TEST(16, R(EDX), R(EDX));
  DSPJitRegCache c(m_gpr);
  FixupBranch cnt = J_CC(CC_Z, Jump::Near);
  dsp_reg_store_stack(StackRegister::LoopCounter);
  MOV(16, R(RDX), Imm16(m_compile_pc + 2));
  dsp_reg_store_stack(StackRegister::Call);
  MOV(16, R(RDX), Imm16(loop_pc));
  dsp_reg_store_stack(StackRegister::LoopAddress);
  MOV(16, M_SDSP_pc(), Imm16(m_compile_pc + 2));
  m_gpr.FlushRegs(c, true);
  FixupBranch exit = J(Jump::Near);

  SetJumpTarget(cnt);
  // g_dsp.pc = loop_pc;
  // dsp_skip_inst();
  const auto& state = m_dsp_core.DSPState();
  MOV(16, M_SDSP_pc(), Imm16(loop_pc + GetOpTemplate(state.ReadIMEM(loop_pc))->size));
  WriteBranchExit();
  m_gpr.FlushRegs(c, false);
  SetJumpTarget(exit);
}

// BLOOPI #I, addrA
// 0001 0001 iiii iiii
// aaaa aaaa aaaa aaaa
// Repeatedly execute block of code starting at following opcode until
// counter specified by immediate value I reaches zero. Block ends at specified
// address addrA inclusive, ie. opcode at addrA is the last opcode included in
// loop. Counter is pushed on loop stack $st3, end of block address is pushed
// on loop stack $st2 and repeat address is pushed on call stack $st0. Up to 4
// nested loops are allowed.
void DSPEmitter::bloopi(const UDSPInstruction opc)
{
  const auto& state = m_dsp_core.DSPState();
  const u16 cnt = opc & 0xff;
  //	u16 loop_pc = dsp_fetch_code();
  const u16 loop_pc = state.ReadIMEM(m_compile_pc + 1);

  if (cnt != 0)
  {
    MOV(16, R(RDX), Imm16(m_compile_pc + 2));
    dsp_reg_store_stack(StackRegister::Call);
    MOV(16, R(RDX), Imm16(loop_pc));
    dsp_reg_store_stack(StackRegister::LoopAddress);
    MOV(16, R(RDX), Imm16(cnt));
    dsp_reg_store_stack(StackRegister::LoopCounter);

    MOV(16, M_SDSP_pc(), Imm16(m_compile_pc + 2));
  }
  else
  {
    // g_dsp.pc = loop_pc;
    // dsp_skip_inst();
    MOV(16, M_SDSP_pc(), Imm16(loop_pc + GetOpTemplate(state.ReadIMEM(loop_pc))->size));
    WriteBranchExit();
  }
}

}  // namespace DSP::JIT::x64
