// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/JitRegister.h"
#include "Common/MathUtil.h"
#include "Common/x64ABI.h"
#include "Common/x64Emitter.h"

#include "Core/PowerPC/Jit64Common/Jit64AsmCommon.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

#define QUANTIZED_REGS_TO_SAVE \
	(ABI_ALL_CALLER_SAVED & ~BitSet32 { \
		RSCRATCH, RSCRATCH2, RSCRATCH_EXTRA, XMM0+16, XMM1+16 \
	})

#define QUANTIZED_REGS_TO_SAVE_LOAD (QUANTIZED_REGS_TO_SAVE | BitSet32 { RSCRATCH2 })

using namespace Gen;

void CommonAsmRoutines::GenFifoWrite(int size)
{
	const void* start = GetCodePtr();

	// Assume value in RSCRATCH
	u32 gather_pipe = (u32)(u64)GPFifo::m_gatherPipe;
	_assert_msg_(DYNA_REC, gather_pipe <= 0x7FFFFFFF, "Gather pipe not in low 2GB of memory!");
	MOV(32, R(RSCRATCH2), M(&GPFifo::m_gatherPipeCount));
	SwapAndStore(size, MDisp(RSCRATCH2, gather_pipe), RSCRATCH);
	ADD(32, R(RSCRATCH2), Imm8(size >> 3));
	MOV(32, M(&GPFifo::m_gatherPipeCount), R(RSCRATCH2));
	RET();

	JitRegister::Register(start, GetCodePtr(), "JIT_FifoWrite_%i", size);
}

void CommonAsmRoutines::GenFrsqrte()
{
	const void* start = GetCodePtr();

	// Assume input in XMM0.
	// This function clobbers all three RSCRATCH.
	MOVQ_xmm(R(RSCRATCH), XMM0);

	// Negative and zero inputs set an exception and take the complex path.
	TEST(64, R(RSCRATCH), R(RSCRATCH));
	FixupBranch zero = J_CC(CC_Z, true);
	FixupBranch negative = J_CC(CC_S, true);
	MOV(64, R(RSCRATCH_EXTRA), R(RSCRATCH));
	SHR(64, R(RSCRATCH_EXTRA), Imm8(52));

	// Zero and max exponents (non-normal floats) take the complex path.
	FixupBranch complex1 = J_CC(CC_Z, true);
	CMP(32, R(RSCRATCH_EXTRA), Imm32(0x7FF));
	FixupBranch complex2 = J_CC(CC_E, true);

	SUB(32, R(RSCRATCH_EXTRA), Imm32(0x3FD));
	SAR(32, R(RSCRATCH_EXTRA), Imm8(1));
	MOV(32, R(RSCRATCH2), Imm32(0x3FF));
	SUB(32, R(RSCRATCH2), R(RSCRATCH_EXTRA));
	SHL(64, R(RSCRATCH2), Imm8(52));   // exponent = ((0x3FFLL << 52) - ((exponent - (0x3FELL << 52)) / 2)) & (0x7FFLL << 52);

	MOV(64, R(RSCRATCH_EXTRA), R(RSCRATCH));
	SHR(64, R(RSCRATCH_EXTRA), Imm8(48));
	AND(32, R(RSCRATCH_EXTRA), Imm8(0x1F));
	XOR(32, R(RSCRATCH_EXTRA), Imm8(0x10)); // int index = i / 2048 + (odd_exponent ? 16 : 0);

	SHR(64, R(RSCRATCH), Imm8(37));
	AND(32, R(RSCRATCH), Imm32(0x7FF));
	IMUL(32, RSCRATCH, MScaled(RSCRATCH_EXTRA, SCALE_4, (u32)(u64)MathUtil::frsqrte_expected_dec));
	MOV(32, R(RSCRATCH_EXTRA), MScaled(RSCRATCH_EXTRA, SCALE_4, (u32)(u64)MathUtil::frsqrte_expected_base));
	SUB(32, R(RSCRATCH_EXTRA), R(RSCRATCH));
	SHL(64, R(RSCRATCH_EXTRA), Imm8(26));
	OR(64, R(RSCRATCH2), R(RSCRATCH_EXTRA));     // vali |= (s64)(frsqrte_expected_base[index] - frsqrte_expected_dec[index] * (i % 2048)) << 26;
	MOVQ_xmm(XMM0, R(RSCRATCH2));
	RET();

	// Exception flags for zero input.
	SetJumpTarget(zero);
	TEST(32, PPCSTATE(fpscr), Imm32(FPSCR_ZX));
	FixupBranch skip_set_fx1 = J_CC(CC_NZ);
	OR(32, PPCSTATE(fpscr), Imm32(FPSCR_FX | FPSCR_ZX));
	FixupBranch complex3 = J();

	// Exception flags for negative input.
	SetJumpTarget(negative);
	TEST(32, PPCSTATE(fpscr), Imm32(FPSCR_VXSQRT));
	FixupBranch skip_set_fx2 = J_CC(CC_NZ);
	OR(32, PPCSTATE(fpscr), Imm32(FPSCR_FX | FPSCR_VXSQRT));

	SetJumpTarget(skip_set_fx1);
	SetJumpTarget(skip_set_fx2);
	SetJumpTarget(complex1);
	SetJumpTarget(complex2);
	SetJumpTarget(complex3);
	ABI_PushRegistersAndAdjustStack(QUANTIZED_REGS_TO_SAVE, 8);
	ABI_CallFunction((void *)&MathUtil::ApproximateReciprocalSquareRoot);
	ABI_PopRegistersAndAdjustStack(QUANTIZED_REGS_TO_SAVE, 8);
	RET();

	JitRegister::Register(start, GetCodePtr(), "JIT_Frsqrte");
}

