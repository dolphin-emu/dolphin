// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CPUDetect.h"
#include "Common/MemoryUtil.h"

#include "Core/PowerPC/JitCommon/JitAsmCommon.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

#define QUANTIZED_REGS_TO_SAVE (ABI_ALL_CALLEE_SAVED & ~((1 << RAX) | (1 << RCX) | (1 << RDX) | \
                                                         (1 << XMM0) | (1 << XMM1)))

using namespace Gen;

static int temp32;

void CommonAsmRoutines::GenFifoWrite(int size)
{
	// Assume value in ABI_PARAM1
	PUSH(ESI);
	if (size != 32)
		PUSH(EDX);
	MOV(32, R(EAX), Imm32((u32)(u64)GPFifo::m_gatherPipe));
	MOV(32, R(ESI), M(&GPFifo::m_gatherPipeCount));

	SwapAndStore(size, MComplex(RAX, RSI, 1, 0), ABI_PARAM1);

	ADD(32, R(ESI), Imm8(size >> 3));
	MOV(32, M(&GPFifo::m_gatherPipeCount), R(ESI));
	if (size != 32)
		POP(EDX);
	POP(ESI);
	RET();
}

void CommonAsmRoutines::GenFifoFloatWrite()
{
	// Assume value in XMM0
	PUSH(ESI);
	PUSH(EDX);
	MOVSS(M(&temp32), XMM0);
	MOV(32, R(EDX), M(&temp32));
	MOV(32, R(EAX), Imm32((u32)(u64)GPFifo::m_gatherPipe));
	MOV(32, R(ESI), M(&GPFifo::m_gatherPipeCount));
	SwapAndStore(32, MComplex(RAX, RSI, 1, 0), EDX);
	ADD(32, R(ESI), Imm8(4));
	MOV(32, M(&GPFifo::m_gatherPipeCount), R(ESI));
	POP(EDX);
	POP(ESI);
	RET();
}

// Safe + Fast Quantizers, originally from JITIL by magumagu

