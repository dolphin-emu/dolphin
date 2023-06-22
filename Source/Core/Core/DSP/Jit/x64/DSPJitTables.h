// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/DSP/DSPCommon.h"

namespace DSP::JIT::x64
{
class DSPEmitter;

using JITFunction = void (DSPEmitter::*)(UDSPInstruction);

JITFunction GetOp(UDSPInstruction inst);
JITFunction GetExtOp(UDSPInstruction inst);
void InitInstructionTables();
}  // namespace DSP::JIT::x64
