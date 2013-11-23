// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef JITARMIL_TABLES_H
#define JITARMIL_TABLES_H

#include "../Gekko.h"
#include "../PPCTables.h"

namespace JitArmILTables
{
	void CompileInstruction(PPCAnalyst::CodeOp & op);
	void InitTables();
}
#endif
