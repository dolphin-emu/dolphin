// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/x64ABI.h"
#include "Common/x64Emitter.h"

using namespace Gen;

// Shared code between Win64 and Unix64

void XEmitter::ABI_CalculateFrameSize(BitSet32 mask, size_t rsp_alignment, size_t needed_frame_size, size_t* shadowp, size_t* subtractionp, size_t* xmm_offsetp)
{
	size_t shadow = 0;
#if defined(_WIN32)
	shadow = 0x20;
#endif

	int count = (mask & ABI_ALL_GPRS).Count();
	rsp_alignment -= count * 8;
	size_t subtraction = 0;
	int fpr_count = (mask & ABI_ALL_FPRS).Count();
	if (fpr_count)
	{
		// If we have any XMMs to save, we must align the stack here.
		subtraction = rsp_alignment & 0xf;
	}
	subtraction += 16 * fpr_count;
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

size_t XEmitter::ABI_PushRegistersAndAdjustStack(BitSet32 mask, size_t rsp_alignment, size_t needed_frame_size)
{
	size_t shadow, subtraction, xmm_offset;
	ABI_CalculateFrameSize(mask, rsp_alignment, needed_frame_size, &shadow, &subtraction, &xmm_offset);

	for (int r : mask & ABI_ALL_GPRS)
		PUSH((X64Reg) r);

	if (subtraction)
		SUB(64, R(RSP), subtraction >= 0x80 ? Imm32((u32)subtraction) : Imm8((u8)subtraction));

	for (int x : mask & ABI_ALL_FPRS)
	{
		MOVAPD(MDisp(RSP, (int)xmm_offset), (X64Reg) (x - 16));
		xmm_offset += 16;
	}

	return shadow;
}

void XEmitter::ABI_PopRegistersAndAdjustStack(BitSet32 mask, size_t rsp_alignment, size_t needed_frame_size)
{
	size_t shadow, subtraction, xmm_offset;
	ABI_CalculateFrameSize(mask, rsp_alignment, needed_frame_size, &shadow, &subtraction, &xmm_offset);

	for (int x : mask & ABI_ALL_FPRS)
	{
		MOVAPD((X64Reg) (x - 16), MDisp(RSP, (int)xmm_offset));
		xmm_offset += 16;
	}

	if (subtraction)
		ADD(64, R(RSP), subtraction >= 0x80 ? Imm32((u32)subtraction) : Imm8((u8)subtraction));

	for (int r = 15; r >= 0; r--)
	{
		if (mask[r])
			POP((X64Reg) r);
	}
}

// Common functions
void XEmitter::ABI_CallFunction(const void *func)
{
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
}

void XEmitter::ABI_CallFunctionC16(const void *func, u16 param1)
{
	MOV(32, R(ABI_PARAM1), Imm32((u32)param1));
	ABI_CallFunction(func);
}

void XEmitter::ABI_CallFunctionCC16(const void *func, u32 param1, u16 param2)
{
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	MOV(32, R(ABI_PARAM2), Imm32((u32)param2));
	ABI_CallFunction(func);
}

void XEmitter::ABI_CallFunctionC(const void *func, u32 param1)
{
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	ABI_CallFunction(func);
}

void XEmitter::ABI_CallFunctionCC(const void *func, u32 param1, u32 param2)
{
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	MOV(32, R(ABI_PARAM2), Imm32(param2));
	ABI_CallFunction(func);
}

void XEmitter::ABI_CallFunctionCP(const void *func, u32 param1, void *param2)
{
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	MOV(64, R(ABI_PARAM2), Imm64((u64)param2));
	ABI_CallFunction(func);
}

void XEmitter::ABI_CallFunctionCCC(const void *func, u32 param1, u32 param2, u32 param3)
{
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	MOV(32, R(ABI_PARAM2), Imm32(param2));
	MOV(32, R(ABI_PARAM3), Imm32(param3));
	ABI_CallFunction(func);
}

void XEmitter::ABI_CallFunctionCCP(const void *func, u32 param1, u32 param2, void *param3)
{
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	MOV(32, R(ABI_PARAM2), Imm32(param2));
	MOV(64, R(ABI_PARAM3), Imm64((u64)param3));
	ABI_CallFunction(func);
}

void XEmitter::ABI_CallFunctionCCCP(const void *func, u32 param1, u32 param2, u32 param3, void *param4)
{
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	MOV(32, R(ABI_PARAM2), Imm32(param2));
	MOV(32, R(ABI_PARAM3), Imm32(param3));
	MOV(64, R(ABI_PARAM4), Imm64((u64)param4));
	ABI_CallFunction(func);
}

void XEmitter::ABI_CallFunctionPC(const void *func, void *param1, u32 param2)
{
	MOV(64, R(ABI_PARAM1), Imm64((u64)param1));
	MOV(32, R(ABI_PARAM2), Imm32(param2));
	ABI_CallFunction(func);
}

void XEmitter::ABI_CallFunctionPPC(const void *func, void *param1, void *param2, u32 param3)
{
	MOV(64, R(ABI_PARAM1), Imm64((u64)param1));
	MOV(64, R(ABI_PARAM2), Imm64((u64)param2));
	MOV(32, R(ABI_PARAM3), Imm32(param3));
	ABI_CallFunction(func);
}

// Pass a register as a parameter.
void XEmitter::ABI_CallFunctionR(const void *func, X64Reg reg1)
{
	if (reg1 != ABI_PARAM1)
		MOV(32, R(ABI_PARAM1), R(reg1));
	ABI_CallFunction(func);
}

// Pass two registers as parameters.
void XEmitter::ABI_CallFunctionRR(const void *func, X64Reg reg1, X64Reg reg2)
{
	MOVTwo(64, ABI_PARAM1, reg1, ABI_PARAM2, reg2);
	ABI_CallFunction(func);
}

void XEmitter::MOVTwo(int bits, Gen::X64Reg dst1, Gen::X64Reg src1, Gen::X64Reg dst2, Gen::X64Reg src2)
{
	if (dst1 == src2 && dst2 == src1)
	{
		XCHG(bits, R(src1), R(src2));
	}
	else if (src2 != dst1)
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

void XEmitter::ABI_CallFunctionAC(const void *func, const Gen::OpArg &arg1, u32 param2)
{
	if (!arg1.IsSimpleReg(ABI_PARAM1))
		MOV(32, R(ABI_PARAM1), arg1);
	MOV(32, R(ABI_PARAM2), Imm32(param2));
	ABI_CallFunction(func);
}

void XEmitter::ABI_CallFunctionA(const void *func, const Gen::OpArg &arg1)
{
	if (!arg1.IsSimpleReg(ABI_PARAM1))
		MOV(32, R(ABI_PARAM1), arg1);
	ABI_CallFunction(func);
}

