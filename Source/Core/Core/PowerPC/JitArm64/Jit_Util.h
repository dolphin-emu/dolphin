// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Arm64Emitter.h"
#include "Common/Common.h"

#include "Core/HW/MMIO.h"

namespace Core
{
class System;
}

void SwapPairs(Arm64Gen::ARM64XEmitter* emit, Arm64Gen::ARM64Reg dst_reg,
               Arm64Gen::ARM64Reg src_reg, u32 flags);

void ByteswapAfterLoad(Arm64Gen::ARM64XEmitter* emit, Arm64Gen::ARM64FloatEmitter* float_emit,
                       Arm64Gen::ARM64Reg dst_reg, Arm64Gen::ARM64Reg src_reg, u32 flags,
                       bool is_reversed, bool is_extended);

Arm64Gen::ARM64Reg ByteswapBeforeStore(Arm64Gen::ARM64XEmitter* emit,
                                       Arm64Gen::ARM64FloatEmitter* float_emit,
                                       Arm64Gen::ARM64Reg tmp_reg, Arm64Gen::ARM64Reg src_reg,
                                       u32 flags, bool want_reversed);

void MMIOLoadToReg(Core::System& system, MMIO::Mapping* mmio, Arm64Gen::ARM64XEmitter* emit,
                   Arm64Gen::ARM64FloatEmitter* float_emit, BitSet32 gprs_in_use,
                   BitSet32 fprs_in_use, Arm64Gen::ARM64Reg dst_reg, u32 address, u32 flags);

void MMIOWriteRegToAddr(Core::System& system, MMIO::Mapping* mmio, Arm64Gen::ARM64XEmitter* emit,
                        Arm64Gen::ARM64FloatEmitter* float_emit, BitSet32 gprs_in_use,
                        BitSet32 fprs_in_use, Arm64Gen::ARM64Reg src_reg, u32 address, u32 flags);