static const u8 GC_ALIGNED16(pbswapShuffle1x4[16]) = {3, 2, 1, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
static const u8 GC_ALIGNED16(pbswapShuffle2x4[16]) = {3, 2, 1, 0, 7, 6, 5, 4, 8, 9, 10, 11, 12, 13, 14, 15};

static const float GC_ALIGNED16(m_quantizeTableS[]) =
{
	(1 <<  0),  (1 <<  1),  (1 <<  2),  (1 <<  3),
	(1 <<  4),  (1 <<  5),  (1 <<  6),  (1 <<  7),
	(1 <<  8),  (1 <<  9),  (1 << 10),  (1 << 11),
	(1 << 12),  (1 << 13),  (1 << 14),  (1 << 15),
	(1 << 16),  (1 << 17),  (1 << 18),  (1 << 19),
	(1 << 20),  (1 << 21),  (1 << 22),  (1 << 23),
	(1 << 24),  (1 << 25),  (1 << 26),  (1 << 27),
	(1 << 28),  (1 << 29),  (1 << 30),  (1 << 31),
	1.0 / (1ULL << 32), 1.0 / (1 << 31), 1.0 / (1 << 30), 1.0 / (1 << 29),
	1.0 / (1 << 28),    1.0 / (1 << 27), 1.0 / (1 << 26), 1.0 / (1 << 25),
	1.0 / (1 << 24),    1.0 / (1 << 23), 1.0 / (1 << 22), 1.0 / (1 << 21),
	1.0 / (1 << 20),    1.0 / (1 << 19), 1.0 / (1 << 18), 1.0 / (1 << 17),
	1.0 / (1 << 16),    1.0 / (1 << 15), 1.0 / (1 << 14), 1.0 / (1 << 13),
	1.0 / (1 << 12),    1.0 / (1 << 11), 1.0 / (1 << 10), 1.0 / (1 <<  9),
	1.0 / (1 <<  8),    1.0 / (1 <<  7), 1.0 / (1 <<  6), 1.0 / (1 <<  5),
	1.0 / (1 <<  4),    1.0 / (1 <<  3), 1.0 / (1 <<  2), 1.0 / (1 <<  1),
};

static const float GC_ALIGNED16(m_dequantizeTableS[]) =
{
	1.0 / (1 <<  0), 1.0 / (1 <<  1), 1.0 / (1 <<  2), 1.0 / (1 <<  3),
	1.0 / (1 <<  4), 1.0 / (1 <<  5), 1.0 / (1 <<  6), 1.0 / (1 <<  7),
	1.0 / (1 <<  8), 1.0 / (1 <<  9), 1.0 / (1 << 10), 1.0 / (1 << 11),
	1.0 / (1 << 12), 1.0 / (1 << 13), 1.0 / (1 << 14), 1.0 / (1 << 15),
	1.0 / (1 << 16), 1.0 / (1 << 17), 1.0 / (1 << 18), 1.0 / (1 << 19),
	1.0 / (1 << 20), 1.0 / (1 << 21), 1.0 / (1 << 22), 1.0 / (1 << 23),
	1.0 / (1 << 24), 1.0 / (1 << 25), 1.0 / (1 << 26), 1.0 / (1 << 27),
	1.0 / (1 << 28), 1.0 / (1 << 29), 1.0 / (1 << 30), 1.0 / (1 << 31),
	(1ULL << 32),   (1 << 31),      (1 << 30),      (1 << 29),
	(1 << 28),      (1 << 27),      (1 << 26),      (1 << 25),
	(1 << 24),      (1 << 23),      (1 << 22),      (1 << 21),
	(1 << 20),      (1 << 19),      (1 << 18),      (1 << 17),
	(1 << 16),      (1 << 15),      (1 << 14),      (1 << 13),
	(1 << 12),      (1 << 11),      (1 << 10),      (1 <<  9),
	(1 <<  8),      (1 <<  7),      (1 <<  6),      (1 <<  5),
	(1 <<  4),      (1 <<  3),      (1 <<  2),      (1 <<  1),
};

static float GC_ALIGNED16(psTemp[4]);

static const float GC_ALIGNED16(m_65535) = 65535.0f;
static const float GC_ALIGNED16(m_32767) = 32767.0f;
static const float GC_ALIGNED16(m_m32768) = -32768.0f;
static const float GC_ALIGNED16(m_255) = 255.0f;
static const float GC_ALIGNED16(m_127) = 127.0f;
static const float GC_ALIGNED16(m_m128) = -128.0f;

static const float GC_ALIGNED16(m_one[]) = {1.0f, 0.0f, 0.0f, 0.0f};

#define QUANTIZE_OVERFLOW_SAFE

// according to Intel Docs CVTPS2DQ writes 0x80000000 if the source floating point value is out of int32 range
// while it's OK for large negatives, it isn't for positives
// I don't know whether the overflow actually happens in any games
// but it potentially can cause problems, so we need some clamping

static void WriteDual32(u32 address)
{
	Memory::Write_U64(*(u64 *) psTemp, address);
}

// See comment in header for in/outs.
void CommonAsmRoutines::GenQuantizedStores()
{
	const u8* storePairedIllegal = AlignCode4();
	UD2();
	const u8* storePairedFloat = AlignCode4();

#if _M_X86_64
	SHUFPS(XMM0, R(XMM0), 1);
	MOVQ_xmm(M(&psTemp[0]), XMM0);
	TEST(32, R(ECX), Imm32(0x0C000000));
	FixupBranch too_complex = J_CC(CC_NZ, true);
	MOV(64, R(RAX), M(&psTemp[0]));
	SwapAndStore(64, MComplex(RBX, RCX, SCALE_1, 0), RAX);
	FixupBranch skip_complex = J(true);
	SetJumpTarget(too_complex);
	ABI_PushRegistersAndAdjustStack(QUANTIZED_REGS_TO_SAVE, true);
	ABI_CallFunctionR((void *)&WriteDual32, RCX);
	ABI_PopRegistersAndAdjustStack(QUANTIZED_REGS_TO_SAVE, true);
	SetJumpTarget(skip_complex);
	RET();
#else
	TEST(32, R(ECX), Imm32(0x0C000000));
	FixupBranch argh = J_CC(CC_NZ, true);
	MOVQ_xmm(M(&psTemp[0]), XMM0);
	MOV(32, R(EAX), M(&psTemp));
	BSWAP(32, EAX);
	AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));
	MOV(32, MDisp(ECX, (u32)Memory::base), R(EAX));
	MOV(32, R(EAX), M(((char*)&psTemp) + 4));
	BSWAP(32, EAX);
	MOV(32, MDisp(ECX, 4+(u32)Memory::base), R(EAX));
	FixupBranch arg2 = J(true);
	SetJumpTarget(argh);
	SHUFPS(XMM0, R(XMM0), 1);
	MOVQ_xmm(M(&psTemp[0]), XMM0);
	ABI_PushRegistersAndAdjustStack(QUANTIZED_REGS_TO_SAVE, true);
	ABI_CallFunctionR((void *)&WriteDual32, ECX);
	ABI_PopRegistersAndAdjustStack(QUANTIZED_REGS_TO_SAVE, true);
	SetJumpTarget(arg2);
	RET();
