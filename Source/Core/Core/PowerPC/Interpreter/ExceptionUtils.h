// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PowerPC.h"

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

inline void GenerateProgramException()
{
  PowerPC::ppcState.Exceptions |= EXCEPTION_PROGRAM;
}
