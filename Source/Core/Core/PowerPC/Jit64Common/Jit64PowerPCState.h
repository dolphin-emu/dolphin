// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <type_traits>

#include "Common/CommonTypes.h"
#include "Core/PowerPC/Jit64Common/Jit64Constants.h"
#include "Core/PowerPC/PowerPC.h"

// We offset by 0x80 because the range of one byte memory offsets is
// -0x80..0x7f.
#define PPCSTATE_OFF(i) (static_cast<int>(offsetof(PowerPC::PowerPCState, i)) - 0x80)
#define PPCSTATE_OFF_ARRAY(elem, i)                                                                \
  (static_cast<int>(offsetof(PowerPC::PowerPCState, elem[0]) +                                     \
                    sizeof(PowerPC::PowerPCState::elem[0]) * (i)) -                                \
   0x80)

#define PPCSTATE_OFF_GPR(i) PPCSTATE_OFF_ARRAY(gpr, i)
#define PPCSTATE_OFF_CR(i) PPCSTATE_OFF_ARRAY(cr.fields, i)
#define PPCSTATE_OFF_SR(i) PPCSTATE_OFF_ARRAY(sr, i)
#define PPCSTATE_OFF_SPR(i) PPCSTATE_OFF_ARRAY(spr, i)

static_assert(std::is_same_v<decltype(PowerPC::PowerPCState::ps[0]), PowerPC::PairedSingle&>);
#define PPCSTATE_OFF_PS0(i) (PPCSTATE_OFF_ARRAY(ps, i) + offsetof(PowerPC::PairedSingle, ps0))
#define PPCSTATE_OFF_PS1(i) (PPCSTATE_OFF_ARRAY(ps, i) + offsetof(PowerPC::PairedSingle, ps1))

#define PPCSTATE(i) MDisp(RPPCSTATE, PPCSTATE_OFF(i))
#define PPCSTATE_GPR(i) MDisp(RPPCSTATE, PPCSTATE_OFF_ARRAY(gpr, i))
#define PPCSTATE_CR(i) MDisp(RPPCSTATE, PPCSTATE_OFF_ARRAY(cr.fields, i))
#define PPCSTATE_SR(i) MDisp(RPPCSTATE, PPCSTATE_OFF_ARRAY(sr, i))
#define PPCSTATE_SPR(i) MDisp(RPPCSTATE, PPCSTATE_OFF_ARRAY(spr, i))
#define PPCSTATE_PS0(i) MDisp(RPPCSTATE, PPCSTATE_OFF_PS0(i))
#define PPCSTATE_PS1(i) MDisp(RPPCSTATE, PPCSTATE_OFF_PS1(i))

#define PPCSTATE_LR PPCSTATE_SPR(SPR_LR)
#define PPCSTATE_CTR PPCSTATE_SPR(SPR_CTR)
#define PPCSTATE_SRR0 PPCSTATE_SPR(SPR_SRR0)
#define PPCSTATE_SRR1 PPCSTATE_SPR(SPR_SRR1)