#endif

	const u8* storePairedU8 = AlignCode4();
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_quantizeTableS));
	PUNPCKLDQ(XMM1, R(XMM1));
	MULPS(XMM0, R(XMM1));
#ifdef QUANTIZE_OVERFLOW_SAFE
	MOVSS(XMM1, M((void *)&m_65535));
	PUNPCKLDQ(XMM1, R(XMM1));
	MINPS(XMM0, R(XMM1));
#endif
	CVTTPS2DQ(XMM0, R(XMM0));
	PACKSSDW(XMM0, R(XMM0));
	PACKUSWB(XMM0, R(XMM0));
	MOVD_xmm(R(EAX), XMM0);
	SafeWriteRegToReg(AX, ECX, 16, 0, QUANTIZED_REGS_TO_SAVE, SAFE_LOADSTORE_NO_SWAP | SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);

	RET();

	const u8* storePairedS8 = AlignCode4();
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_quantizeTableS));
	PUNPCKLDQ(XMM1, R(XMM1));
	MULPS(XMM0, R(XMM1));
#ifdef QUANTIZE_OVERFLOW_SAFE
	MOVSS(XMM1, M((void *)&m_65535));
	PUNPCKLDQ(XMM1, R(XMM1));
	MINPS(XMM0, R(XMM1));
#endif
	CVTTPS2DQ(XMM0, R(XMM0));
	PACKSSDW(XMM0, R(XMM0));
	PACKSSWB(XMM0, R(XMM0));
	MOVD_xmm(R(EAX), XMM0);

	SafeWriteRegToReg(AX, ECX, 16, 0, QUANTIZED_REGS_TO_SAVE, SAFE_LOADSTORE_NO_SWAP | SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);

	RET();

	const u8* storePairedU16 = AlignCode4();
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_quantizeTableS));
	PUNPCKLDQ(XMM1, R(XMM1));
	MULPS(XMM0, R(XMM1));

	// PACKUSDW is available only in SSE4
	PXOR(XMM1, R(XMM1));
	MAXPS(XMM0, R(XMM1));
	MOVSS(XMM1, M((void *)&m_65535));
	PUNPCKLDQ(XMM1, R(XMM1));
	MINPS(XMM0, R(XMM1));

	CVTTPS2DQ(XMM0, R(XMM0));
	MOVQ_xmm(M(psTemp), XMM0);
	// place ps[0] into the higher word, ps[1] into the lower
	// so no need in ROL after BSWAP
	MOVZX(32, 16, EAX, M((char*)psTemp + 0));
	SHL(32, R(EAX), Imm8(16));
	MOV(16, R(AX), M((char*)psTemp + 4));

	BSWAP(32, EAX);
	SafeWriteRegToReg(EAX, ECX, 32, 0, QUANTIZED_REGS_TO_SAVE, SAFE_LOADSTORE_NO_SWAP | SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);

	RET();

	const u8* storePairedS16 = AlignCode4();
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_quantizeTableS));
	// SHUFPS or UNPCKLPS might be a better choice here. The last one might just be an alias though.
	PUNPCKLDQ(XMM1, R(XMM1));
	MULPS(XMM0, R(XMM1));
#ifdef QUANTIZE_OVERFLOW_SAFE
	MOVSS(XMM1, M((void *)&m_65535));
	PUNPCKLDQ(XMM1, R(XMM1));
	MINPS(XMM0, R(XMM1));
