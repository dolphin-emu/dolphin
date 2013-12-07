// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef JITIL_TABLES_H
#define JITIL_TABLES_H

#include "../Gekko.h"
#include "../PPCTables.h"
#include "../Jit64IL/JitIL.h"

namespace JitILTables
{
	void CompileInstruction(PPCAnalyst::CodeOp & op);
	void InitTables();
}

#endif