void CommonAsmRoutines::GenFres()
{
	const void* start = GetCodePtr();

	// Assume input in XMM0.
	// This function clobbers all three RSCRATCH.
	MOVQ_xmm(R(RSCRATCH), XMM0);

	// Zero inputs set an exception and take the complex path.
	TEST(64, R(RSCRATCH), R(RSCRATCH));
	FixupBranch zero = J_CC(CC_Z);

	MOV(64, R(RSCRATCH_EXTRA), R(RSCRATCH));
	SHR(64, R(RSCRATCH_EXTRA), Imm8(52));
	MOV(32, R(RSCRATCH2), R(RSCRATCH_EXTRA));
	AND(32, R(RSCRATCH_EXTRA), Imm32(0x7FF)); // exp
	AND(32, R(RSCRATCH2), Imm32(0x800)); // sign
	CMP(32, R(RSCRATCH_EXTRA), Imm32(895));
	// Take the complex path for very large/small exponents.
	FixupBranch complex1 = J_CC(CC_L);
	CMP(32, R(RSCRATCH_EXTRA), Imm32(1149));
	FixupBranch complex2 = J_CC(CC_GE);

	SUB(32, R(RSCRATCH_EXTRA), Imm32(0x7FD));
	NEG(32, R(RSCRATCH_EXTRA));
	OR(32, R(RSCRATCH_EXTRA), R(RSCRATCH2));
	SHL(64, R(RSCRATCH_EXTRA), Imm8(52));	   // vali = sign | exponent

	MOV(64, R(RSCRATCH2), R(RSCRATCH));
	SHR(64, R(RSCRATCH), Imm8(37));
	SHR(64, R(RSCRATCH2), Imm8(47));
	AND(32, R(RSCRATCH), Imm32(0x3FF)); // i % 1024
	AND(32, R(RSCRATCH2), Imm8(0x1F));   // i / 1024

	IMUL(32, RSCRATCH, MScaled(RSCRATCH2, SCALE_4, (u32)(u64)MathUtil::fres_expected_dec));
	ADD(32, R(RSCRATCH), Imm8(1));
	SHR(32, R(RSCRATCH), Imm8(1));

	MOV(32, R(RSCRATCH2), MScaled(RSCRATCH2, SCALE_4, (u32)(u64)MathUtil::fres_expected_base));
	SUB(32, R(RSCRATCH2), R(RSCRATCH));
	SHL(64, R(RSCRATCH2), Imm8(29));
	OR(64, R(RSCRATCH2), R(RSCRATCH_EXTRA));     // vali |= (s64)(fres_expected_base[i / 1024] - (fres_expected_dec[i / 1024] * (i % 1024) + 1) / 2) << 29
	MOVQ_xmm(XMM0, R(RSCRATCH2));
	RET();

	// Exception flags for zero input.
	SetJumpTarget(zero);
	TEST(32, PPCSTATE(fpscr), Imm32(FPSCR_ZX));
	FixupBranch skip_set_fx1 = J_CC(CC_NZ);
	OR(32, PPCSTATE(fpscr), Imm32(FPSCR_FX | FPSCR_ZX));
	SetJumpTarget(skip_set_fx1);

	SetJumpTarget(complex1);
	SetJumpTarget(complex2);
	ABI_PushRegistersAndAdjustStack(QUANTIZED_REGS_TO_SAVE, 8);
	ABI_CallFunction((void *)&MathUtil::ApproximateReciprocal);
	ABI_PopRegistersAndAdjustStack(QUANTIZED_REGS_TO_SAVE, 8);
	RET();

	JitRegister::Register(start, GetCodePtr(), "JIT_Fres");
}