#endif
	CVTTPS2DQ(XMM0, R(XMM0));
	PACKSSDW(XMM0, R(XMM0));
	MOVD_xmm(R(EAX), XMM0);
	BSWAP(32, EAX);
	ROL(32, R(EAX), Imm8(16));
	SafeWriteRegToReg(EAX, ECX, 32, 0, QUANTIZED_REGS_TO_SAVE, SAFE_LOADSTORE_NO_SWAP | SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);

	RET();

	pairedStoreQuantized = reinterpret_cast<const u8**>(const_cast<u8*>(AlignCode16()));
	ReserveCodeSpace(8 * sizeof(u8*));

	pairedStoreQuantized[0] = storePairedFloat;
	pairedStoreQuantized[1] = storePairedIllegal;
	pairedStoreQuantized[2] = storePairedIllegal;
	pairedStoreQuantized[3] = storePairedIllegal;
	pairedStoreQuantized[4] = storePairedU8;
	pairedStoreQuantized[5] = storePairedU16;
	pairedStoreQuantized[6] = storePairedS8;
	pairedStoreQuantized[7] = storePairedS16;
}

// See comment in header for in/outs.
void CommonAsmRoutines::GenQuantizedSingleStores()
{
	const u8* storeSingleIllegal = AlignCode4();
	UD2();

	// Easy!
	const u8* storeSingleFloat = AlignCode4();
	SafeWriteF32ToReg(XMM0, ECX, 0, QUANTIZED_REGS_TO_SAVE, SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);
	RET();
	/*
	if (cpu_info.bSSSE3) {
		PSHUFB(XMM0, M((void *)pbswapShuffle2x4));
		// TODO: SafeWriteFloat
		MOVSS(M(&psTemp[0]), XMM0);
		MOV(32, R(EAX), M(&psTemp[0]));
		SafeWriteRegToReg(EAX, ECX, 32, 0, SAFE_LOADSTORE_NO_SWAP | SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);
	} else {
		MOVSS(M(&psTemp[0]), XMM0);
		MOV(32, R(EAX), M(&psTemp[0]));
		SafeWriteRegToReg(EAX, ECX, 32, 0, SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);
	}*/

	const u8* storeSingleU8 = AlignCode4();  // Used by MKWii
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_quantizeTableS));
	MULSS(XMM0, R(XMM1));
	PXOR(XMM1, R(XMM1));
	MAXSS(XMM0, R(XMM1));
	MINSS(XMM0, M((void *)&m_255));
	CVTTSS2SI(EAX, R(XMM0));
	SafeWriteRegToReg(AL, ECX, 8, 0, QUANTIZED_REGS_TO_SAVE, SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);
	RET();

	const u8* storeSingleS8 = AlignCode4();
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_quantizeTableS));
	MULSS(XMM0, R(XMM1));
	MAXSS(XMM0, M((void *)&m_m128));
	MINSS(XMM0, M((void *)&m_127));
	CVTTSS2SI(EAX, R(XMM0));
	SafeWriteRegToReg(AL, ECX, 8, 0, QUANTIZED_REGS_TO_SAVE, SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);
	RET();

	const u8* storeSingleU16 = AlignCode4();  // Used by MKWii
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_quantizeTableS));
	MULSS(XMM0, R(XMM1));
	PXOR(XMM1, R(XMM1));
	MAXSS(XMM0, R(XMM1));
	MINSS(XMM0, M((void *)&m_65535));
	CVTTSS2SI(EAX, R(XMM0));
	SafeWriteRegToReg(EAX, ECX, 16, 0, QUANTIZED_REGS_TO_SAVE, SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);
	RET();

	const u8* storeSingleS16 = AlignCode4();
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_quantizeTableS));
	MULSS(XMM0, R(XMM1));
	MAXSS(XMM0, M((void *)&m_m32768));
	MINSS(XMM0, M((void *)&m_32767));
	CVTTSS2SI(EAX, R(XMM0));
	SafeWriteRegToReg(EAX, ECX, 16, 0, QUANTIZED_REGS_TO_SAVE, SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);
	RET();

	singleStoreQuantized = reinterpret_cast<const u8**>(const_cast<u8*>(AlignCode16()));
	ReserveCodeSpace(8 * sizeof(u8*));

	singleStoreQuantized[0] = storeSingleFloat;
	singleStoreQuantized[1] = storeSingleIllegal;
	singleStoreQuantized[2] = storeSingleIllegal;
	singleStoreQuantized[3] = storeSingleIllegal;
	singleStoreQuantized[4] = storeSingleU8;
	singleStoreQuantized[5] = storeSingleU16;
	singleStoreQuantized[6] = storeSingleS8;
	singleStoreQuantized[7] = storeSingleS16;
}

