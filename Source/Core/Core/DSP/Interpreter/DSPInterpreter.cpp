// Copyright 2008 Dolphin Emulator Project
// Copyright 2004 Duddie & Tratax
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/DSP/Interpreter/DSPInterpreter.h"

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MemoryUtil.h"

#include "Core/CoreTiming.h"
#include "Core/DSP/DSPAnalyzer.h"
#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPHost.h"
#include "Core/DSP/DSPTables.h"
#include "Core/DSP/Interpreter/DSPIntCCUtil.h"
#include "Core/DSP/Interpreter/DSPIntTables.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/System.h"

namespace DSP::Interpreter
{
// Correctly handle instructions such as `INC'L $ac0 : $ac0.l, @$ar0` (encoded as 0x7660) where both
// the main opcode and the extension opcode modify the same register. See the "Extended opcodes"
// section in the manual for more details.  No official uCode writes to the same register twice like
// this, so we don't emulate it by default (and also don't support it in the recompiler).
//
// Dolphin only supports this behavior in the interpreter when PRECISE_BACKLOG is defined.
// In ExecuteInstruction, if an extended opcode is in use, the extended opcode's behavior is
// executed first, followed by the main opcode's behavior. The extended opcode does not directly
// write to registers, but instead records the writes into a backlog (WriteToBackLog). The main
// opcode calls ZeroWriteBackLog after it is done reading the register values; this directly
// writes zero to all registers that have pending writes in the backlog. The main opcode then is
// free to write directly to registers it changes. Afterwards, ApplyWriteBackLog bitwise-ors the
// value of the register and the value in the backlog; if the main opcode didn't write to the
// register then ZeroWriteBackLog means that the pending value is being or'd with zero, so it's
// used without changes. When PRECISE_BACKLOG is not defined, ZeroWriteBackLog does nothing and
// ApplyWriteBackLog overwrites the register value with the value from the backlog (so writes from
// extended opcodes "win" over the main opcode).
//#define PRECISE_BACKLOG

Interpreter::Interpreter(DSPCore& dsp) : m_dsp_core{dsp}
{
  InitInstructionTables();
}

Interpreter::~Interpreter() = default;

void Interpreter::ExecuteInstruction(const UDSPInstruction inst)
{
  const DSPOPCTemplate* opcode_template = GetOpTemplate(inst);

  if (opcode_template->extended)
  {
    (this->*GetExtOp(inst))(inst);
  }

  (this->*GetOp(inst))(inst);

  if (opcode_template->extended)
  {
    ApplyWriteBackLog();
  }
}

void Interpreter::Step()
{
  auto& state = m_dsp_core.DSPState();

  m_dsp_core.CheckExceptions();
  state.AdvanceStepCounter();

  const u16 opc = state.FetchInstruction();
  ExecuteInstruction(UDSPInstruction{opc});

  const auto pc = state.pc;
  if (state.GetAnalyzer().IsLoopEnd(static_cast<u16>(pc - 1)))
    HandleLoop();
}

// Used by thread mode.
int Interpreter::RunCyclesThread(int cycles)
{
  auto& state = m_dsp_core.DSPState();

  while (true)
  {
    if ((state.control_reg & CR_HALT) != 0)
      return 0;

    if (state.external_interrupt_waiting.exchange(false, std::memory_order_acquire))
    {
      m_dsp_core.CheckExternalInterrupt();
    }

    Step();
    cycles--;
    if (cycles <= 0)
      return 0;
  }
}

// This one has basic idle skipping, and checks breakpoints.
int Interpreter::RunCyclesDebug(int cycles)
{
  auto& state = m_dsp_core.DSPState();

  // First, let's run a few cycles with no idle skipping so that things can progress a bit.
  for (int i = 0; i < 8; i++)
  {
    if ((state.control_reg & CR_HALT) != 0)
      return 0;

    if (m_dsp_core.BreakPoints().IsAddressBreakPoint(state.pc))
    {
      m_dsp_core.SetState(State::Stepping);
      return cycles;
    }
    Step();
    cycles--;
    if (cycles <= 0)
      return 0;
  }

  while (true)
  {
    // Next, let's run a few cycles with idle skipping, so that we can skip
    // idle loops.
    for (int i = 0; i < 8; i++)
    {
      if ((state.control_reg & CR_HALT) != 0)
        return 0;

      if (m_dsp_core.BreakPoints().IsAddressBreakPoint(state.pc))
      {
        m_dsp_core.SetState(State::Stepping);
        return cycles;
      }

      if (state.GetAnalyzer().IsIdleSkip(state.pc))
        return 0;

      Step();
      cycles--;
      if (cycles <= 0)
        return 0;
    }

    // Now, lets run some more without idle skipping.
    for (int i = 0; i < 200; i++)
    {
      if (m_dsp_core.BreakPoints().IsAddressBreakPoint(state.pc))
      {
        m_dsp_core.SetState(State::Stepping);
        return cycles;
      }
      Step();
      cycles--;
      if (cycles <= 0)
        return 0;
      // We don't bother directly supporting pause - if the main emu pauses,
      // it just won't call this function anymore.
    }
  }
}

// Used by non-thread mode. Meant to be efficient.
int Interpreter::RunCycles(int cycles)
{
  auto& state = m_dsp_core.DSPState();

  // First, let's run a few cycles with no idle skipping so that things can
  // progress a bit.
  for (int i = 0; i < 8; i++)
  {
    if ((state.control_reg & CR_HALT) != 0)
      return 0;

    Step();
    cycles--;

    if (cycles <= 0)
      return 0;
  }

  while (true)
  {
    // Next, let's run a few cycles with idle skipping, so that we can skip
    // idle loops.
    for (int i = 0; i < 8; i++)
    {
      if ((state.control_reg & CR_HALT) != 0)
        return 0;

      if (state.GetAnalyzer().IsIdleSkip(state.pc))
        return 0;

      Step();
      cycles--;

      if (cycles <= 0)
        return 0;
    }

    // Now, lets run some more without idle skipping.
    for (int i = 0; i < 200; i++)
    {
      Step();
      cycles--;
      if (cycles <= 0)
        return 0;
      // We don't bother directly supporting pause - if the main emu pauses,
      // it just won't call this function anymore.
    }
  }
}

// NOTE: These have nothing to do with SDSP::r::cr!
void Interpreter::WriteControlRegister(u16 val)
{
  auto& state = m_dsp_core.DSPState();

  if ((state.control_reg & CR_HALT) != (val & CR_HALT))
  {
    // This bit is handled by Interpreter::RunCycles and DSPEmitter::CompileDispatcher
    INFO_LOG_FMT(DSPLLE, "DSP_CONTROL halt bit changed: {:04x} -> {:04x}, PC {:04x}",
                 state.control_reg, val, state.pc);
  }

  // The CR_EXTERNAL_INT bit is handled by DSPLLE::DSP_WriteControlRegister

  // reset
  if ((val & CR_RESET) != 0)
  {
    INFO_LOG_FMT(DSPLLE, "DSP_CONTROL RESET");
    m_dsp_core.Reset();
    val &= ~CR_RESET;
  }
  // init - unclear if writing CR_INIT_CODE does something. Clearing CR_INIT immediately sets
  // CR_INIT_CODE, which gets unset a bit later...
  if (((state.control_reg & CR_INIT) != 0) && ((val & CR_INIT) == 0))
  {
    INFO_LOG_FMT(DSPLLE, "DSP_CONTROL INIT");
    // Copy 1024(?) bytes of uCode from main memory 0x81000000 (or is it ARAM 00000000?)
    // to IMEM 0000 and jump to that code
    // TODO: Determine exactly how this initialization works
    state.pc = 0;

    Common::UnWriteProtectMemory(state.iram, DSP_IRAM_BYTE_SIZE, false);
    Host::DMAToDSP(state.iram, 0x81000000, 0x1000);
    Common::WriteProtectMemory(state.iram, DSP_IRAM_BYTE_SIZE, false);

    Host::CodeLoaded(m_dsp_core, 0x81000000, 0x1000);

    val &= ~CR_INIT;
    val |= CR_INIT_CODE;
    // Number obtained from real hardware on a Wii, but it's not perfectly consistent
    state.control_reg_init_code_clear_time =
        Core::System::GetInstance().GetSystemTimers().GetFakeTimeBase() + 130;
  }

  // update cr
  state.control_reg = val;
}

u16 Interpreter::ReadControlRegister()
{
  auto& state = m_dsp_core.DSPState();
  if ((state.control_reg & CR_INIT_CODE) != 0)
  {
    auto& system = Core::System::GetInstance();
    if (system.GetSystemTimers().GetFakeTimeBase() >= state.control_reg_init_code_clear_time)
      state.control_reg &= ~CR_INIT_CODE;
    else
      system.GetCoreTiming().ForceExceptionCheck(50);  // Keep checking
  }
  return state.control_reg;
}

void Interpreter::SetSRFlag(u16 flag)
{
  m_dsp_core.DSPState().SetSRFlag(flag);
}

bool Interpreter::IsSRFlagSet(u16 flag) const
{
  return m_dsp_core.DSPState().IsSRFlagSet(flag);
}

bool Interpreter::CheckCondition(u8 condition) const
{
  const auto IsCarry = [this] { return IsSRFlagSet(SR_CARRY); };
  const auto IsOverflow = [this] { return IsSRFlagSet(SR_OVERFLOW); };
  const auto IsOverS32 = [this] { return IsSRFlagSet(SR_OVER_S32); };
  const auto IsLess = [this] { return IsSRFlagSet(SR_OVERFLOW) != IsSRFlagSet(SR_SIGN); };
  const auto IsZero = [this] { return IsSRFlagSet(SR_ARITH_ZERO); };
  const auto IsLogicZero = [this] { return IsSRFlagSet(SR_LOGIC_ZERO); };
  const auto IsConditionB = [this] {
    return (!(IsSRFlagSet(SR_OVER_S32) || IsSRFlagSet(SR_TOP2BITS))) || IsSRFlagSet(SR_ARITH_ZERO);
  };

  switch (condition & 0xf)
  {
  case 0xf:  // Always true.
    return true;
  case 0x0:  // GE - Greater Equal
    return !IsLess();
  case 0x1:  // L - Less
    return IsLess();
  case 0x2:  // G - Greater
    return !IsLess() && !IsZero();
  case 0x3:  // LE - Less Equal
    return IsLess() || IsZero();
  case 0x4:  // NZ - Not Zero
    return !IsZero();
  case 0x5:  // Z - Zero
    return IsZero();
  case 0x6:  // NC - Not carry
    return !IsCarry();
  case 0x7:  // C - Carry
    return IsCarry();
  case 0x8:  // ? - Not over s32
    return !IsOverS32();
  case 0x9:  // ? - Over s32
    return IsOverS32();
  case 0xa:  // ?
    return !IsConditionB();
  case 0xb:  // ?
    return IsConditionB();
  case 0xc:  // LNZ  - Logic Not Zero
    return !IsLogicZero();
  case 0xd:  // LZ - Logic Zero
    return IsLogicZero();
  case 0xe:  // O - Overflow
    return IsOverflow();
  default:
    return true;
  }
}

u16 Interpreter::IncrementAddressRegister(u16 reg) const
{
  auto& state = m_dsp_core.DSPState();
  const u32 ar = state.r.ar[reg];
  const u32 wr = state.r.wr[reg];
  u32 nar = ar + 1;

  if ((nar ^ ar) > ((wr | 1) << 1))
    nar -= wr + 1;

  return static_cast<u16>(nar);
}

u16 Interpreter::DecrementAddressRegister(u16 reg) const
{
  const auto& state = m_dsp_core.DSPState();
  const u32 ar = state.r.ar[reg];
  const u32 wr = state.r.wr[reg];
  u32 nar = ar + wr;

  if (((nar ^ ar) & ((wr | 1) << 1)) > wr)
    nar -= wr + 1;

  return static_cast<u16>(nar);
}

u16 Interpreter::IncreaseAddressRegister(u16 reg, s16 ix_) const
{
  const auto& state = m_dsp_core.DSPState();
  const u32 ar = state.r.ar[reg];
  const u32 wr = state.r.wr[reg];
  const s32 ix = ix_;

  const u32 mx = (wr | 1) << 1;
  u32 nar = ar + ix;
  const u32 dar = (nar ^ ar ^ ix) & mx;

  if (ix >= 0)
  {
    if (dar > wr)  // Overflow
      nar -= wr + 1;
  }
  else
  {
    // Underflow or below min for mask
    if ((((nar + wr + 1) ^ nar) & dar) <= wr)
      nar += wr + 1;
  }

  return static_cast<u16>(nar);
}

u16 Interpreter::DecreaseAddressRegister(u16 reg, s16 ix_) const
{
  const auto& state = m_dsp_core.DSPState();
  const u32 ar = state.r.ar[reg];
  const u32 wr = state.r.wr[reg];
  const s32 ix = ix_;

  const u32 mx = (wr | 1) << 1;
  u32 nar = ar - ix;
  const u32 dar = (nar ^ ar ^ ~ix) & mx;

  // (ix < 0 && ix != -0x8000)
  if (static_cast<u32>(ix) > 0xFFFF8000)
  {
    if (dar > wr)  // overflow
      nar -= wr + 1;
  }
  else
  {
    // Underflow or below min for mask
    if ((((nar + wr + 1) ^ nar) & dar) <= wr)
      nar += wr + 1;
  }

  return static_cast<u16>(nar);
}

s32 Interpreter::GetLongACX(s32 reg) const
{
  const auto& state = m_dsp_core.DSPState();
  return static_cast<s32>((static_cast<u32>(state.r.ax[reg].h) << 16) | state.r.ax[reg].l);
}

s16 Interpreter::GetAXLow(s32 reg) const
{
  return static_cast<s16>(m_dsp_core.DSPState().r.ax[reg].l);
}

s16 Interpreter::GetAXHigh(s32 reg) const
{
  return static_cast<s16>(m_dsp_core.DSPState().r.ax[reg].h);
}

s64 Interpreter::GetLongAcc(s32 reg) const
{
  const auto& state = m_dsp_core.DSPState();
  return static_cast<s64>(state.r.ac[reg].val);
}

void Interpreter::SetLongAcc(s32 reg, s64 value)
{
  auto& state = m_dsp_core.DSPState();
  // 40-bit sign extension
  state.r.ac[reg].val = static_cast<u64>((value << (64 - 40)) >> (64 - 40));
}

s16 Interpreter::GetAccLow(s32 reg) const
{
  return static_cast<s16>(m_dsp_core.DSPState().r.ac[reg].l);
}

s16 Interpreter::GetAccMid(s32 reg) const
{
  return static_cast<s16>(m_dsp_core.DSPState().r.ac[reg].m);
}

s16 Interpreter::GetAccHigh(s32 reg) const
{
  return static_cast<s16>(m_dsp_core.DSPState().r.ac[reg].h);
}

s64 Interpreter::GetLongProduct() const
{
  const auto& state = m_dsp_core.DSPState();

  s64 val = static_cast<s8>(static_cast<u8>(state.r.prod.h));
  val <<= 32;

  s64 low_prod = state.r.prod.m;
  low_prod += state.r.prod.m2;
  low_prod <<= 16;
  low_prod |= state.r.prod.l;

  val += low_prod;

  return val;
}

s64 Interpreter::GetLongProductRounded() const
{
  const s64 prod = GetLongProduct();

  if ((prod & 0x10000) != 0)
    return (prod + 0x8000) & ~0xffff;
  else
    return (prod + 0x7fff) & ~0xffff;
}

void Interpreter::SetLongProduct(s64 value)
{
  // For accurate emulation, this is wrong - but the real prod registers behave
  // in completely bizarre ways. Not needed to emulate them correctly for game ucodes.
  m_dsp_core.DSPState().r.prod.val = static_cast<u64>(value & 0x000000FFFFFFFFFFULL);
}

s64 Interpreter::GetMultiplyProduct(u16 a, u16 b, u8 sign) const
{
  s64 prod;

  // Unsigned
  if (sign == 1 && IsSRFlagSet(SR_MUL_UNSIGNED))
    prod = static_cast<u32>(a * b);
  else if (sign == 2 && IsSRFlagSet(SR_MUL_UNSIGNED))  // mixed
    prod = a * static_cast<s16>(b);
  else  // Signed
    prod = static_cast<s16>(a) * static_cast<s16>(b);

  // Conditionally multiply by 2.
  if (!IsSRFlagSet(SR_MUL_MODIFY))
    prod <<= 1;

  return prod;
}

s64 Interpreter::Multiply(u16 a, u16 b, u8 sign) const
{
  return GetMultiplyProduct(a, b, sign);
}

s64 Interpreter::MultiplyAdd(u16 a, u16 b, u8 sign) const
{
  return GetLongProduct() + GetMultiplyProduct(a, b, sign);
}

s64 Interpreter::MultiplySub(u16 a, u16 b, u8 sign) const
{
  return GetLongProduct() - GetMultiplyProduct(a, b, sign);
}

s64 Interpreter::MultiplyMulX(u8 axh0, u8 axh1, u16 val1, u16 val2) const
{
  s64 result;

  if (axh0 == 0 && axh1 == 0)
    result = Multiply(val1, val2, 1);  // Unsigned support ON if both ax?.l regs are used
  else if (axh0 == 0 && axh1 == 1)
    result = Multiply(val1, val2, 2);  // Mixed support ON (u16)axl.0  * (s16)axh.1
  else if (axh0 == 1 && axh1 == 0)
    result = Multiply(val2, val1, 2);  // Mixed support ON (u16)axl.1  * (s16)axh.0
  else
    result = Multiply(val1, val2, 0);  // Unsigned support OFF if both ax?.h regs are used

  return result;
}

void Interpreter::UpdateSR16(s16 value, bool carry, bool overflow, bool over_s32)
{
  auto& state = m_dsp_core.DSPState();

  state.r.sr &= ~SR_CMP_MASK;

  // 0x01
  if (carry)
  {
    state.r.sr |= SR_CARRY;
  }

  // 0x02 and 0x80
  if (overflow)
  {
    state.r.sr |= SR_OVERFLOW;
    state.r.sr |= SR_OVERFLOW_STICKY;
  }

  // 0x04
  if (value == 0)
  {
    state.r.sr |= SR_ARITH_ZERO;
  }

  // 0x08
  if (value < 0)
  {
    state.r.sr |= SR_SIGN;
  }

  // 0x10
  if (over_s32)
  {
    state.r.sr |= SR_OVER_S32;
  }

  // 0x20 - Checks if top bits of m are equal
  if (((static_cast<u16>(value) >> 14) == 0) || ((static_cast<u16>(value) >> 14) == 3))
  {
    state.r.sr |= SR_TOP2BITS;
  }
}

[[maybe_unused]] static constexpr bool IsProperlySignExtended(u64 val)
{
  const u64 topbits = val & 0xffff'ff80'0000'0000ULL;
  return (topbits == 0) || (0xffff'ff80'0000'0000ULL == topbits);
}

void Interpreter::UpdateSR64(s64 value, bool carry, bool overflow)
{
  DEBUG_ASSERT(IsProperlySignExtended(value));

  auto& state = m_dsp_core.DSPState();

  state.r.sr &= ~SR_CMP_MASK;

  // 0x01
  if (carry)
  {
    state.r.sr |= SR_CARRY;
  }

  // 0x02 and 0x80
  if (overflow)
  {
    state.r.sr |= SR_OVERFLOW;
    state.r.sr |= SR_OVERFLOW_STICKY;
  }

  // 0x04
  if (value == 0)
  {
    state.r.sr |= SR_ARITH_ZERO;
  }

  // 0x08
  if (value < 0)
  {
    state.r.sr |= SR_SIGN;
  }

  // 0x10
  if (isOverS32(value))
  {
    state.r.sr |= SR_OVER_S32;
  }

  // 0x20 - Checks if top bits of m are equal
  if (((value & 0xc0000000) == 0) || ((value & 0xc0000000) == 0xc0000000))
  {
    state.r.sr |= SR_TOP2BITS;
  }
}

// Updates SR based on a 64-bit value computed by result = val1 + val2.
// Result is a separate parameter that is properly sign-extended, and as such may not equal the
// result of adding a and b in a 64-bit context.
void Interpreter::UpdateSR64Add(s64 val1, s64 val2, s64 result)
{
  DEBUG_ASSERT(((val1 + val2) & 0xff'ffff'ffffULL) == (result & 0xff'ffff'ffffULL));
  DEBUG_ASSERT(IsProperlySignExtended(val1));
  DEBUG_ASSERT(IsProperlySignExtended(val2));
  UpdateSR64(result, isCarryAdd(val1, result), isOverflow(val1, val2, result));
}

// Updates SR based on a 64-bit value computed by result = val1 - val2.
// Result is a separate parameter that is properly sign-extended, and as such may not equal the
// result of adding a and b in a 64-bit context.
void Interpreter::UpdateSR64Sub(s64 val1, s64 val2, s64 result)
{
  DEBUG_ASSERT(((val1 - val2) & 0xff'ffff'ffffULL) == (result & 0xff'ffff'ffffULL));
  DEBUG_ASSERT(IsProperlySignExtended(val1));
  DEBUG_ASSERT(IsProperlySignExtended(val2));
  UpdateSR64(result, isCarrySubtract(val1, result), isOverflow(val1, -val2, result));
}

void Interpreter::UpdateSRLogicZero(bool value)
{
  auto& state = m_dsp_core.DSPState();

  if (value)
    state.r.sr |= SR_LOGIC_ZERO;
  else
    state.r.sr &= ~SR_LOGIC_ZERO;
}

u16 Interpreter::OpReadRegister(int reg_)
{
  const int reg = reg_ & 0x1f;
  auto& state = m_dsp_core.DSPState();

  switch (reg)
  {
  case DSP_REG_ST0:
  case DSP_REG_ST1:
  case DSP_REG_ST2:
  case DSP_REG_ST3:
    return state.PopStack(static_cast<StackRegister>(reg - DSP_REG_ST0));
  case DSP_REG_AR0:
  case DSP_REG_AR1:
  case DSP_REG_AR2:
  case DSP_REG_AR3:
    return state.r.ar[reg - DSP_REG_AR0];
  case DSP_REG_IX0:
  case DSP_REG_IX1:
  case DSP_REG_IX2:
  case DSP_REG_IX3:
    return state.r.ix[reg - DSP_REG_IX0];
  case DSP_REG_WR0:
  case DSP_REG_WR1:
  case DSP_REG_WR2:
  case DSP_REG_WR3:
    return state.r.wr[reg - DSP_REG_WR0];
  case DSP_REG_ACH0:
  case DSP_REG_ACH1:
    return state.r.ac[reg - DSP_REG_ACH0].h;
  case DSP_REG_CR:
    return state.r.cr;
  case DSP_REG_SR:
    return state.r.sr;
  case DSP_REG_PRODL:
    return state.r.prod.l;
  case DSP_REG_PRODM:
    return state.r.prod.m;
  case DSP_REG_PRODH:
    return state.r.prod.h;
  case DSP_REG_PRODM2:
    return state.r.prod.m2;
  case DSP_REG_AXL0:
  case DSP_REG_AXL1:
    return state.r.ax[reg - DSP_REG_AXL0].l;
  case DSP_REG_AXH0:
  case DSP_REG_AXH1:
    return state.r.ax[reg - DSP_REG_AXH0].h;
  case DSP_REG_ACL0:
  case DSP_REG_ACL1:
    return state.r.ac[reg - DSP_REG_ACL0].l;
  case DSP_REG_ACM0:
  case DSP_REG_ACM1:
  {
    // Saturate reads from $ac0.m or $ac1.m if that mode is enabled.
    if (IsSRFlagSet(SR_40_MODE_BIT))
    {
      const s64 acc = GetLongAcc(reg - DSP_REG_ACM0);

      if (acc != static_cast<s32>(acc))
      {
        if (acc > 0)
          return 0x7fff;
        else
          return 0x8000;
      }

      return state.r.ac[reg - DSP_REG_ACM0].m;
    }

    return state.r.ac[reg - DSP_REG_ACM0].m;
  }
  default:
    ASSERT_MSG(DSPLLE, 0, "cannot happen");
    return 0;
  }
}

void Interpreter::OpWriteRegister(int reg_, u16 val)
{
  const int reg = reg_ & 0x1f;
  auto& state = m_dsp_core.DSPState();

  switch (reg)
  {
  // 8-bit sign extended registers.
  case DSP_REG_ACH0:
  case DSP_REG_ACH1:
    // Sign extend from the bottom 8 bits.
    state.r.ac[reg - DSP_REG_ACH0].h = static_cast<s8>(val);
    break;

  // Stack registers.
  case DSP_REG_ST0:
  case DSP_REG_ST1:
  case DSP_REG_ST2:
  case DSP_REG_ST3:
    state.StoreStack(static_cast<StackRegister>(reg - DSP_REG_ST0), val);
    break;
  case DSP_REG_AR0:
  case DSP_REG_AR1:
  case DSP_REG_AR2:
  case DSP_REG_AR3:
    state.r.ar[reg - DSP_REG_AR0] = val;
    break;
  case DSP_REG_IX0:
  case DSP_REG_IX1:
  case DSP_REG_IX2:
  case DSP_REG_IX3:
    state.r.ix[reg - DSP_REG_IX0] = val;
    break;
  case DSP_REG_WR0:
  case DSP_REG_WR1:
  case DSP_REG_WR2:
  case DSP_REG_WR3:
    state.r.wr[reg - DSP_REG_WR0] = val;
    break;
  case DSP_REG_CR:
    state.r.cr = val & 0x00ff;
    break;
  case DSP_REG_SR:
    state.r.sr = val & ~SR_100;
    break;
  case DSP_REG_PRODL:
    state.r.prod.l = val;
    break;
  case DSP_REG_PRODM:
    state.r.prod.m = val;
    break;
  case DSP_REG_PRODH:
    // Unlike ac0.h and ac1.h, prod.h is not sign-extended
    state.r.prod.h = val & 0x00ff;
    break;
  case DSP_REG_PRODM2:
    state.r.prod.m2 = val;
    break;
  case DSP_REG_AXL0:
  case DSP_REG_AXL1:
    state.r.ax[reg - DSP_REG_AXL0].l = val;
    break;
  case DSP_REG_AXH0:
  case DSP_REG_AXH1:
    state.r.ax[reg - DSP_REG_AXH0].h = val;
    break;
  case DSP_REG_ACL0:
  case DSP_REG_ACL1:
    state.r.ac[reg - DSP_REG_ACL0].l = val;
    break;
  case DSP_REG_ACM0:
  case DSP_REG_ACM1:
    state.r.ac[reg - DSP_REG_ACM0].m = val;
    break;
  }
}

void Interpreter::ConditionalExtendAccum(int reg)
{
  if (reg != DSP_REG_ACM0 && reg != DSP_REG_ACM1)
    return;

  if (!IsSRFlagSet(SR_40_MODE_BIT))
    return;

  // Sign extend into whole accum.
  auto& state = m_dsp_core.DSPState();
  const u16 val = state.r.ac[reg - DSP_REG_ACM0].m;
  state.r.ac[reg - DSP_REG_ACM0].h = (val & 0x8000) != 0 ? 0xFFFFFFFF : 0x0000;
  state.r.ac[reg - DSP_REG_ACM0].l = 0;
}

// The ext op are writing their output into the backlog which is
// being applied to the real registers after the main op was executed
void Interpreter::ApplyWriteBackLog()
{
  // Always make sure to have an extra entry at the end w/ -1 to avoid
  // infinite loops
  for (int i = 0; m_write_back_log_idx[i] != -1; i++)
  {
    u16 value = m_write_back_log[i];
#ifdef PRECISE_BACKLOG
    value |= OpReadRegister(m_write_back_log_idx[i]);
#endif
    OpWriteRegister(m_write_back_log_idx[i], value);

    // Clear back log
    m_write_back_log_idx[i] = -1;
  }
}

// The ext ops are calculated in parallel with the actual op. That means that
// both the main op and the ext op see the same register state as input. The
// output is simple as long as the main and ext ops don't change the same
// register. If they do the output is the bitwise OR of the result of both the
// main and ext ops.
void Interpreter::WriteToBackLog(int i, int idx, u16 value)
{
  m_write_back_log[i] = value;
  m_write_back_log_idx[i] = idx;
}

// This function is being called in the main op after all input regs were read
// and before it writes into any regs. This way we can always use bitwise or to
// apply the ext command output, because if the main op didn't change the value
// then 0 | ext output = ext output and if it did then bitwise or is still the
// right thing to do
// Only needed for cases when mainop and extended are modifying the same ACC
// Games are not doing that + in motorola (similar DSP) dox this is forbidden to do.
void Interpreter::ZeroWriteBackLog()
{
#ifdef PRECISE_BACKLOG
  // always make sure to have an extra entry at the end w/ -1 to avoid
  // infinite loops
  for (int i = 0; m_write_back_log_idx[i] != -1; i++)
  {
    OpWriteRegister(m_write_back_log_idx[i], 0);
  }
#endif
}

void Interpreter::ZeroWriteBackLogPreserveAcc([[maybe_unused]] u8 acc)
{
#ifdef PRECISE_BACKLOG
  for (int i = 0; m_write_back_log_idx[i] != -1; i++)
  {
    // acc0
    if (acc == 0 &&
        ((m_write_back_log_idx[i] == DSP_REG_ACL0) || (m_write_back_log_idx[i] == DSP_REG_ACM0) ||
         (m_write_back_log_idx[i] == DSP_REG_ACH0)))
    {
      continue;
    }

    // acc1
    if (acc == 1 &&
        ((m_write_back_log_idx[i] == DSP_REG_ACL1) || (m_write_back_log_idx[i] == DSP_REG_ACM1) ||
         (m_write_back_log_idx[i] == DSP_REG_ACH1)))
    {
      continue;
    }

    OpWriteRegister(m_write_back_log_idx[i], 0);
  }
#endif
}

void Interpreter::nop(const UDSPInstruction opc)
{
  // The real nop is 0. Anything else is bad.
  if (opc == 0)
    return;

  ERROR_LOG_FMT(DSPLLE, "LLE: Unrecognized opcode {:#06x}", opc);
}
}  // namespace DSP::Interpreter
