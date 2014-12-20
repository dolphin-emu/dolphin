// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/ArmEmitter.h"
#include "Core/PowerPC/JitCommon/JitAsmCommon.h"

class JitArmAsmRoutineManager : public CommonAsmRoutinesBase, public ArmGen::ARMCodeBlock
{
private:
	void Generate();
	void GenerateCommon();

public:
	const u8* m_increment_profile_counter;

	void Init()
	{
		AllocCodeSpace(8192);
		Generate();
		WriteProtect();
	}

	void Shutdown()
	{
		FreeCodeSpace();
	}
};

extern JitArmAsmRoutineManager asm_routines;
