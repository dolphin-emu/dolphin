// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
//
// Additional copyrights go to Duddie and Tratax (c) 2004

#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPMemoryMap.h"
#include "Core/DSP/DSPStacks.h"
#include "Core/DSP/Interpreter/DSPIntCCUtil.h"
#include "Core/DSP/Interpreter/DSPIntUtil.h"
#include "Core/DSP/Interpreter/DSPInterpreter.h"

namespace DSP::Interpreter
{
// Generic call implementation
// CALLcc addressA
// 0000 0010 1011 cccc
// aaaa aaaa aaaa aaaa
// Call function if condition cc has been met. Push program counter of
// instruction following "call" to $st0. Set program counter to address
// represented by value that follows this "call" instruction.
void call(const UDSPInstruction opc)
{
  // must be outside the if.
  u16 dest = dsp_fetch_code();
  if (CheckCondition(opc & 0xf))
  {
    dsp_reg_store_stack(StackRegister::Call, g_dsp.pc);
    g_dsp.pc = dest;
  }
}

// Generic callr implementation
// CALLRcc $R
// 0001 0111 rrr1 cccc
// Call function if condition cc has been met. Push program counter of
// instruction following "call" to call stack $st0. Set program counter to
// register $R.
void callr(const UDSPInstruction opc)
{
  if (CheckCondition(opc & 0xf))
  {
    u8 reg = (opc >> 5) & 0x7;
    u16 addr = dsp_op_read_reg(reg);
    dsp_reg_store_stack(StackRegister::Call, g_dsp.pc);
    g_dsp.pc = addr;
  }
}

// Generic if implementation
// IFcc
// 0000 0010 0111 cccc
// Execute following opcode if the condition has been met.
void ifcc(const UDSPInstruction opc)
{
  if (!CheckCondition(opc & 0xf))
  {
    // skip the next opcode - we have to lookup its size.
    dsp_skip_inst();
  }
}

// Generic jmp implementation
// Jcc addressA
// 0000 0010 1001 cccc
// aaaa aaaa aaaa aaaa
// Jump to addressA if condition cc has been met. Set program counter to
// address represented by value that follows this "jmp" instruction.
void jcc(const UDSPInstruction opc)
{
  u16 dest = dsp_fetch_code();
  if (CheckCondition(opc & 0xf))
  {
    g_dsp.pc = dest;
  }
}

// Generic jmpr implementation
// JMPcc $R
// 0001 0111 rrr0 cccc
// Jump to address; set program counter to a value from register $R.
void jmprcc(const UDSPInstruction opc)
{
  if (CheckCondition(opc & 0xf))
  {
    u8 reg = (opc >> 5) & 0x7;
    g_dsp.pc = dsp_op_read_reg(reg);
  }
}

// Generic ret implementation
// RETcc
// 0000 0010 1101 cccc
// Return from subroutine if condition cc has been met. Pops stored PC
// from call stack $st0 and sets $pc to this location.
void ret(const UDSPInstruction opc)
{
  if (CheckCondition(opc & 0xf))
  {
    g_dsp.pc = dsp_reg_load_stack(StackRegister::Call);
  }
}

// RTI
// 0000 0010 1111 1111
// Return from exception. Pops stored status register $sr from data stack
// $st1 and program counter PC from call stack $st0 and sets $pc to this
// location.
void rti(const UDSPInstruction opc)
{
  g_dsp.r.sr = dsp_reg_load_stack(StackRegister::Data);
  g_dsp.pc = dsp_reg_load_stack(StackRegister::Call);
}

// HALT
// 0000 0000 0020 0001
// Stops execution of DSP code. Sets bit DSP_CR_HALT in register DREG_CR.
void halt(const UDSPInstruction opc)
{
  g_dsp.cr |= 0x4;
  g_dsp.pc--;
}

// LOOP handling: Loop stack is used to control execution of repeated blocks of
// instructions. Whenever there is value on stack $st2 and current PC is equal
// value at $st2, then value at stack $st3 is decremented. If value is not zero
// then PC is modified with value from call stack $st0. Otherwise values from
// call stack $st0 and both loop stacks $st2 and $st3 are popped and execution
// continues at next opcode.
void HandleLoop()
{
  // Handle looping hardware.
  const u16 rCallAddress = g_dsp.r.st[0];
  const u16 rLoopAddress = g_dsp.r.st[2];
  u16& rLoopCounter = g_dsp.r.st[3];

  if (rLoopAddress > 0 && rLoopCounter > 0)
  {
    // FIXME: why -1? because we just read past it.
    if (g_dsp.pc - 1 == rLoopAddress)
    {
      rLoopCounter--;
      if (rLoopCounter > 0)
      {
        g_dsp.pc = rCallAddress;
      }
      else
      {
        // end of loop
        dsp_reg_load_stack(StackRegister::Call);
        dsp_reg_load_stack(StackRegister::LoopAddress);
        dsp_reg_load_stack(StackRegister::LoopCounter);
      }
    }
  }
}

// LOOP $R
// 0000 0000 010r rrrr
// Repeatedly execute following opcode until counter specified by value
// from register $R reaches zero. Each execution decrement counter. Register
// $R remains unchanged. If register $R is set to zero at the beginning of loop
// then looped instruction will not get executed.
// Actually, this instruction simply prepares the loop stacks for the above.
// The looping hardware takes care of the rest.
void loop(const UDSPInstruction opc)
{
  u16 reg = opc & 0x1f;
  u16 cnt = dsp_op_read_reg(reg);
  u16 loop_pc = g_dsp.pc;

  if (cnt)
  {
    dsp_reg_store_stack(StackRegister::Call, g_dsp.pc);
    dsp_reg_store_stack(StackRegister::LoopAddress, loop_pc);
    dsp_reg_store_stack(StackRegister::LoopCounter, cnt);
  }
  else
  {
    dsp_skip_inst();
  }
}

// LOOPI #I
// 0001 0000 iiii iiii
// Repeatedly execute following opcode until counter specified by
// immediate value I reaches zero. Each execution decrement counter. If
// immediate value I is set to zero at the beginning of loop then looped
// instruction will not get executed.
// Actually, this instruction simply prepares the loop stacks for the above.
// The looping hardware takes care of the rest.
void loopi(const UDSPInstruction opc)
{
  u16 cnt = opc & 0xff;
  u16 loop_pc = g_dsp.pc;

  if (cnt)
  {
    dsp_reg_store_stack(StackRegister::Call, g_dsp.pc);
    dsp_reg_store_stack(StackRegister::LoopAddress, loop_pc);
    dsp_reg_store_stack(StackRegister::LoopCounter, cnt);
  }
  else
  {
    dsp_skip_inst();
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
void bloop(const UDSPInstruction opc)
{
  u16 reg = opc & 0x1f;
  u16 cnt = dsp_op_read_reg(reg);
  u16 loop_pc = dsp_fetch_code();

  if (cnt)
  {
    dsp_reg_store_stack(StackRegister::Call, g_dsp.pc);
    dsp_reg_store_stack(StackRegister::LoopAddress, loop_pc);
    dsp_reg_store_stack(StackRegister::LoopCounter, cnt);
  }
  else
  {
    g_dsp.pc = loop_pc;
    dsp_skip_inst();
  }
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
void bloopi(const UDSPInstruction opc)
{
  u16 cnt = opc & 0xff;
  u16 loop_pc = dsp_fetch_code();

  if (cnt)
  {
    dsp_reg_store_stack(StackRegister::Call, g_dsp.pc);
    dsp_reg_store_stack(StackRegister::LoopAddress, loop_pc);
    dsp_reg_store_stack(StackRegister::LoopCounter, cnt);
  }
  else
  {
    g_dsp.pc = loop_pc;
    dsp_skip_inst();
  }
}
}  // namespace DSP::Interpreter
