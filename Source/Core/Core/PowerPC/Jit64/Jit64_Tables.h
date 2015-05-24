// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/Jit64/Jit.h"

namespace Jit64Tables
{
	void CompileInstruction(PPCAnalyst::CodeOp & op);
	void InitTables();
}
