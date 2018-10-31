// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/DSP/DSPCommon.h"

namespace DSP::Interpreter
{
using InterpreterFunction = void (*)(UDSPInstruction);

InterpreterFunction GetOp(UDSPInstruction inst);
InterpreterFunction GetExtOp(UDSPInstruction inst);
void InitInstructionTables();
}  // namespace DSP::Interpreter