void CommonAsmRoutines::GenMfcr()
{
	const void* start = GetCodePtr();

	// Input: none
	// Output: RSCRATCH
	// This function clobbers all three RSCRATCH.
	X64Reg dst = RSCRATCH;
	X64Reg tmp = RSCRATCH2;
	X64Reg cr_val = RSCRATCH_EXTRA;
	XOR(32, R(dst), R(dst));
	// we only need to zero the high bits of tmp once
	XOR(32, R(tmp), R(tmp));
	for (int i = 0; i < 8; i++)
	{
		static const u32 m_flagTable[8] = { 0x0, 0x1, 0x8, 0x9, 0x0, 0x1, 0x8, 0x9 };
		if (i != 0)
			SHL(32, R(dst), Imm8(4));

		MOV(64, R(cr_val), PPCSTATE(cr_val[i]));

		// EQ: Bits 31-0 == 0; set flag bit 1
		TEST(32, R(cr_val), R(cr_val));
		// FIXME: is there a better way to do this without the partial register merging?
		SETcc(CC_Z, R(tmp));
		LEA(32, dst, MComplex(dst, tmp, SCALE_2, 0));

		// GT: Value > 0; set flag bit 2
		TEST(64, R(cr_val), R(cr_val));
		SETcc(CC_G, R(tmp));
		LEA(32, dst, MComplex(dst, tmp, SCALE_4, 0));

		// SO: Bit 61 set; set flag bit 0
		// LT: Bit 62 set; set flag bit 3
		SHR(64, R(cr_val), Imm8(61));
		OR(32, R(dst), MScaled(cr_val, SCALE_4, (u32)(u64)m_flagTable));
	}
	RET();

	JitRegister::Register(start, GetCodePtr(), "JIT_Mfcr");
}

// Safe + Fast Quantizers, originally from JITIL by magumagu
static const float GC_ALIGNED16(m_65535[4]) = {65535.0f, 65535.0f, 65535.0f, 65535.0f};
static const float GC_ALIGNED16(m_32767) = 32767.0f;
static const float GC_ALIGNED16(m_m32768) = -32768.0f;
static const float GC_ALIGNED16(m_255) = 255.0f;
static const float GC_ALIGNED16(m_127) = 127.0f;
static const float GC_ALIGNED16(m_m128) = -128.0f;

#define QUANTIZE_OVERFLOW_SAFE

// according to Intel Docs CVTPS2DQ writes 0x80000000 if the source floating point value is out of int32 range
// while it's OK for large negatives, it isn't for positives
// I don't know whether the overflow actually happens in any games
// but it potentially can cause problems, so we need some clamping

