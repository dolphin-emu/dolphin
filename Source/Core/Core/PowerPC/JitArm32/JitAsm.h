// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/ArmEmitter.h"
#include "Core/PowerPC/JitCommon/JitAsmCommon.h"

using namespace ArmGen;
class JitArmAsmRoutineManager : public CommonAsmRoutinesBase, public ARMXCodeBlock
{
private:
	void Generate();
	void GenerateCommon();

public:
	void Init() {
		AllocCodeSpace(8192);
		Generate();
		WriteProtect();
	}

	void Shutdown() {
		FreeCodeSpace();
	}
};

extern JitArmAsmRoutineManager asm_routines;
