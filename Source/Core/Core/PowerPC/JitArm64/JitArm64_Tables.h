// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

class JitArm64;

namespace PPCAnalyst
{
struct CodeOp;
}

namespace JitArm64Tables
{
void CompileInstruction(JitArm64& jit, PPCAnalyst::CodeOp& op);
void InitTables();
}
