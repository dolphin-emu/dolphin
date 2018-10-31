// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
