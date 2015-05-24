// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"

#include "Core/PowerPC/JitArm32/Jit.h"
#include "Core/PowerPC/JitArm32/JitAsm.h"
#include "Core/PowerPC/JitArm32/JitFPRCache.h"
#include "Core/PowerPC/JitArm32/JitRegCache.h"

static const double minmaxFloat[2] = {-(double)0x80000000, (double)0x7FFFFFFF};
static const double doublenum = 0xfff8000000000000ull;

// Exception masks
static ArmGen::Operand2 FRFIMask(5, 0x8); // 0x60000
static ArmGen::Operand2 FIMask(2, 8); // 0x20000
static ArmGen::Operand2 FRMask(4, 8); // 0x40000
static ArmGen::Operand2 FXMask(2, 1); // 0x80000000
static ArmGen::Operand2 VEMask(0x40, 0); // 0x40

static ArmGen::Operand2 XXException(2, 4); // 0x2000000
static ArmGen::Operand2 CVIException(1, 0xC); // 0x100
static ArmGen::Operand2 NANException(1, 4); // 0x1000000
static ArmGen::Operand2 VXVCException(8, 8); // 0x80000
static ArmGen::Operand2 ZXException(1, 3); // 0x4000000
static ArmGen::Operand2 VXSQRTException(2, 5); // 0x200

inline void JitArm::SetFPException(ArmGen::ARMReg Reg, u32 Exception)
{
	ArmGen::Operand2 *ExceptionMask;
	switch (Exception)
	{
		case FPSCR_VXCVI:
			ExceptionMask = &CVIException;
		break;
		case FPSCR_XX:
			ExceptionMask = &XXException;
		break;
		case FPSCR_VXSNAN:
			ExceptionMask = &NANException;
		break;
		case FPSCR_VXVC:
			ExceptionMask = &VXVCException;
		break;
		case FPSCR_ZX:
			ExceptionMask = &ZXException;
		break;
		case FPSCR_VXSQRT:
			ExceptionMask = &VXSQRTException;
		break;
		default:
			_assert_msg_(DYNA_REC, false, "Passed unsupported FPexception: 0x%08x", Exception);
			return;
		break;
	}
	ArmGen::ARMReg rB = gpr.GetReg();
	MOV(rB, Reg);
	ORR(Reg, Reg, *ExceptionMask);
	CMP(rB, Reg);
	SetCC(CC_NEQ);
	ORR(Reg, Reg, FXMask); // If exception is set, set exception bit
	SetCC();
	BIC(Reg, Reg, FRFIMask);
	gpr.Unlock(rB);
}

