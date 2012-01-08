// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Common.h"
#include "x64Emitter.h"
#include "ABI.h"

using namespace Gen;

// Shared code between Win64 and Unix64

// Sets up a __cdecl function.
void XEmitter::ABI_EmitPrologue(int maxCallParams)
{
#ifdef _M_IX86
	// Don't really need to do anything
#elif defined(_M_X64)
#if _WIN32
	int stacksize = ((maxCallParams + 1) & ~1) * 8 + 8;
	// Set up a stack frame so that we can call functions
	// TODO: use maxCallParams
    SUB(64, R(RSP), Imm8(stacksize));
#endif
#else
#error Arch not supported
#endif
}

void XEmitter::ABI_EmitEpilogue(int maxCallParams)
{
#ifdef _M_IX86
	RET();
#elif defined(_M_X64)
#ifdef _WIN32
	int stacksize = ((maxCallParams+1)&~1)*8 + 8;
	ADD(64, R(RSP), Imm8(stacksize));
#endif
	RET();
#else
#error Arch not supported


#endif
}

#ifdef _M_IX86 // All32

// Shared code between Win32 and Unix32
void XEmitter::ABI_CallFunction(void *func) {
	ABI_AlignStack(0);
	CALL(func);
	ABI_RestoreStack(0);
}

void XEmitter::ABI_CallFunctionC16(void *func, u16 param1) {
	ABI_AlignStack(1 * 2);
	PUSH(16, Imm16(param1));
	CALL(func);
	ABI_RestoreStack(1 * 2);
}

void XEmitter::ABI_CallFunctionCC16(void *func, u32 param1, u16 param2) {
	ABI_AlignStack(1 * 2 + 1 * 4);
	PUSH(16, Imm16(param2));
	PUSH(32, Imm32(param1));
	CALL(func);
	ABI_RestoreStack(1 * 2 + 1 * 4);
}

void XEmitter::ABI_CallFunctionC(void *func, u32 param1) {
	ABI_AlignStack(1 * 4);
	PUSH(32, Imm32(param1));
	CALL(func);
	ABI_RestoreStack(1 * 4);
}

void XEmitter::ABI_CallFunctionCC(void *func, u32 param1, u32 param2) {
	ABI_AlignStack(2 * 4);
	PUSH(32, Imm32(param2));
	PUSH(32, Imm32(param1));
	CALL(func);
	ABI_RestoreStack(2 * 4);
}

void XEmitter::ABI_CallFunctionCCC(void *func, u32 param1, u32 param2, u32 param3) {
	ABI_AlignStack(3 * 4);
	PUSH(32, Imm32(param3));
	PUSH(32, Imm32(param2));
	PUSH(32, Imm32(param1));
	CALL(func);
	ABI_RestoreStack(3 * 4);
}

void XEmitter::ABI_CallFunctionCCP(void *func, u32 param1, u32 param2, void *param3) {
	ABI_AlignStack(3 * 4);
	PUSH(32, Imm32((u32)param3));
	PUSH(32, Imm32(param2));
	PUSH(32, Imm32(param1));
	CALL(func);
	ABI_RestoreStack(3 * 4);
}

void XEmitter::ABI_CallFunctionCCCP(void *func, u32 param1, u32 param2,u32 param3, void *param4) {
	ABI_AlignStack(4 * 4);
	PUSH(32, Imm32((u32)param4));
	PUSH(32, Imm32(param3));
	PUSH(32, Imm32(param2));
	PUSH(32, Imm32(param1));
	CALL(func);
	ABI_RestoreStack(4 * 4);
}

void XEmitter::ABI_CallFunctionPPC(void *func, void *param1, void *param2,u32 param3) {
	ABI_AlignStack(3 * 4);
	PUSH(32, Imm32(param3));
	PUSH(32, Imm32((u32)param2));
	PUSH(32, Imm32((u32)param1));
	CALL(func);
	ABI_RestoreStack(3 * 4);
}

// Pass a register as a parameter.
void XEmitter::ABI_CallFunctionR(void *func, X64Reg reg1) {
	ABI_AlignStack(1 * 4);
	PUSH(32, R(reg1));
	CALL(func);
	ABI_RestoreStack(1 * 4);
}

