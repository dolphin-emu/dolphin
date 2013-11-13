// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _JITARMILASM_H
#define _JITARMILASM_H
#include "ArmEmitter.h"
#include "../JitCommon/JitAsmCommon.h"
using namespace ArmGen;
class JitArmILAsmRoutineManager : public CommonAsmRoutinesBase, public ARMXCodeBlock
{
private:
	void Generate();
	void GenerateCommon() {}

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

extern JitArmILAsmRoutineManager armil_asm_routines;

#endif


