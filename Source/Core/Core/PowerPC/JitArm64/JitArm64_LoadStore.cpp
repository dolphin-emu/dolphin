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

void JitArm64::lwz(UGeckoInstruction inst)
{
	Gen::OpArg opAddress;
	u32 tmp;
	if(!inst.RA) tmp = (u32)inst.SIMM_16;
	else tmp = gpr.R(inst.RA).offset + inst.SIMM_16;
	
	opAddress = Imm32(tmp);
	
	SafeLoadToReg(gpr.RX(inst.RD), opAddress, 32, 0, CallerSavedRegistersInUse(), false);
}

void JitArm64::lwzu(UGeckoInstruction inst)
{
	Gen::OpArg opAddress;
	u32 tmp = gpr.R(inst.RA).offset + inst.SIMM_16;
	opaddress = Imm32(tmp);
	
	SafeLoadToReg(gpr.RX(inst.RD), opAddress, 32, 0, CallerSavedRegistersInUse(), false);
	gpr.SetImmediate(inst.RA, tmp);
}

void JitArm64::lwzx(UGeckoInstruction inst)
{
	Gen::OpArg opAddress;
	u32 tmp;
	if(!inst.RA) tmp = gpr.R(inst.RB).offset;
	else tmp = gpr.R(inst.RA).offset + gpr.R(inst.RB).offset;
	
	opAddress = Imm32(tmp);
	
	SafeLoadToReg(gpr.RX(inst.RD), opAddress, 32, 0, CallerSavedRegistersInUse(), false);
}

void JitArm64::lwzux(UGeckoInstruction inst)
{
	Gen::OpArg opAddress;
	u32 tmp = gpr.R(inst.RA).offset + gpr.R(inst.RB).offset;
	opaddress = Imm32(tmp);
	
	SafeLoadToReg(gpr.RX(inst.RD), opAddress, 32, 0, CallerSavedRegistersInUse(), false);
	gpr.SetImmediate(inst.RA, tmp);
}