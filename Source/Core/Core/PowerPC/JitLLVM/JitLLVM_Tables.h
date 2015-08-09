// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/JitLLVM/Jit.h"

namespace JitLLVMTables
{
	void CompileInstruction(JitLLVM* _jit, LLVMFunction* func, PPCAnalyst::CodeOp & op);
	void InitTables();
}
