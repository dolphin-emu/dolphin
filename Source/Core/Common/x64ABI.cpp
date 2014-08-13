// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Common/x64ABI.h"
#include "Common/x64Emitter.h"

using namespace Gen;

// Shared code between Win64 and Unix64

unsigned int XEmitter::ABI_GetAlignedFrameSize(unsigned int frameSize, bool noProlog) {
	frameSize = noProlog ? 0x28 : 0;
	return frameSize;
}

void XEmitter::ABI_AlignStack(unsigned int frameSize, bool noProlog) {
	unsigned int fillSize =
		ABI_GetAlignedFrameSize(frameSize, noProlog) - frameSize;
	if (fillSize != 0) {
		SUB(64, R(RSP), Imm8(fillSize));
	}
}

void XEmitter::ABI_RestoreStack(unsigned int frameSize, bool noProlog) {
	unsigned int alignedSize = ABI_GetAlignedFrameSize(frameSize, noProlog);
	if (alignedSize != 0) {
		ADD(64, R(RSP), Imm8(alignedSize));
	}
}

void XEmitter::ABI_PushRegistersAndAdjustStack(u32 mask, bool noProlog)
{
	int regSize = 8;
	int shadow = 0;
#if defined(_WIN32)
	shadow = 0x20;
#endif
	int count = 0;
	for (int r = 0; r < 16; r++)
	{
		if (mask & (1 << r))
		{
			PUSH((X64Reg) r);
			count++;
		}
	}
	int size = ((noProlog ? -regSize : 0) - (count * regSize)) & 0xf;
	for (int x = 0; x < 16; x++)
	{
		if (mask & (1 << (16 + x)))
			size += 16;
	}
	size += shadow;
	if (size)
		SUB(regSize * 8, R(RSP), size >= 0x80 ? Imm32(size) : Imm8(size));
	int offset = shadow;
	for (int x = 0; x < 16; x++)
	{
		if (mask & (1 << (16 + x)))
		{
			MOVUPD(MDisp(RSP, offset), (X64Reg) x);
			offset += 16;
		}
	}
}

void XEmitter::ABI_PopRegistersAndAdjustStack(u32 mask, bool noProlog)
{
	int regSize = 8;
	int size = 0;
#if defined(_WIN32)
	size += 0x20;
#endif
	for (int x = 0; x < 16; x++)
	{
		if (mask & (1 << (16 + x)))
		{
			MOVUPD((X64Reg) x, MDisp(RSP, size));
			size += 16;
		}
	}
	int count = 0;
	for (int r = 0; r < 16; r++)
	{
		if (mask & (1 << r))
			count++;
	}
	size += ((noProlog ? -regSize : 0) - (count * regSize)) & 0xf;

	if (size)
		ADD(regSize * 8, R(RSP), size >= 0x80 ? Imm32(size) : Imm8(size));
	for (int r = 15; r >= 0; r--)
	{
		if (mask & (1 << r))
		{
			POP((X64Reg) r);
		}
	}
}

// Common functions
void XEmitter::ABI_CallFunction(void *func) {
	ABI_AlignStack(0);
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL &&
	    distance <  0xFFFFFFFF80000000ULL)
	{
		// Far call
		MOV(64, R(RAX), Imm64((u64)func));
		CALLptr(R(RAX));
	}
	else
	{
		CALL(func);
	}
	ABI_RestoreStack(0);
}

void XEmitter::ABI_CallFunctionC16(void *func, u16 param1) {
	ABI_AlignStack(0);
	MOV(32, R(ABI_PARAM1), Imm32((u32)param1));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL &&
	    distance <  0xFFFFFFFF80000000ULL)
	{
		// Far call
		MOV(64, R(RAX), Imm64((u64)func));
		CALLptr(R(RAX));
	}
	else
	{
		CALL(func);
	}
	ABI_RestoreStack(0);
}

void XEmitter::ABI_CallFunctionCC16(void *func, u32 param1, u16 param2) {
	ABI_AlignStack(0);
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	MOV(32, R(ABI_PARAM2), Imm32((u32)param2));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL &&
	    distance <  0xFFFFFFFF80000000ULL)
	{
		// Far call
		MOV(64, R(RAX), Imm64((u64)func));
		CALLptr(R(RAX));
	}
	else
	{
		CALL(func);
	}
	ABI_RestoreStack(0);
}

