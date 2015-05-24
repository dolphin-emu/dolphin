// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Arm64Emitter.h"
#include "Core/PowerPC/JitCommon/JitAsmCommon.h"

class JitArm64AsmRoutineManager : public CommonAsmRoutinesBase, public Arm64Gen::ARM64CodeBlock
{
private:
	void Generate();
	void GenerateCommon();

public:
	void Init()
	{
		AllocCodeSpace(16384);
		Generate();
		WriteProtect();
	}

	void Shutdown()
	{
		FreeCodeSpace();
	}
};

