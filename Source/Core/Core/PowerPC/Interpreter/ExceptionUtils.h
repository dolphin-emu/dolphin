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

inline void GenerateAlignmentException(u32 address)
{
  PowerPC::ppcState.Exceptions |= EXCEPTION_ALIGNMENT;
  PowerPC::ppcState.spr[SPR_DAR] = address;
}

inline void GenerateDSIException(u32 address)
{
  PowerPC::ppcState.Exceptions |= EXCEPTION_DSI;
  PowerPC::ppcState.spr[SPR_DAR] = address;
}

inline void GenerateProgramException(ProgramExceptionCause cause)
{
  PowerPC::ppcState.Exceptions |= EXCEPTION_PROGRAM;
  PowerPC::ppcState.spr[SPR_SRR1] = static_cast<u32>(cause);
}
