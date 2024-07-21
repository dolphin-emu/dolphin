// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/PowerPC/CachedInterpreter/CachedInterpreter.h"

#include "Common/CommonTypes.h"
#include "Core/CoreTiming.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#include <memory>

namespace CachedOpCode
{
template <bool breakpoint, bool check_fpu, bool memcheck, bool check_program_exception,
          bool idle_loop, bool endblock>
class Function
{
private:
  constexpr static u32 EXCEPTION_MASK =
      (memcheck * EXCEPTION_DSI) | (check_program_exception * EXCEPTION_PROGRAM);

  struct OpData
  {
    const UGeckoInstruction inst;

    const u32 address;
    const u32 downcountAmount;

    const u32 blockStart;

    const u32 numLoadStoreInst;
    const u32 numFloatingPointInst;
  };

  const Interpreter::Instruction function;
  const std::unique_ptr<OpData> data;

public:
  Function(UGeckoInstruction inst_, u32 address_, u32 downcountAmount_, u32 blockStart_,
           u32 numLoadStoreInst_, u32 numFloatingPointInst_)
      : function(Interpreter::GetInterpreterOp(inst_)),
        data(std::make_unique<OpData>(OpData{inst_, address_, downcountAmount_, blockStart_,
                                             numLoadStoreInst_, numFloatingPointInst_}))
  {
    ASSERT(sizeof(Function) <= 16);
  }

  Function(const Function& other)
      : function(other.function), data(std::make_unique<OpData>(*other.data))
  {
  }

  Function& operator=(const Function& other)
  {
    function = other.function;
    *data = *other.data;
    return *this;
  }

  bool operator()(Interpreter& interpreter) const
  {
    if constexpr (breakpoint || check_fpu || memcheck || check_program_exception || endblock)
    {
      interpreter.m_ppc_state.pc = data->address;
      interpreter.m_ppc_state.npc = data->address + 4;
    }

    if constexpr (breakpoint)
    {
      interpreter.m_system.GetPowerPC().CheckBreakPoints();
      if (interpreter.m_system.GetCPU().GetState() != CPU::State::Running)
      {
        interpreter.m_ppc_state.downcount -= data->downcountAmount;
        return true;
      }
    }

    if constexpr (check_fpu)
    {
      if (!interpreter.m_ppc_state.msr.FP)
      {
        interpreter.m_ppc_state.Exceptions |= EXCEPTION_FPU_UNAVAILABLE;
        interpreter.m_system.GetPowerPC().CheckExceptions();
        interpreter.m_ppc_state.downcount -= data->downcountAmount;
        return true;
      }
    }

    function(interpreter, data->inst);

    if constexpr (memcheck || check_program_exception)
    {
      if (interpreter.m_ppc_state.Exceptions & EXCEPTION_MASK)
      {
        interpreter.m_system.GetPowerPC().CheckExceptions();
        interpreter.m_ppc_state.downcount -= data->downcountAmount;
        return true;
      }
    }

    if constexpr (idle_loop)
    {
      if (interpreter.m_ppc_state.npc == data->blockStart)
      {
        interpreter.m_system.GetCoreTiming().Idle();
      }
    }

    if constexpr (endblock)
    {
      interpreter.m_ppc_state.pc = interpreter.m_ppc_state.npc;
      interpreter.m_ppc_state.downcount -= data->downcountAmount;
      PowerPC::UpdatePerformanceMonitor(data->downcountAmount, data->numLoadStoreInst,
                                        data->numFloatingPointInst, interpreter.m_ppc_state);
    }

    return false;
  }
};

// This condition fits < 16 bytes, although it's not particularly common
template <>
class Function<false, false, false, false, true, false>
{
private:
  const Interpreter::Instruction function;
  const UGeckoInstruction inst;

  const u32 blockStart;

public:
  Function(UGeckoInstruction inst_, u32 address, u32 downcountAmount, u32 blockStart_,
           u32 numLoadStoreInst, u32 numFloatingPointInst)
      : function(Interpreter::GetInterpreterOp(inst_)), inst(inst_), blockStart(blockStart_)
  {
    ASSERT(sizeof(Function) <= 16);
  }

  bool operator()(Interpreter& interpreter) const
  {
    function(interpreter, inst);
    if (interpreter.m_ppc_state.npc == blockStart)
    {
      interpreter.m_system.GetCoreTiming().Idle();
    }
    return false;
  }
};

// This condition fits < 16 bytes, and is common enough to get its own specialization.
template <>
class Function<false, false, false, false, false, false>
{
private:
  const Interpreter::Instruction function;
  const UGeckoInstruction inst;

public:
  Function(UGeckoInstruction inst_, u32 address, u32 downcountAmount, u32 blockStart,
           u32 numLoadStoreInst, u32 numFloatingPointInst)
      : function(Interpreter::GetInterpreterOp(inst_)), inst(inst_)
  {
    ASSERT(sizeof(Function) <= 16);
  }

