// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Additional copyrights go to Duddie and Tratax (c) 2004

#include "Core/DSP/Interpreter/DSPInterpreter.h"

#include "Core/DSP/DSPCore.h"

namespace DSP::Interpreter
{
// Generic call implementation
// CALLcc addressA
// 0000 0010 1011 cccc
// aaaa aaaa aaaa aaaa
// Call function if condition cc has been met. Push program counter of
// instruction following "call" to $st0. Set program counter to address
// represented by value that follows this "call" instruction.
void Interpreter::call(const UDSPInstruction opc)
{
  auto& state = m_dsp_core.DSPState();

  // must be outside the if.
  const u16 dest = state.FetchInstruction();
  if (CheckCondition(opc & 0xf))
  {
    state.StoreStack(StackRegister::Call, state.pc);
    state.pc = dest;
  }
}

// Generic callr implementation
// CALLRcc $R
// 0001 0111 rrr1 cccc
// Call function if condition cc has been met. Push program counter of
// instruction following "call" to call stack $st0. Set program counter to
// register $R.
void Interpreter::callr(const UDSPInstruction opc)
{
  if (!CheckCondition(opc & 0xf))
    return;

  auto& state = m_dsp_core.DSPState();
  const u8 reg = (opc >> 5) & 0x7;
  const u16 addr = OpReadRegister(reg);
  state.StoreStack(StackRegister::Call, state.pc);
  state.pc = addr;
}

// Generic if implementation
// IFcc
// 0000 0010 0111 cccc
// Execute following opcode if the condition has been met.
void Interpreter::ifcc(const UDSPInstruction opc)
{
  if (CheckCondition(opc & 0xf))
    return;

  // skip the next opcode - we have to lookup its size.
  m_dsp_core.DSPState().SkipInstruction();
}

// Generic jmp implementation
// Jcc addressA
// 0000 0010 1001 cccc
// aaaa aaaa aaaa aaaa
// Jump to addressA if condition cc has been met. Set program counter to
// address represented by value that follows this "jmp" instruction.
void Interpreter::jcc(const UDSPInstruction opc)
{
  auto& state = m_dsp_core.DSPState();
  const u16 dest = state.FetchInstruction();
  if (CheckCondition(opc & 0xf))
  {
    state.pc = dest;
  }
}

// Generic jmpr implementation
// JRcc $R
// 0001 0111 rrr0 cccc
// Jump to address if condition cc has been met. Set program counter to
// a value from register $R.
void Interpreter::jmprcc(const UDSPInstruction opc)
{
  if (!CheckCondition(opc & 0xf))
    return;

  auto& state = m_dsp_core.DSPState();
  const u8 reg = (opc >> 5) & 0x7;
  state.pc = OpReadRegister(reg);
}

// Generic ret implementation
// RETcc
// 0000 0010 1101 cccc
// Return from subroutine if condition cc has been met. Pops stored PC
// from call stack $st0 and sets $pc to this location.
void Interpreter::ret(const UDSPInstruction opc)
{
  if (!CheckCondition(opc & 0xf))
    return;

  auto& state = m_dsp_core.DSPState();
  state.pc = state.PopStack(StackRegister::Call);
}

// RTIcc
// 0000 0010 1111 1111
// Return from exception. Pops stored status register $sr from data stack
// $st1 and program counter PC from call stack $st0 and sets $pc to this
// location.
// This instruction has a conditional form, but it is not used by any official ucode.
void Interpreter::rti(const UDSPInstruction opc)
{
  if (!CheckCondition(opc & 0xf))
    return;

  auto& state = m_dsp_core.DSPState();
  state.r.sr = state.PopStack(StackRegister::Data);
  state.pc = state.PopStack(StackRegister::Call);
}

// HALT
// 0000 0000 0010 0001
// Stops execution of DSP code. Sets bit DSP_CR_HALT in register DREG_CR.
void Interpreter::halt(const UDSPInstruction)
{
  auto& state = m_dsp_core.DSPState();
  state.control_reg |= CR_HALT;
  state.pc--;
}

// LOOP handling: Loop stack is used to control execution of repeated blocks of
// instructions. Whenever there is value on stack $st2 and current PC is equal
// value at $st2, then value at stack $st3 is decremented. If value is not zero
// then PC is modified with value from call stack $st0. Otherwise values from
// call stack $st0 and both loop stacks $st2 and $st3 are popped and execution
// continues at next opcode.
void Interpreter::HandleLoop()
{
  auto& state = m_dsp_core.DSPState();

  // Handle looping hardware.
  const u16 rCallAddress = state.r.st[0];
  const u16 rLoopAddress = state.r.st[2];
  u16& rLoopCounter = state.r.st[3];

  if (rLoopAddress > 0 && rLoopCounter > 0)
  {
    // FIXME: why -1? because we just read past it.
    if (state.pc - 1 == rLoopAddress)
    {
      rLoopCounter--;
      if (rLoopCounter > 0)
      {
        state.pc = rCallAddress;
      }
      else
      {
        // end of loop
        state.PopStack(StackRegister::Call);
        state.PopStack(StackRegister::LoopAddress);
        state.PopStack(StackRegister::LoopCounter);
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
void Interpreter::loop(const UDSPInstruction opc)
{
  auto& state = m_dsp_core.DSPState();
  const u16 reg = opc & 0x1f;
  const u16 cnt = OpReadRegister(reg);
  const u16 loop_pc = state.pc;

  if (cnt != 0)
  {
    state.StoreStack(StackRegister::Call, state.pc);
    state.StoreStack(StackRegister::LoopAddress, loop_pc);
    state.StoreStack(StackRegister::LoopCounter, cnt);
  }
  else
  {
    state.SkipInstruction();
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
void Interpreter::loopi(const UDSPInstruction opc)
{
  auto& state = m_dsp_core.DSPState();
  const u16 cnt = opc & 0xff;
  const u16 loop_pc = state.pc;

  if (cnt != 0)
  {
    state.StoreStack(StackRegister::Call, state.pc);
    state.StoreStack(StackRegister::LoopAddress, loop_pc);
    state.StoreStack(StackRegister::LoopCounter, cnt);
  }
  else
  {
    state.SkipInstruction();
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
void Interpreter::bloop(const UDSPInstruction opc)
{
  auto& state = m_dsp_core.DSPState();
  const u16 reg = opc & 0x1f;
  const u16 cnt = OpReadRegister(reg);
  const u16 loop_pc = state.FetchInstruction();

  if (cnt != 0)
  {
    state.StoreStack(StackRegister::Call, state.pc);
    state.StoreStack(StackRegister::LoopAddress, loop_pc);
    state.StoreStack(StackRegister::LoopCounter, cnt);
  }
  else
  {
    state.pc = loop_pc;
    state.SkipInstruction();
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
void Interpreter::bloopi(const UDSPInstruction opc)
{
  auto& state = m_dsp_core.DSPState();
  const u16 cnt = opc & 0xff;
  const u16 loop_pc = state.FetchInstruction();

  if (cnt != 0)
  {
    state.StoreStack(StackRegister::Call, state.pc);
    state.StoreStack(StackRegister::LoopAddress, loop_pc);
    state.StoreStack(StackRegister::LoopCounter, cnt);
  }
  else
  {
    state.pc = loop_pc;
    state.SkipInstruction();
  }
}
}  // namespace DSP::Interpreter