// See comment in header for in/outs.
void CommonAsmRoutines::GenQuantizedStores()
{
	const void* start = GetCodePtr();

	const u8* storePairedIllegal = AlignCode4();
	UD2();

	const u8* storePairedFloat = AlignCode4();
	if (cpu_info.bSSSE3)
	{
		PSHUFB(XMM0, M((void *)pbswapShuffle2x4));
		MOVQ_xmm(R(RSCRATCH), XMM0);
	}
	else
	{
		MOVQ_xmm(R(RSCRATCH), XMM0);
		ROL(64, R(RSCRATCH), Imm8(32));
		BSWAP(64, RSCRATCH);
	}
	SafeWriteRegToReg(RSCRATCH, RSCRATCH_EXTRA, 64, 0, QUANTIZED_REGS_TO_SAVE, SAFE_LOADSTORE_NO_SWAP | SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);

	RET();

	const u8* storePairedU8 = AlignCode4();
	SHR(32, R(RSCRATCH2), Imm8(5));
	MOVQ_xmm(XMM1, MDisp(RSCRATCH2, (u32)(u64)m_quantizeTableS));
	MULPS(XMM0, R(XMM1));
#ifdef QUANTIZE_OVERFLOW_SAFE
	MINPS(XMM0, M(m_65535));
#endif
	CVTTPS2DQ(XMM0, R(XMM0));
	PACKSSDW(XMM0, R(XMM0));
	PACKUSWB(XMM0, R(XMM0));
	MOVD_xmm(R(RSCRATCH), XMM0);
	SafeWriteRegToReg(RSCRATCH, RSCRATCH_EXTRA, 16, 0, QUANTIZED_REGS_TO_SAVE, SAFE_LOADSTORE_NO_SWAP | SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);

	RET();

	const u8* storePairedS8 = AlignCode4();
	SHR(32, R(RSCRATCH2), Imm8(5));
	MOVQ_xmm(XMM1, MDisp(RSCRATCH2, (u32)(u64)m_quantizeTableS));
	MULPS(XMM0, R(XMM1));
#ifdef QUANTIZE_OVERFLOW_SAFE
	MINPS(XMM0, M(m_65535));
#endif
	CVTTPS2DQ(XMM0, R(XMM0));
	PACKSSDW(XMM0, R(XMM0));
	PACKSSWB(XMM0, R(XMM0));
	MOVD_xmm(R(RSCRATCH), XMM0);

	SafeWriteRegToReg(RSCRATCH, RSCRATCH_EXTRA, 16, 0, QUANTIZED_REGS_TO_SAVE, SAFE_LOADSTORE_NO_SWAP | SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);

	RET();

	const u8* storePairedU16 = AlignCode4();
	SHR(32, R(RSCRATCH2), Imm8(5));
	MOVQ_xmm(XMM1, MDisp(RSCRATCH2, (u32)(u64)m_quantizeTableS));
	MULPS(XMM0, R(XMM1));

	if (cpu_info.bSSE4_1)
	{
#ifdef QUANTIZE_OVERFLOW_SAFE
		MINPS(XMM0, M(m_65535));
#endif
		CVTTPS2DQ(XMM0, R(XMM0));
		PACKUSDW(XMM0, R(XMM0));
		MOVD_xmm(R(RSCRATCH), XMM0);
		BSWAP(32, RSCRATCH);
		ROL(32, R(RSCRATCH), Imm8(16));
	}
	else
	{
		XORPS(XMM1, R(XMM1));
		MAXPS(XMM0, R(XMM1));
		MINPS(XMM0, M(m_65535));

		CVTTPS2DQ(XMM0, R(XMM0));
		PSHUFLW(XMM0, R(XMM0), 2); // AABBCCDD -> CCAA____
		MOVD_xmm(R(RSCRATCH), XMM0);
		BSWAP(32, RSCRATCH);
	}

	SafeWriteRegToReg(RSCRATCH, RSCRATCH_EXTRA, 32, 0, QUANTIZED_REGS_TO_SAVE, SAFE_LOADSTORE_NO_SWAP | SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);

	RET();

	const u8* storePairedS16 = AlignCode4();
	SHR(32, R(RSCRATCH2), Imm8(5));
	MOVQ_xmm(XMM1, MDisp(RSCRATCH2, (u32)(u64)m_quantizeTableS));
	MULPS(XMM0, R(XMM1));
#ifdef QUANTIZE_OVERFLOW_SAFE
	MINPS(XMM0, M(m_65535));
#endif
	CVTTPS2DQ(XMM0, R(XMM0));
	PACKSSDW(XMM0, R(XMM0));
	MOVD_xmm(R(RSCRATCH), XMM0);
	BSWAP(32, RSCRATCH);
	ROL(32, R(RSCRATCH), Imm8(16));
	SafeWriteRegToReg(RSCRATCH, RSCRATCH_EXTRA, 32, 0, QUANTIZED_REGS_TO_SAVE, SAFE_LOADSTORE_NO_SWAP | SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);

	RET();

	JitRegister::Register(start, GetCodePtr(), "JIT_QuantizedStore");

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
	const void* start = GetCodePtr();

	const u8* storeSingleIllegal = AlignCode4();
	UD2();

	// Easy!
	const u8* storeSingleFloat = AlignCode4();
	MOVD_xmm(R(RSCRATCH), XMM0);
	SafeWriteRegToReg(RSCRATCH, RSCRATCH_EXTRA, 32, 0, QUANTIZED_REGS_TO_SAVE, SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);
	RET();

	const u8* storeSingleU8 = AlignCode4();  // Used by MKWii
	SHR(32, R(RSCRATCH2), Imm8(5));
	MULSS(XMM0, MDisp(RSCRATCH2, (u32)(u64)m_quantizeTableS));
	XORPS(XMM1, R(XMM1));
	MAXSS(XMM0, R(XMM1));
	MINSS(XMM0, M(&m_255));
	CVTTSS2SI(RSCRATCH, R(XMM0));
	SafeWriteRegToReg(RSCRATCH, RSCRATCH_EXTRA, 8, 0, QUANTIZED_REGS_TO_SAVE, SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);
	RET();

	const u8* storeSingleS8 = AlignCode4();
	SHR(32, R(RSCRATCH2), Imm8(5));
	MULSS(XMM0, MDisp(RSCRATCH2, (u32)(u64)m_quantizeTableS));
	MAXSS(XMM0, M(&m_m128));
	MINSS(XMM0, M(&m_127));
	CVTTSS2SI(RSCRATCH, R(XMM0));
	SafeWriteRegToReg(RSCRATCH, RSCRATCH_EXTRA, 8, 0, QUANTIZED_REGS_TO_SAVE, SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);
	RET();

	const u8* storeSingleU16 = AlignCode4();  // Used by MKWii
	SHR(32, R(RSCRATCH2), Imm8(5));
	MULSS(XMM0, MDisp(RSCRATCH2, (u32)(u64)m_quantizeTableS));
	XORPS(XMM1, R(XMM1));
	MAXSS(XMM0, R(XMM1));
	MINSS(XMM0, M(m_65535));
	CVTTSS2SI(RSCRATCH, R(XMM0));
	SafeWriteRegToReg(RSCRATCH, RSCRATCH_EXTRA, 16, 0, QUANTIZED_REGS_TO_SAVE, SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);
	RET();

	const u8* storeSingleS16 = AlignCode4();
	SHR(32, R(RSCRATCH2), Imm8(5));
	MULSS(XMM0, MDisp(RSCRATCH2, (u32)(u64)m_quantizeTableS));
	MAXSS(XMM0, M(&m_m32768));
	MINSS(XMM0, M(&m_32767));
	CVTTSS2SI(RSCRATCH, R(XMM0));
	SafeWriteRegToReg(RSCRATCH, RSCRATCH_EXTRA, 16, 0, QUANTIZED_REGS_TO_SAVE, SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_NO_FASTMEM);
	RET();

	JitRegister::Register(start, GetCodePtr(), "JIT_QuantizedSingleStore");

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
	const void* start = GetCodePtr();

	const u8* loadPairedIllegal = AlignCode4();
	UD2();

	// FIXME? This code (in non-MMU mode) assumes all accesses are directly to RAM, i.e.
	// don't need hardware access handling. This will definitely crash if paired loads occur
	// from non-RAM areas, but as far as I know, this never happens. I don't know if this is
	// for a good reason, or merely because no game does this.
	// If we find something that actually does do this, maybe this should be changed. How
	// much of a performance hit would it be?
	const u8* loadPairedFloatTwo = AlignCode4();
	if (jit->jo.memcheck)
	{
		SafeLoadToReg(RSCRATCH_EXTRA, R(RSCRATCH_EXTRA), 64, 0, QUANTIZED_REGS_TO_SAVE, false, SAFE_LOADSTORE_NO_FASTMEM | SAFE_LOADSTORE_NO_PROLOG);
		ROL(64, R(RSCRATCH_EXTRA), Imm8(32));
		MOVQ_xmm(XMM0, R(RSCRATCH_EXTRA));
	}
	else if (cpu_info.bSSSE3)
	{
		MOVQ_xmm(XMM0, MRegSum(RMEM, RSCRATCH_EXTRA));
		PSHUFB(XMM0, M(pbswapShuffle2x4));
	}
	else
	{
		LoadAndSwap(64, RSCRATCH_EXTRA, MRegSum(RMEM, RSCRATCH_EXTRA));
		ROL(64, R(RSCRATCH_EXTRA), Imm8(32));
		MOVQ_xmm(XMM0, R(RSCRATCH_EXTRA));
	}
	RET();

	const u8* loadPairedFloatOne = AlignCode4();
	if (jit->jo.memcheck)
	{
		SafeLoadToReg(RSCRATCH_EXTRA, R(RSCRATCH_EXTRA), 32, 0, QUANTIZED_REGS_TO_SAVE, false, SAFE_LOADSTORE_NO_FASTMEM | SAFE_LOADSTORE_NO_PROLOG);
		MOVD_xmm(XMM0, R(RSCRATCH_EXTRA));
		UNPCKLPS(XMM0, M(m_one));
	}
	else if (cpu_info.bSSSE3)
	{
		MOVD_xmm(XMM0, MRegSum(RMEM, RSCRATCH_EXTRA));
		PSHUFB(XMM0, M(pbswapShuffle1x4));
		UNPCKLPS(XMM0, M(m_one));
	}
	else
	{
		LoadAndSwap(32, RSCRATCH_EXTRA, MRegSum(RMEM, RSCRATCH_EXTRA));
		MOVD_xmm(XMM0, R(RSCRATCH_EXTRA));
		UNPCKLPS(XMM0, M(m_one));
	}
	RET();

	const u8* loadPairedU8Two = AlignCode4();
	if (jit->jo.memcheck)
	{
		// TODO: Support not swapping in safeLoadToReg to avoid bswapping twice
		SafeLoadToReg(RSCRATCH_EXTRA, R(RSCRATCH_EXTRA), 16, 0, QUANTIZED_REGS_TO_SAVE_LOAD, false, SAFE_LOADSTORE_NO_FASTMEM | SAFE_LOADSTORE_NO_PROLOG);
		ROR(16, R(RSCRATCH_EXTRA), Imm8(8));
	}
	else
	{
		UnsafeLoadRegToRegNoSwap(RSCRATCH_EXTRA, RSCRATCH_EXTRA, 16, 0);
	}
	MOVD_xmm(XMM0, R(RSCRATCH_EXTRA));
	if (cpu_info.bSSE4_1)
	{
		PMOVZXBD(XMM0, R(XMM0));
	}
	else
	{
		PXOR(XMM1, R(XMM1));
		PUNPCKLBW(XMM0, R(XMM1));
		PUNPCKLWD(XMM0, R(XMM1));
	}
	CVTDQ2PS(XMM0, R(XMM0));
	SHR(32, R(RSCRATCH2), Imm8(5));
	MOVQ_xmm(XMM1, MDisp(RSCRATCH2, (u32)(u64)m_dequantizeTableS));
	MULPS(XMM0, R(XMM1));
	RET();

	const u8* loadPairedU8One = AlignCode4();
	if (jit->jo.memcheck)
		SafeLoadToReg(RSCRATCH_EXTRA, R(RSCRATCH_EXTRA), 8, 0, QUANTIZED_REGS_TO_SAVE_LOAD, false, SAFE_LOADSTORE_NO_FASTMEM | SAFE_LOADSTORE_NO_PROLOG);
	else
		UnsafeLoadRegToRegNoSwap(RSCRATCH_EXTRA, RSCRATCH_EXTRA, 8, 0); // RSCRATCH_EXTRA = 0x000000xx
	CVTSI2SS(XMM0, R(RSCRATCH_EXTRA));
	SHR(32, R(RSCRATCH2), Imm8(5));
	MULSS(XMM0, MDisp(RSCRATCH2, (u32)(u64)m_dequantizeTableS));
	UNPCKLPS(XMM0, M(m_one));
	RET();

	const u8* loadPairedS8Two = AlignCode4();
	if (jit->jo.memcheck)
	{
		// TODO: Support not swapping in safeLoadToReg to avoid bswapping twice
		SafeLoadToReg(RSCRATCH_EXTRA, R(RSCRATCH_EXTRA), 16, 0, QUANTIZED_REGS_TO_SAVE_LOAD, false, SAFE_LOADSTORE_NO_FASTMEM | SAFE_LOADSTORE_NO_PROLOG);
		ROR(16, R(RSCRATCH_EXTRA), Imm8(8));
	}
	else
	{
		UnsafeLoadRegToRegNoSwap(RSCRATCH_EXTRA, RSCRATCH_EXTRA, 16, 0);
	}
	MOVD_xmm(XMM0, R(RSCRATCH_EXTRA));
	if (cpu_info.bSSE4_1)
	{
		PMOVSXBD(XMM0, R(XMM0));
	}
	else
	{
		PUNPCKLBW(XMM0, R(XMM0));
		PUNPCKLWD(XMM0, R(XMM0));
		PSRAD(XMM0, 24);
	}
	CVTDQ2PS(XMM0, R(XMM0));
	SHR(32, R(RSCRATCH2), Imm8(5));
	MOVQ_xmm(XMM1, MDisp(RSCRATCH2, (u32)(u64)m_dequantizeTableS));
	MULPS(XMM0, R(XMM1));
	RET();

	const u8* loadPairedS8One = AlignCode4();
	if (jit->jo.memcheck)
		SafeLoadToReg(RSCRATCH_EXTRA, R(RSCRATCH_EXTRA), 8, 0, QUANTIZED_REGS_TO_SAVE_LOAD, true, SAFE_LOADSTORE_NO_FASTMEM | SAFE_LOADSTORE_NO_PROLOG);
	else
		UnsafeLoadRegToRegNoSwap(RSCRATCH_EXTRA, RSCRATCH_EXTRA, 8, 0, true);
	CVTSI2SS(XMM0, R(RSCRATCH_EXTRA));
	SHR(32, R(RSCRATCH2), Imm8(5));
	MULSS(XMM0, MDisp(RSCRATCH2, (u32)(u64)m_dequantizeTableS));
	UNPCKLPS(XMM0, M(m_one));
	RET();

	const u8* loadPairedU16Two = AlignCode4();
	// TODO: Support not swapping in (un)safeLoadToReg to avoid bswapping twice
	if (jit->jo.memcheck)
		SafeLoadToReg(RSCRATCH_EXTRA, R(RSCRATCH_EXTRA), 32, 0, QUANTIZED_REGS_TO_SAVE_LOAD, false, SAFE_LOADSTORE_NO_FASTMEM | SAFE_LOADSTORE_NO_PROLOG);
	else
		UnsafeLoadRegToReg(RSCRATCH_EXTRA, RSCRATCH_EXTRA, 32, 0, false);
	ROL(32, R(RSCRATCH_EXTRA), Imm8(16));
	MOVD_xmm(XMM0, R(RSCRATCH_EXTRA));
	if (cpu_info.bSSE4_1)
	{
		PMOVZXWD(XMM0, R(XMM0));
	}
	else
	{
		PXOR(XMM1, R(XMM1));
		PUNPCKLWD(XMM0, R(XMM1));
	}
	CVTDQ2PS(XMM0, R(XMM0));
	SHR(32, R(RSCRATCH2), Imm8(5));
	MOVQ_xmm(XMM1, MDisp(RSCRATCH2, (u32)(u64)m_dequantizeTableS));
	MULPS(XMM0, R(XMM1));
	RET();

	const u8* loadPairedU16One = AlignCode4();
	if (jit->jo.memcheck)
		SafeLoadToReg(RSCRATCH_EXTRA, R(RSCRATCH_EXTRA), 16, 0, QUANTIZED_REGS_TO_SAVE_LOAD, false, SAFE_LOADSTORE_NO_FASTMEM | SAFE_LOADSTORE_NO_PROLOG);
	else
		UnsafeLoadRegToReg(RSCRATCH_EXTRA, RSCRATCH_EXTRA, 16, 0, false);
	CVTSI2SS(XMM0, R(RSCRATCH_EXTRA));
	SHR(32, R(RSCRATCH2), Imm8(5));
	MULSS(XMM0, MDisp(RSCRATCH2, (u32)(u64)m_dequantizeTableS));
	UNPCKLPS(XMM0, M(m_one));
	RET();

	const u8* loadPairedS16Two = AlignCode4();
	if (jit->jo.memcheck)
		SafeLoadToReg(RSCRATCH_EXTRA, R(RSCRATCH_EXTRA), 32, 0, QUANTIZED_REGS_TO_SAVE_LOAD, false, SAFE_LOADSTORE_NO_FASTMEM | SAFE_LOADSTORE_NO_PROLOG);
	else
		UnsafeLoadRegToReg(RSCRATCH_EXTRA, RSCRATCH_EXTRA, 32, 0, false);
	ROL(32, R(RSCRATCH_EXTRA), Imm8(16));
	MOVD_xmm(XMM0, R(RSCRATCH_EXTRA));
	if (cpu_info.bSSE4_1)
	{
		PMOVSXWD(XMM0, R(XMM0));
	}
	else
	{
		PUNPCKLWD(XMM0, R(XMM0));
		PSRAD(XMM0, 16);
	}
	CVTDQ2PS(XMM0, R(XMM0));
	SHR(32, R(RSCRATCH2), Imm8(5));
	MOVQ_xmm(XMM1, MDisp(RSCRATCH2, (u32)(u64)m_dequantizeTableS));
	MULPS(XMM0, R(XMM1));
	RET();

	const u8* loadPairedS16One = AlignCode4();
	if (jit->jo.memcheck)
		SafeLoadToReg(RSCRATCH_EXTRA, R(RSCRATCH_EXTRA), 16, 0, QUANTIZED_REGS_TO_SAVE_LOAD, true, SAFE_LOADSTORE_NO_FASTMEM | SAFE_LOADSTORE_NO_PROLOG);
	else
		UnsafeLoadRegToReg(RSCRATCH_EXTRA, RSCRATCH_EXTRA, 16, 0, true);
	CVTSI2SS(XMM0, R(RSCRATCH_EXTRA));
	SHR(32, R(RSCRATCH2), Imm8(5));
	MULSS(XMM0, MDisp(RSCRATCH2, (u32)(u64)m_dequantizeTableS));
	UNPCKLPS(XMM0, M(m_one));
	RET();


	JitRegister::Register(start, GetCodePtr(), "JIT_QuantizedLoad");

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