  bool operator()(Interpreter& interpreter) const
  {
    function(interpreter, inst);
    return false;
  }
};

template <bool breakpoint, bool check_fpu, bool memcheck, bool check_program_exception,
          bool idle_loop, bool endblock>
CachedInterpreter::Instruction t_CreateFunction6(UGeckoInstruction inst, u32 address,
                                                 u32 downcountAmount, u32 blockStart,
                                                 u32 numLoadStoreInst, u32 numFloatingPointInst)
{
  return Function<breakpoint, check_fpu, memcheck, check_program_exception, idle_loop, endblock>(
      inst, address, downcountAmount, blockStart, numLoadStoreInst, numFloatingPointInst);
}

template <bool breakpoint, bool check_fpu, bool memcheck, bool check_program_exception,
          bool idle_loop>
CachedInterpreter::Instruction t_CreateFunction5(bool endblock, UGeckoInstruction inst, u32 address,
                                                 u32 downcountAmount, u32 blockStart,
                                                 u32 numLoadStoreInst, u32 numFloatingPointInst)
{
  if (endblock)
  {
    return t_CreateFunction6<breakpoint, check_fpu, memcheck, check_program_exception, idle_loop,
                             true>(inst, address, downcountAmount, blockStart, numLoadStoreInst,
                                   numFloatingPointInst);
  }
  else
  {
    return t_CreateFunction6<breakpoint, check_fpu, memcheck, check_program_exception, idle_loop,
                             false>(inst, address, downcountAmount, blockStart, numLoadStoreInst,
                                    numFloatingPointInst);
  }
}

template <bool breakpoint, bool check_fpu, bool memcheck, bool check_program_exception>
CachedInterpreter::Instruction t_CreateFunction4(bool idle_loop, bool endblock,
                                                 UGeckoInstruction inst, u32 address,
                                                 u32 downcountAmount, u32 blockStart,
                                                 u32 numLoadStoreInst, u32 numFloatingPointInst)
{
  if (idle_loop)
  {
    return t_CreateFunction5<breakpoint, check_fpu, memcheck, check_program_exception, true>(
        endblock, inst, address, downcountAmount, blockStart, numLoadStoreInst,
        numFloatingPointInst);
  }
  else
  {
    return t_CreateFunction5<breakpoint, check_fpu, memcheck, check_program_exception, false>(
        endblock, inst, address, downcountAmount, blockStart, numLoadStoreInst,
        numFloatingPointInst);
  }
}

template <bool breakpoint, bool check_fpu, bool memcheck>
CachedInterpreter::Instruction t_CreateFunction3(bool check_program_exception, bool idle_loop,
                                                 bool endblock, UGeckoInstruction inst, u32 address,
                                                 u32 downcountAmount, u32 blockStart,
                                                 u32 numLoadStoreInst, u32 numFloatingPointInst)
{
  if (check_program_exception)
  {
    return t_CreateFunction4<breakpoint, check_fpu, memcheck, true>(
        idle_loop, endblock, inst, address, downcountAmount, blockStart, numLoadStoreInst,
        numFloatingPointInst);
  }
  else
  {
    return t_CreateFunction4<breakpoint, check_fpu, memcheck, false>(
        idle_loop, endblock, inst, address, downcountAmount, blockStart, numLoadStoreInst,
        numFloatingPointInst);
  }
}

template <bool breakpoint, bool check_fpu>
CachedInterpreter::Instruction
t_CreateFunction2(bool memcheck, bool check_program_exception, bool idle_loop, bool endblock,
                  UGeckoInstruction inst, u32 address, u32 downcountAmount, u32 blockStart,
                  u32 numLoadStoreInst, u32 numFloatingPointInst)
{
  if (memcheck)
  {
    return t_CreateFunction3<breakpoint, check_fpu, true>(
        check_program_exception, idle_loop, endblock, inst, address, downcountAmount, blockStart,
        numLoadStoreInst, numFloatingPointInst);
  }
  else
  {
    return t_CreateFunction3<breakpoint, check_fpu, false>(
        check_program_exception, idle_loop, endblock, inst, address, downcountAmount, blockStart,
        numLoadStoreInst, numFloatingPointInst);
  }
}

template <bool breakpoint>
CachedInterpreter::Instruction
t_CreateFunction1(bool check_fpu, bool memcheck, bool check_program_exception, bool idle_loop,
                  bool endblock, UGeckoInstruction inst, u32 address, u32 downcountAmount,
                  u32 blockStart, u32 numLoadStoreInst, u32 numFloatingPointInst)
{
  if (check_fpu)
  {
    return t_CreateFunction2<breakpoint, true>(memcheck, check_program_exception, idle_loop,
                                               endblock, inst, address, downcountAmount, blockStart,
                                               numLoadStoreInst, numFloatingPointInst);
  }
  else
  {
    return t_CreateFunction2<breakpoint, false>(memcheck, check_program_exception, idle_loop,
                                                endblock, inst, address, downcountAmount,
                                                blockStart, numLoadStoreInst, numFloatingPointInst);
  }
}

CachedInterpreter::Instruction CreateFunction(bool breakpoint, bool check_fpu, bool memcheck,
                                              bool check_program_exception, bool idle_loop,
                                              bool endblock, UGeckoInstruction inst, u32 address,
                                              u32 downcountAmount, u32 blockStart,
                                              u32 numLoadStoreInst, u32 numFloatingPointInst)
{
  if (breakpoint)
  {
    return t_CreateFunction1<true>(check_fpu, memcheck, check_program_exception, idle_loop,
                                   endblock, inst, address, downcountAmount, blockStart,
                                   numLoadStoreInst, numFloatingPointInst);
  }
  else
  {
    return t_CreateFunction1<false>(check_fpu, memcheck, check_program_exception, idle_loop,
                                    endblock, inst, address, downcountAmount, blockStart,
                                    numLoadStoreInst, numFloatingPointInst);
  }
}
}  // namespace CachedOpCode