// Pass two registers as parameters.
void XEmitter::ABI_CallFunctionRR(void *func, Gen::X64Reg reg1, Gen::X64Reg reg2)
{
	ABI_AlignStack(2 * 4);
	PUSH(32, R(reg2));
	PUSH(32, R(reg1));
	CALL(func);
	ABI_RestoreStack(2 * 4);
}

void XEmitter::ABI_CallFunctionAC(void *func, const Gen::OpArg &arg1, u32 param2)
{
	ABI_AlignStack(2 * 4);
	PUSH(32, Imm32(param2));
	PUSH(32, arg1);
	CALL(func);
	ABI_RestoreStack(2 * 4);
}

void XEmitter::ABI_CallFunctionA(void *func, const Gen::OpArg &arg1)
{
	ABI_AlignStack(1 * 4);
	PUSH(32, arg1);
	CALL(func);
	ABI_RestoreStack(1 * 4);
}

void XEmitter::ABI_PushAllCalleeSavedRegsAndAdjustStack() {
	// Note: 4 * 4 = 16 bytes, so alignment is preserved.
	PUSH(EBP);
	PUSH(EBX);
	PUSH(ESI);
	PUSH(EDI);
}

void XEmitter::ABI_PopAllCalleeSavedRegsAndAdjustStack() {
	POP(EDI);
	POP(ESI);
	POP(EBX);
	POP(EBP);
}

unsigned int XEmitter::ABI_GetAlignedFrameSize(unsigned int frameSize) {
	frameSize += 4; // reserve space for return address
	unsigned int alignedSize =
#ifdef __GNUC__
		(frameSize + 15) & -16;
#else
		(frameSize + 3) & -4;
#endif
	return alignedSize;
}


void XEmitter::ABI_AlignStack(unsigned int frameSize) {
// Mac OS X requires the stack to be 16-byte aligned before every call.
// Linux requires the stack to be 16-byte aligned before calls that put SSE
// vectors on the stack, but since we do not keep track of which calls do that,
// it is effectively every call as well.
// Windows binaries compiled with MSVC do not have such a restriction*, but I
// expect that GCC on Windows acts the same as GCC on Linux in this respect.
// It would be nice if someone could verify this.
// *However, the MSVC optimizing compiler assumes a 4-byte-aligned stack at times.
	unsigned int fillSize =
		ABI_GetAlignedFrameSize(frameSize) - (frameSize + 4);
	if (fillSize != 0) {
		SUB(32, R(ESP), Imm8(fillSize));
	}
}

void XEmitter::ABI_RestoreStack(unsigned int frameSize) {
	unsigned int alignedSize = ABI_GetAlignedFrameSize(frameSize);
	alignedSize -= 4; // return address is POPped at end of call
	if (alignedSize != 0) {
		ADD(32, R(ESP), Imm8(alignedSize));
	}
}

#else //64bit

// Common functions
void XEmitter::ABI_CallFunction(void *func) {
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL
	 && distance <  0xFFFFFFFF80000000ULL) {
	    // Far call
	    MOV(64, R(RAX), Imm64((u64)func));
	    CALLptr(R(RAX));
	} else {
	    CALL(func);
	}
}

void XEmitter::ABI_CallFunctionC16(void *func, u16 param1) {
	MOV(32, R(ABI_PARAM1), Imm32((u32)param1));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL
	 && distance <  0xFFFFFFFF80000000ULL) {
	    // Far call
	    MOV(64, R(RAX), Imm64((u64)func));
	    CALLptr(R(RAX));
	} else {
	    CALL(func);
	}
}

void XEmitter::ABI_CallFunctionCC16(void *func, u32 param1, u16 param2) {
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	MOV(32, R(ABI_PARAM2), Imm32((u32)param2));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL
		&& distance <  0xFFFFFFFF80000000ULL) {
			// Far call
			MOV(64, R(RAX), Imm64((u64)func));
			CALLptr(R(RAX));
	} else {
		CALL(func);
	}
}

void XEmitter::ABI_CallFunctionC(void *func, u32 param1) {
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL
	 && distance <  0xFFFFFFFF80000000ULL) {
	    // Far call
	    MOV(64, R(RAX), Imm64((u64)func));
	    CALLptr(R(RAX));
	} else {
	    CALL(func);
	}
}

