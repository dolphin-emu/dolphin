// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

class Jit64;

namespace PPCAnalyst
{
struct CodeOp;
}

namespace Jit64Tables
{
void CompileInstruction(Jit64& jit, PPCAnalyst::CodeOp& op);
void InitTables();
}