void XEmitter::ABI_CallFunctionC(void *func, u32 param1) {
	ABI_AlignStack(0);
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL &&
	    distance <  0xFFFFFFFF80000000ULL)
	{
		// Far call
		MOV(64, R(RAX), Imm64((u64)func));
		CALLptr(R(RAX));
	}
	else
	{
		CALL(func);
	}
	ABI_RestoreStack(0);
}

void XEmitter::ABI_CallFunctionCC(void *func, u32 param1, u32 param2) {
	ABI_AlignStack(0);
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	MOV(32, R(ABI_PARAM2), Imm32(param2));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL &&
	    distance <  0xFFFFFFFF80000000ULL)
	{
		// Far call
		MOV(64, R(RAX), Imm64((u64)func));
		CALLptr(R(RAX));
	}
	else
	{
		CALL(func);
	}
	ABI_RestoreStack(0);
}

void XEmitter::ABI_CallFunctionCP(void *func, u32 param1, void *param2) {
	ABI_AlignStack(0);
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	MOV(64, R(ABI_PARAM2), Imm64((u64)param2));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL &&
	    distance <  0xFFFFFFFF80000000ULL)
	{
		// Far call
		MOV(64, R(RAX), Imm64((u64)func));
		CALLptr(R(RAX));
	}
	else
	{
		CALL(func);
	}
	ABI_RestoreStack(0);
}

void XEmitter::ABI_CallFunctionCCC(void *func, u32 param1, u32 param2, u32 param3) {
	ABI_AlignStack(0);
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	MOV(32, R(ABI_PARAM2), Imm32(param2));
	MOV(32, R(ABI_PARAM3), Imm32(param3));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL &&
	    distance <  0xFFFFFFFF80000000ULL)
	{
		// Far call
		MOV(64, R(RAX), Imm64((u64)func));
		CALLptr(R(RAX));
	}
	else
	{
		CALL(func);
	}
	ABI_RestoreStack(0);
}

void XEmitter::ABI_CallFunctionCCP(void *func, u32 param1, u32 param2, void *param3) {
	ABI_AlignStack(0);
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	MOV(32, R(ABI_PARAM2), Imm32(param2));
	MOV(64, R(ABI_PARAM3), Imm64((u64)param3));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL &&
	    distance <  0xFFFFFFFF80000000ULL)
	{
		// Far call
		MOV(64, R(RAX), Imm64((u64)func));
		CALLptr(R(RAX));
	}
	else
	{
		CALL(func);
	}
	ABI_RestoreStack(0);
}

void XEmitter::ABI_CallFunctionCCCP(void *func, u32 param1, u32 param2, u32 param3, void *param4) {
	ABI_AlignStack(0);
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	MOV(32, R(ABI_PARAM2), Imm32(param2));
	MOV(32, R(ABI_PARAM3), Imm32(param3));
	MOV(64, R(ABI_PARAM4), Imm64((u64)param4));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL &&
	    distance <  0xFFFFFFFF80000000ULL)
	{
		// Far call
		MOV(64, R(RAX), Imm64((u64)func));
		CALLptr(R(RAX));
	}
	else
	{
		CALL(func);
	}
	ABI_RestoreStack(0);
}

void XEmitter::ABI_CallFunctionPC(void *func, void *param1, u32 param2) {
	ABI_AlignStack(0);
	MOV(64, R(ABI_PARAM1), Imm64((u64)param1));
	MOV(32, R(ABI_PARAM2), Imm32(param2));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL &&
	    distance <  0xFFFFFFFF80000000ULL)
	{
		// Far call
		MOV(64, R(RAX), Imm64((u64)func));
		CALLptr(R(RAX));
	}
	else
	{
		CALL(func);
	}
	ABI_RestoreStack(0);
}

void XEmitter::ABI_CallFunctionPPC(void *func, void *param1, void *param2, u32 param3) {
	ABI_AlignStack(0);
	MOV(64, R(ABI_PARAM1), Imm64((u64)param1));
	MOV(64, R(ABI_PARAM2), Imm64((u64)param2));
	MOV(32, R(ABI_PARAM3), Imm32(param3));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL &&
	    distance <  0xFFFFFFFF80000000ULL)
	{
		// Far call
		MOV(64, R(RAX), Imm64((u64)func));
		CALLptr(R(RAX));
	}
	else
	{
		CALL(func);
	}
	ABI_RestoreStack(0);
}

