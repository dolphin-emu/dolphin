// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Common/x64ABI.h"
#include "Common/x64Emitter.h"

using namespace Gen;

// Shared code between Win64 and Unix64

unsigned int XEmitter::ABI_GetAlignedFrameSize(unsigned int frameSize, bool noProlog)
{
	frameSize = noProlog ? 0x28 : 0;
	return frameSize;
}

void XEmitter::ABI_AlignStack(unsigned int frameSize, bool noProlog)
{
	unsigned int fillSize = ABI_GetAlignedFrameSize(frameSize, noProlog) - frameSize;

	if (fillSize != 0)
	{
		SUB(64, R(RSP), Imm8(fillSize));
	}
}

void XEmitter::ABI_RestoreStack(unsigned int frameSize, bool noProlog)
{
	unsigned int alignedSize = ABI_GetAlignedFrameSize(frameSize, noProlog);

	if (alignedSize != 0)
	{
		ADD(64, R(RSP), Imm8(alignedSize));
	}
}

void XEmitter::ABI_CalculateFrameSize(u32 mask, size_t rsp_alignment, size_t needed_frame_size, size_t* shadowp, size_t* subtractionp, size_t* xmm_offsetp)
{
	size_t shadow = 0;
#if defined(_WIN32)
	shadow = 0x20;
#endif

	int count = 0;
	for (int r = 0; r < 16; r++)
	{
		if (mask & (1 << r))
			count++;
	}
	rsp_alignment -= count * 8;
	size_t subtraction = 0;
	if (mask & 0xffff0000)
	{
		// If we have any XMMs to save, we must align the stack here.
		subtraction = rsp_alignment & 0xf;
	}
	for (int x = 0; x < 16; x++)
	{
		if (mask & (1 << (16 + x)))
			subtraction += 16;
	}
	size_t xmm_base_subtraction = subtraction;
	subtraction += needed_frame_size;
	subtraction += shadow;
	// Final alignment.
	rsp_alignment -= subtraction;
	subtraction += rsp_alignment & 0xf;

	*shadowp = shadow;
	*subtractionp = subtraction;
	*xmm_offsetp = subtraction - xmm_base_subtraction;
}

size_t XEmitter::ABI_PushRegistersAndAdjustStack(u32 mask, size_t rsp_alignment, size_t needed_frame_size)
{
	size_t shadow, subtraction, xmm_offset;
	ABI_CalculateFrameSize(mask, rsp_alignment, needed_frame_size, &shadow, &subtraction, &xmm_offset);

	for (int r = 0; r < 16; r++)
	{
		if (mask & (1 << r))
			PUSH((X64Reg) r);
	}

	if (subtraction)
		SUB(64, R(RSP), subtraction >= 0x80 ? Imm32((u32)subtraction) : Imm8((u8)subtraction));

	for (int x = 0; x < 16; x++)
	{
		if (mask & (1 << (16 + x)))
		{
			MOVAPD(MDisp(RSP, (int)xmm_offset), (X64Reg) x);
			xmm_offset += 16;
		}
	}

	return shadow;
}

void XEmitter::ABI_PopRegistersAndAdjustStack(u32 mask, size_t rsp_alignment, size_t needed_frame_size)
{
	size_t shadow, subtraction, xmm_offset;
	ABI_CalculateFrameSize(mask, rsp_alignment, needed_frame_size, &shadow, &subtraction, &xmm_offset);

	for (int x = 0; x < 16; x++)
	{
		if (mask & (1 << (16 + x)))
		{
			MOVAPD((X64Reg) x, MDisp(RSP, (int)xmm_offset));
			xmm_offset += 16;
		}
	}

	if (subtraction)
		ADD(64, R(RSP), subtraction >= 0x80 ? Imm32((u32)subtraction) : Imm8((u8)subtraction));

	for (int r = 15; r >= 0; r--)
	{
		if (mask & (1 << r))
		{
			POP((X64Reg) r);
		}
	}
}

// Common functions
void XEmitter::ABI_CallFunction(void *func)
{
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

void XEmitter::ABI_CallFunctionC16(void *func, u16 param1)
{
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

void XEmitter::ABI_CallFunctionCC16(void *func, u32 param1, u16 param2)
{
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

void XEmitter::ABI_CallFunctionC(void *func, u32 param1)
{
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

void XEmitter::ABI_CallFunctionCC(void *func, u32 param1, u32 param2)
{
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

void XEmitter::ABI_CallFunctionCP(void *func, u32 param1, void *param2)
{
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

void XEmitter::ABI_CallFunctionCCC(void *func, u32 param1, u32 param2, u32 param3)
{
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

void XEmitter::ABI_CallFunctionCCP(void *func, u32 param1, u32 param2, void *param3)
{
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

void XEmitter::ABI_CallFunctionCCCP(void *func, u32 param1, u32 param2, u32 param3, void *param4)
{
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

void XEmitter::ABI_CallFunctionPC(void *func, void *param1, u32 param2)
{
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

void XEmitter::ABI_CallFunctionPPC(void *func, void *param1, void *param2, u32 param3)
{
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
void XEmitter::ABI_CallFunctionR(void *func, X64Reg reg1)
{
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
void XEmitter::ABI_CallFunctionRR(void *func, X64Reg reg1, X64Reg reg2, bool noProlog)
{
	ABI_AlignStack(0, noProlog);
	MOVTwo(64, ABI_PARAM1, reg1, ABI_PARAM2, reg2, ABI_PARAM3);
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

void XEmitter::MOVTwo(int bits, Gen::X64Reg dst1, Gen::X64Reg src1, Gen::X64Reg dst2, Gen::X64Reg src2, X64Reg temp)
{
	if (dst1 == src2 && dst2 == src1)
	{
		// need a temporary
		MOV(bits, R(temp), R(src1));
		src1 = temp;
	}
	if (src2 != dst1)
	{
		if (dst1 != src1)
			MOV(bits, R(dst1), R(src1));
		if (dst2 != src2)
			MOV(bits, R(dst2), R(src2));
	}
	else
	{
		if (dst2 != src2)
			MOV(bits, R(dst2), R(src2));
		if (dst1 != src1)
			MOV(bits, R(dst1), R(src1));
	}
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

void XEmitter::ABI_PushAllCalleeSavedRegsAndAdjustStack()
{
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

void XEmitter::ABI_PopAllCalleeSavedRegsAndAdjustStack()
{
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

void XEmitter::ABI_PushAllCalleeSavedRegsAndAdjustStack()
{
	PUSH(RBP);
	MOV(64, R(RBP), R(RSP));
	PUSH(RBX);
	PUSH(R12);
	PUSH(R13);
	PUSH(R14);
	PUSH(R15);
	SUB(64, R(RSP), Imm8(8));
}

void XEmitter::ABI_PopAllCalleeSavedRegsAndAdjustStack()
{
	ADD(64, R(RSP), Imm8(8));
	POP(R15);
	POP(R14);
	POP(R13);
	POP(R12);
	POP(RBX);
	POP(RBP);
}

#endif // WIN32