void CommonAsmRoutines::GenQuantizedLoads()
{
	const u8* loadPairedIllegal = AlignCode4();
	UD2();

	const u8* loadPairedFloatTwo = AlignCode4();
	if (cpu_info.bSSSE3) {
#if _M_X86_64
		MOVQ_xmm(XMM0, MComplex(RBX, RCX, 1, 0));
#else
		AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));
		MOVQ_xmm(XMM0, MDisp(ECX, (u32)Memory::base));
#endif
		PSHUFB(XMM0, M((void *)pbswapShuffle2x4));
	} else {
#if _M_X86_64
		LoadAndSwap(64, RCX, MComplex(RBX, RCX, 1, 0));
		ROL(64, R(RCX), Imm8(32));
		MOVQ_xmm(XMM0, R(RCX));
#else
#if 0
		AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));
		MOVQ_xmm(XMM0, MDisp(ECX, (u32)Memory::base));
		PXOR(XMM1, R(XMM1));
		PSHUFLW(XMM0, R(XMM0), 0xB1);
		MOVAPD(XMM1, R(XMM0));
		PSRLW(XMM0, 8);
		PSLLW(XMM1, 8);
		POR(XMM0, R(XMM1));
#else
		AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));
		MOV(32, R(EAX), MDisp(ECX, (u32)Memory::base));
		BSWAP(32, EAX);
		MOV(32, M(&psTemp[0]), R(RAX));
		MOV(32, R(EAX), MDisp(ECX, (u32)Memory::base + 4));
		BSWAP(32, EAX);
		MOV(32, M(((float *)&psTemp[0]) + 1), R(RAX));
		MOVQ_xmm(XMM0, M(&psTemp[0]));
#endif
#endif
	}
	RET();

	const u8* loadPairedFloatOne = AlignCode4();
	if (cpu_info.bSSSE3) {
#if _M_X86_64
		MOVD_xmm(XMM0, MComplex(RBX, RCX, 1, 0));
#else
		AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));
		MOVD_xmm(XMM0, MDisp(ECX, (u32)Memory::base));
#endif
		PSHUFB(XMM0, M((void *)pbswapShuffle1x4));
		UNPCKLPS(XMM0, M((void*)m_one));
	} else {
#if _M_X86_64
		LoadAndSwap(32, RCX, MComplex(RBX, RCX, 1, 0));
		MOVD_xmm(XMM0, R(RCX));
		UNPCKLPS(XMM0, M((void*)m_one));
#else
		AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));
		MOV(32, R(EAX), MDisp(ECX, (u32)Memory::base));
		BSWAP(32, EAX);
		MOVD_xmm(XMM0, R(EAX));
		UNPCKLPS(XMM0, M((void*)m_one));
