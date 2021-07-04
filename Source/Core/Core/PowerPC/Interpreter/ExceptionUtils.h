// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PowerPC.h"

inline void GenerateAlignmentException(u32 effective_address, UGeckoInstruction inst)
{
  PowerPC::ppcState.Exceptions |= EXCEPTION_ALIGNMENT;
  PowerPC::ppcState.spr[SPR_DAR] = effective_address;

  // It has not been hardware tested what gets used instead of RD and RA in
  // the cases documented as undefined. For now, simply use RD and RA
  const bool x = inst.OPCD >= 32;
  const u32 op = x ? inst.SUBOP10 : (inst.OPCD >> 1);
  const u32 dsisr = ((op >> 8) << 15) | ((op & 0b11111) << 10) | (inst.RD << 5) | (inst.RA);
  PowerPC::ppcState.spr[SPR_DSISR] = dsisr;
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
