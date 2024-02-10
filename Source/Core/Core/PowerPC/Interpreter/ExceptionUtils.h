// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PowerPC.h"

enum class ProgramExceptionCause : u32
{
  FloatingPoint = 1 << (31 - 11),
  IllegalInstruction = 1 << (31 - 12),
  PrivilegedInstruction = 1 << (31 - 13),
  Trap = 1 << (31 - 14),
};

inline void GenerateAlignmentException(PowerPC::PowerPCState& ppc_state, u32 address)
{
  ppc_state.Exceptions |= EXCEPTION_ALIGNMENT;
  ppc_state.spr[SPR_DAR] = address;
}

inline void GenerateDSIException(PowerPC::PowerPCState& ppc_state, u32 address)
{
  ppc_state.Exceptions |= EXCEPTION_DSI;
  ppc_state.spr[SPR_DAR] = address;
}

inline void GenerateProgramException(PowerPC::PowerPCState& ppc_state, ProgramExceptionCause cause)
{
  ppc_state.Exceptions |= EXCEPTION_PROGRAM;
  ppc_state.spr[SPR_SRR1] = static_cast<u32>(cause);
}
