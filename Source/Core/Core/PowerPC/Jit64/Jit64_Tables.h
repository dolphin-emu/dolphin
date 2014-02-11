// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "../Gekko.h"
#include "../PPCTables.h"
#include "../Jit64/Jit.h"

namespace Jit64Tables
{
	void CompileInstruction(PPCAnalyst::CodeOp & op);
	void InitTables();
}