void XEmitter::ABI_CallFunctionCC(void *func, u32 param1, u32 param2) {
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	MOV(32, R(ABI_PARAM2), Imm32(param2));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL
	 && distance <  0xFFFFFFFF80000000ULL) {
	    // Far call
	    MOV(64, R(RAX), Imm64((u64)func));
	    CALLptr(R(RAX));
	} else {
	    CALL(func);
	}
}

void XEmitter::ABI_CallFunctionCCC(void *func, u32 param1, u32 param2, u32 param3) {
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	MOV(32, R(ABI_PARAM2), Imm32(param2));
	MOV(32, R(ABI_PARAM3), Imm32(param3));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL
	 && distance <  0xFFFFFFFF80000000ULL) {
	    // Far call
	    MOV(64, R(RAX), Imm64((u64)func));
	    CALLptr(R(RAX));
	} else {
	    CALL(func);
	}
}

void XEmitter::ABI_CallFunctionCCP(void *func, u32 param1, u32 param2, void *param3) {
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	MOV(32, R(ABI_PARAM2), Imm32(param2));
	MOV(64, R(ABI_PARAM3), Imm64((u64)param3));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL
	 && distance <  0xFFFFFFFF80000000ULL) {
	    // Far call
	    MOV(64, R(RAX), Imm64((u64)func));
	    CALLptr(R(RAX));
	} else {
	    CALL(func);
	}
}

void XEmitter::ABI_CallFunctionCCCP(void *func, u32 param1, u32 param2, u32 param3, void *param4) {
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	MOV(32, R(ABI_PARAM2), Imm32(param2));
	MOV(32, R(ABI_PARAM3), Imm32(param3));
	MOV(64, R(ABI_PARAM4), Imm64((u64)param4));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL
	 && distance <  0xFFFFFFFF80000000ULL) {
	    // Far call
	    MOV(64, R(RAX), Imm64((u64)func));
	    CALLptr(R(RAX));
	} else {
	    CALL(func);
	}
}

void XEmitter::ABI_CallFunctionPPC(void *func, void *param1, void *param2, u32 param3) {
	MOV(64, R(ABI_PARAM1), Imm64((u64)param1));
	MOV(64, R(ABI_PARAM2), Imm64((u64)param2));
	MOV(32, R(ABI_PARAM3), Imm32(param3));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL
	 && distance <  0xFFFFFFFF80000000ULL) {
	    // Far call
	    MOV(64, R(RAX), Imm64((u64)func));
	    CALLptr(R(RAX));
	} else {
	    CALL(func);
	}
}

// Pass a register as a parameter.
void XEmitter::ABI_CallFunctionR(void *func, X64Reg reg1) {
	if (reg1 != ABI_PARAM1)
		MOV(32, R(ABI_PARAM1), R(reg1));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL
	 && distance <  0xFFFFFFFF80000000ULL) {
	    // Far call
	    MOV(64, R(RAX), Imm64((u64)func));
	    CALLptr(R(RAX));
	} else {
	    CALL(func);
	}
}

// Pass two registers as parameters.
void XEmitter::ABI_CallFunctionRR(void *func, X64Reg reg1, X64Reg reg2) {
	if (reg2 != ABI_PARAM1) {
		if (reg1 != ABI_PARAM1)
			MOV(64, R(ABI_PARAM1), R(reg1));
		if (reg2 != ABI_PARAM2)
			MOV(64, R(ABI_PARAM2), R(reg2));
	} else {
		if (reg2 != ABI_PARAM2)
			MOV(64, R(ABI_PARAM2), R(reg2));
		if (reg1 != ABI_PARAM1)
			MOV(64, R(ABI_PARAM1), R(reg1));
	}
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL
	 && distance <  0xFFFFFFFF80000000ULL) {
	    // Far call
	    MOV(64, R(RAX), Imm64((u64)func));
	    CALLptr(R(RAX));
	} else {
	    CALL(func);
	}
}

void XEmitter::ABI_CallFunctionAC(void *func, const Gen::OpArg &arg1, u32 param2)
{
	if (!arg1.IsSimpleReg(ABI_PARAM1))
		MOV(32, R(ABI_PARAM1), arg1);
	MOV(32, R(ABI_PARAM2), Imm32(param2));
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL
	 && distance <  0xFFFFFFFF80000000ULL) {
	    // Far call
	    MOV(64, R(RAX), Imm64((u64)func));
	    CALLptr(R(RAX));
	} else {
	    CALL(func);
	}
}

