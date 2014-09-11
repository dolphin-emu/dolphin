// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Arm64Emitter.h"
#include "Common/Common.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/JitArm64/JitAsm.h"

using namespace Arm64Gen;

void JitArm64::icbi(UGeckoInstruction inst)
{
	FallBackToInterpreter(inst);
	WriteExit(js.compilerPC + 4);
}