// Pass a register as a parameter.
void XEmitter::ABI_CallFunctionR(void *func, X64Reg reg1) {
	ABI_AlignStack(0);
	if (reg1 != ABI_PARAM1)
		MOV(32, R(ABI_PARAM1), R(reg1));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL &&
	    distance <  0xFFFFFFFF80000000ULL)
	{
		// Far call
		MOV(64, R(RAX), Imm64((u64)func));
		CALLptr(R(RAX));
	}
	else
	{
		CALL(func);
	}
	ABI_RestoreStack(0);
}

// Pass two registers as parameters.
void XEmitter::ABI_CallFunctionRR(void *func, X64Reg reg1, X64Reg reg2, bool noProlog) {
	ABI_AlignStack(0, noProlog);
	if (reg2 != ABI_PARAM1)
	{
		if (reg1 != ABI_PARAM1)
			MOV(64, R(ABI_PARAM1), R(reg1));
		if (reg2 != ABI_PARAM2)
			MOV(64, R(ABI_PARAM2), R(reg2));
	}
	else
	{
		if (reg2 != ABI_PARAM2)
			MOV(64, R(ABI_PARAM2), R(reg2));
		if (reg1 != ABI_PARAM1)
			MOV(64, R(ABI_PARAM1), R(reg1));
	}
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL &&
	    distance <  0xFFFFFFFF80000000ULL)
	{
		// Far call
		MOV(64, R(RAX), Imm64((u64)func));
		CALLptr(R(RAX));
	}
	else
	{
		CALL(func);
	}
	ABI_RestoreStack(0, noProlog);
}

void XEmitter::ABI_CallFunctionAC(void *func, const Gen::OpArg &arg1, u32 param2)
{
	ABI_AlignStack(0);
	if (!arg1.IsSimpleReg(ABI_PARAM1))
		MOV(32, R(ABI_PARAM1), arg1);
	MOV(32, R(ABI_PARAM2), Imm32(param2));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL &&
	    distance <  0xFFFFFFFF80000000ULL)
	{
		// Far call
		MOV(64, R(RAX), Imm64((u64)func));
		CALLptr(R(RAX));
	}
	else
	{
		CALL(func);
	}
	ABI_RestoreStack(0);
}

void XEmitter::ABI_CallFunctionA(void *func, const Gen::OpArg &arg1)
{
	ABI_AlignStack(0);
	if (!arg1.IsSimpleReg(ABI_PARAM1))
		MOV(32, R(ABI_PARAM1), arg1);
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL &&
	    distance <  0xFFFFFFFF80000000ULL)
	{
		// Far call
		MOV(64, R(RAX), Imm64((u64)func));
		CALLptr(R(RAX));
	}
	else
	{
		CALL(func);
	}
	ABI_RestoreStack(0);
}

#ifdef _WIN32
// Win64 Specific Code

void XEmitter::ABI_PushAllCalleeSavedRegsAndAdjustStack() {
	//we only want to do this once
	PUSH(RBP);
	MOV(64, R(RBP), R(RSP));
	PUSH(RBX);
	PUSH(RSI);
	PUSH(RDI);
	PUSH(R12);
	PUSH(R13);
	PUSH(R14);
	PUSH(R15);
	SUB(64, R(RSP), Imm8(0x28));
	//TODO: Also preserve XMM0-3?
}

void XEmitter::ABI_PopAllCalleeSavedRegsAndAdjustStack() {
	ADD(64, R(RSP), Imm8(0x28));
	POP(R15);
	POP(R14);
	POP(R13);
	POP(R12);
	POP(RDI);
	POP(RSI);
	POP(RBX);
	POP(RBP);
}

#else
// Unix64 Specific Code

void XEmitter::ABI_PushAllCalleeSavedRegsAndAdjustStack() {
	PUSH(RBP);
	MOV(64, R(RBP), R(RSP));
	PUSH(RBX);
	PUSH(R12);
	PUSH(R13);
	PUSH(R14);
	PUSH(R15);
	SUB(64, R(RSP), Imm8(8));
}

void XEmitter::ABI_PopAllCalleeSavedRegsAndAdjustStack() {
	ADD(64, R(RSP), Imm8(8));
	POP(R15);
	POP(R14);
	POP(R13);
	POP(R12);
	POP(RBX);
	POP(RBP);
}

#endif // WIN32