void XEmitter::ABI_CallFunctionA(void *func, const Gen::OpArg &arg1)
{
	if (!arg1.IsSimpleReg(ABI_PARAM1))
		MOV(32, R(ABI_PARAM1), arg1);
	u64 distance = u64(func) - (u64(code) + 5);
	if (distance >= 0x0000000080000000ULL
	 && distance <  0xFFFFFFFF80000000ULL) {
	    // Far call
	    MOV(64, R(RAX), Imm64((u64)func));
	    CALLptr(R(RAX));
	} else {
	    CALL(func);
	}
}

unsigned int XEmitter::ABI_GetAlignedFrameSize(unsigned int frameSize) {
	return frameSize;
}

#ifdef _WIN32

// Win64 Specific Code
void XEmitter::ABI_PushAllCalleeSavedRegsAndAdjustStack() {
	//we only want to do this once
	PUSH(RBX); 
	PUSH(RSI); 
	PUSH(RDI);
	PUSH(RBP);
	PUSH(R12); 
	PUSH(R13); 
	PUSH(R14); 
	PUSH(R15);
	//TODO: Also preserve XMM0-3?
	ABI_AlignStack(0);
}

void XEmitter::ABI_PopAllCalleeSavedRegsAndAdjustStack() {
	ABI_RestoreStack(0);
	POP(R15);
	POP(R14); 
	POP(R13); 
	POP(R12);
	POP(RBP);
	POP(RDI);
	POP(RSI); 
	POP(RBX); 
}

// Win64 Specific Code
void XEmitter::ABI_PushAllCallerSavedRegsAndAdjustStack() {
	PUSH(RCX);
	PUSH(RDX);
	PUSH(RSI); 
	PUSH(RDI);
	PUSH(R8);
	PUSH(R9);
	PUSH(R10);
	PUSH(R11);
	//TODO: Also preserve XMM0-15?
	ABI_AlignStack(0);
}

void XEmitter::ABI_PopAllCallerSavedRegsAndAdjustStack() {
	ABI_RestoreStack(0);
	POP(R11);
	POP(R10);
	POP(R9);
	POP(R8);
	POP(RDI); 
	POP(RSI); 
	POP(RDX);
	POP(RCX);
}

void XEmitter::ABI_AlignStack(unsigned int /*frameSize*/) {
	SUB(64, R(RSP), Imm8(0x28));
}

void XEmitter::ABI_RestoreStack(unsigned int /*frameSize*/) {
	ADD(64, R(RSP), Imm8(0x28));
}

#else
// Unix64 Specific Code
void XEmitter::ABI_PushAllCalleeSavedRegsAndAdjustStack() {
	PUSH(RBX); 
	PUSH(RBP);
	PUSH(R12); 
	PUSH(R13); 
	PUSH(R14); 
	PUSH(R15);
	PUSH(R15); //just to align stack. duped push/pop doesn't hurt.
}

void XEmitter::ABI_PopAllCalleeSavedRegsAndAdjustStack() {
	POP(R15);
	POP(R15);
	POP(R14); 
	POP(R13); 
	POP(R12);
	POP(RBP);
	POP(RBX); 
}

void XEmitter::ABI_PushAllCallerSavedRegsAndAdjustStack() {
	PUSH(RCX);
	PUSH(RDX);
	PUSH(RSI); 
	PUSH(RDI);
	PUSH(R8);
	PUSH(R9);
	PUSH(R10);
	PUSH(R11);
	PUSH(R11);
}

void XEmitter::ABI_PopAllCallerSavedRegsAndAdjustStack() {
	POP(R11);
	POP(R11);
	POP(R10);
	POP(R9);
	POP(R8);
	POP(RDI); 
	POP(RSI); 
	POP(RDX);
	POP(RCX);
}

void XEmitter::ABI_AlignStack(unsigned int /*frameSize*/) {
	SUB(64, R(RSP), Imm8(0x08));
}

void XEmitter::ABI_RestoreStack(unsigned int /*frameSize*/) {
	ADD(64, R(RSP), Imm8(0x08));
}

#endif // WIN32

#endif // 32bit


