// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace PPCAnalyst { struct CodeOp; }

namespace Jit64Tables
{
	void CompileInstruction(PPCAnalyst::CodeOp& op);
	void InitTables();
}