#endif
	}
	RET();

	const u8* loadPairedU8Two = AlignCode4();
	UnsafeLoadRegToRegNoSwap(ECX, ECX, 16, 0);
	MOVD_xmm(XMM0, R(ECX));
	PXOR(XMM1, R(XMM1));
	PUNPCKLBW(XMM0, R(XMM1));
	PUNPCKLWD(XMM0, R(XMM1));
	CVTDQ2PS(XMM0, R(XMM0));
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_dequantizeTableS));
	PUNPCKLDQ(XMM1, R(XMM1));
	MULPS(XMM0, R(XMM1));
	RET();

	const u8* loadPairedU8One = AlignCode4();
	UnsafeLoadRegToRegNoSwap(ECX, ECX, 8, 0); // ECX = 0x000000xx
	MOVD_xmm(XMM0, R(ECX));
	CVTDQ2PS(XMM0, R(XMM0)); // Is CVTSI2SS better?
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_dequantizeTableS));
	MULSS(XMM0, R(XMM1));
	UNPCKLPS(XMM0, M((void*)m_one));
	RET();

	const u8* loadPairedS8Two = AlignCode4();
	UnsafeLoadRegToRegNoSwap(ECX, ECX, 16, 0);
	MOVD_xmm(XMM0, R(ECX));
	PUNPCKLBW(XMM0, R(XMM0));
	PUNPCKLWD(XMM0, R(XMM0));
	PSRAD(XMM0, 24);
	CVTDQ2PS(XMM0, R(XMM0));
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_dequantizeTableS));
	PUNPCKLDQ(XMM1, R(XMM1));
	MULPS(XMM0, R(XMM1));
	RET();

	const u8* loadPairedS8One = AlignCode4();
	UnsafeLoadRegToRegNoSwap(ECX, ECX, 8, 0);
	SHL(32, R(ECX), Imm8(24));
	SAR(32, R(ECX), Imm8(24));
	MOVD_xmm(XMM0, R(ECX));
	CVTDQ2PS(XMM0, R(XMM0));
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_dequantizeTableS));
	MULSS(XMM0, R(XMM1));
	UNPCKLPS(XMM0, M((void*)m_one));
	RET();

	const u8* loadPairedU16Two = AlignCode4();
	UnsafeLoadRegToReg(ECX, ECX, 32, 0, false);
	ROL(32, R(ECX), Imm8(16));
	MOVD_xmm(XMM0, R(ECX));
	PXOR(XMM1, R(XMM1));
	PUNPCKLWD(XMM0, R(XMM1));
	CVTDQ2PS(XMM0, R(XMM0));
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_dequantizeTableS));
	PUNPCKLDQ(XMM1, R(XMM1));
	MULPS(XMM0, R(XMM1));
	RET();

	const u8* loadPairedU16One = AlignCode4();
	UnsafeLoadRegToReg(ECX, ECX, 32, 0, false);
	SHR(32, R(ECX), Imm8(16));
	MOVD_xmm(XMM0, R(ECX));
	CVTDQ2PS(XMM0, R(XMM0));
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_dequantizeTableS));
	MULSS(XMM0, R(XMM1));
	UNPCKLPS(XMM0, M((void*)m_one));
	RET();

	const u8* loadPairedS16Two = AlignCode4();
	UnsafeLoadRegToReg(ECX, ECX, 32, 0, false);
	ROL(32, R(ECX), Imm8(16));
	MOVD_xmm(XMM0, R(ECX));
	PUNPCKLWD(XMM0, R(XMM0));
	PSRAD(XMM0, 16);
	CVTDQ2PS(XMM0, R(XMM0));
	SHR(32, R(EAX), Imm8(6));
	AND(32, R(EAX), Imm32(0xFC));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_dequantizeTableS));
	PUNPCKLDQ(XMM1, R(XMM1));
	MULPS(XMM0, R(XMM1));
	RET();

	const u8* loadPairedS16One = AlignCode4();
	UnsafeLoadRegToReg(ECX, ECX, 32, 0, false);
	SAR(32, R(ECX), Imm8(16));
	MOVD_xmm(XMM0, R(ECX));
	CVTDQ2PS(XMM0, R(XMM0));
	SHR(32, R(EAX), Imm8(6));
	AND(32, R(EAX), Imm32(0xFC));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_dequantizeTableS));
	MULSS(XMM0, R(XMM1));
	UNPCKLPS(XMM0, M((void*)m_one));
	RET();

	pairedLoadQuantized = reinterpret_cast<const u8**>(const_cast<u8*>(AlignCode16()));
	ReserveCodeSpace(16 * sizeof(u8*));

	pairedLoadQuantized[0] = loadPairedFloatTwo;
	pairedLoadQuantized[1] = loadPairedIllegal;
	pairedLoadQuantized[2] = loadPairedIllegal;
	pairedLoadQuantized[3] = loadPairedIllegal;
	pairedLoadQuantized[4] = loadPairedU8Two;
	pairedLoadQuantized[5] = loadPairedU16Two;
	pairedLoadQuantized[6] = loadPairedS8Two;
	pairedLoadQuantized[7] = loadPairedS16Two;

	pairedLoadQuantized[8] = loadPairedFloatOne;
	pairedLoadQuantized[9] = loadPairedIllegal;
	pairedLoadQuantized[10] = loadPairedIllegal;
	pairedLoadQuantized[11] = loadPairedIllegal;
	pairedLoadQuantized[12] = loadPairedU8One;
	pairedLoadQuantized[13] = loadPairedU16One;
	pairedLoadQuantized[14] = loadPairedS8One;
	pairedLoadQuantized[15] = loadPairedS16One;
}
