// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"

#include "CPUDetect.h"
#include "JitBase.h"
#include "Jit_Util.h"

using namespace Gen;

static const u8 GC_ALIGNED16(pbswapShuffle1x4[16]) = {3, 2, 1, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
static u32 GC_ALIGNED16(float_buffer);

void EmuCodeBlock::UnsafeLoadRegToReg(X64Reg reg_addr, X64Reg reg_value, int accessSize, s32 offset, bool signExtend)
{
#ifdef _M_X64
	MOVZX(32, accessSize, reg_value, MComplex(RBX, reg_addr, SCALE_1, offset));
#else
	AND(32, R(reg_addr), Imm32(Memory::MEMVIEW32_MASK));
	MOVZX(32, accessSize, reg_value, MDisp(reg_addr, (u32)Memory::base + offset));
#endif
	if (accessSize == 32)
	{
		BSWAP(32, reg_value);
	}
	else if (accessSize == 16)
	{
		BSWAP(32, reg_value);
		if (signExtend)
			SAR(32, R(reg_value), Imm8(16));
		else
			SHR(32, R(reg_value), Imm8(16));
	}
	else if (signExtend)
	{
		// TODO: bake 8-bit into the original load.
		MOVSX(32, accessSize, reg_value, R(reg_value));
	}
}

void EmuCodeBlock::UnsafeLoadRegToRegNoSwap(X64Reg reg_addr, X64Reg reg_value, int accessSize, s32 offset)
{
#ifdef _M_X64
	MOVZX(32, accessSize, reg_value, MComplex(RBX, reg_addr, SCALE_1, offset));
#else
	AND(32, R(reg_addr), Imm32(Memory::MEMVIEW32_MASK));
	MOVZX(32, accessSize, reg_value, MDisp(reg_addr, (u32)Memory::base + offset));
#endif
}

u8 *EmuCodeBlock::UnsafeLoadToReg(X64Reg reg_value, Gen::OpArg opAddress, int accessSize, s32 offset, bool signExtend)
{
	u8 *result;
#ifdef _M_X64
	if (opAddress.IsSimpleReg())
	{
		// Deal with potential wraparound.  (This is just a heuristic, and it would
		// be more correct to actually mirror the first page at the end, but the
		// only case where it probably actually matters is JitIL turning adds into
		// offsets with the wrong sign, so whatever.  Since the original code
		// *could* try to wrap an address around, however, this is the correct
		// place to address the issue.)
		if ((u32) offset >= 0x1000) {
			LEA(32, reg_value, MDisp(opAddress.GetSimpleReg(), offset));
			opAddress = R(reg_value);
			offset = 0;
		}

		result = GetWritableCodePtr();
		MOVZX(32, accessSize, reg_value, MComplex(RBX, opAddress.GetSimpleReg(), SCALE_1, offset));
	}
	else
	{
		MOV(32, R(reg_value), opAddress);
		result = GetWritableCodePtr();
		MOVZX(32, accessSize, reg_value, MComplex(RBX, reg_value, SCALE_1, offset));
	}
#else
	if (opAddress.IsImm())
	{
		result = GetWritableCodePtr();
		MOVZX(32, accessSize, reg_value, M(Memory::base + (((u32)opAddress.offset + offset) & Memory::MEMVIEW32_MASK)));
	}
	else
	{
		if (!opAddress.IsSimpleReg(reg_value))
			MOV(32, R(reg_value), opAddress);
		AND(32, R(reg_value), Imm32(Memory::MEMVIEW32_MASK));
		result = GetWritableCodePtr();
		MOVZX(32, accessSize, reg_value, MDisp(reg_value, (u32)Memory::base + offset));
	}
#endif

	// Add a 2 bytes NOP to have some space for the backpatching
	if (accessSize == 8)
		NOP(2);

	if (accessSize == 32)
	{
		BSWAP(32, reg_value);
	}
	else if (accessSize == 16)
	{
		BSWAP(32, reg_value);
		if (signExtend)
			SAR(32, R(reg_value), Imm8(16));
		else
			SHR(32, R(reg_value), Imm8(16));
	}
	else if (signExtend)
	{
		// TODO: bake 8-bit into the original load.
		MOVSX(32, accessSize, reg_value, R(reg_value));
	}
	return result;
}

void EmuCodeBlock::SafeLoadToReg(X64Reg reg_value, const Gen::OpArg & opAddress, int accessSize, s32 offset, u32 registersInUse, bool signExtend, int flags)
{
	if (!jit->js.memcheck)
	{
		registersInUse &= ~(1 << RAX | 1 << reg_value);
	}
#if defined(_M_X64)
	if (!Core::g_CoreStartupParameter.bMMU &&
	    Core::g_CoreStartupParameter.bFastmem &&
	    !opAddress.IsImm() &&
	    !(flags & (SAFE_LOADSTORE_NO_SWAP | SAFE_LOADSTORE_NO_FASTMEM))
#ifdef ENABLE_MEM_CHECK
	    && !Core::g_CoreStartupParameter.bEnableDebugging
#endif
	    )
	{
		u8 *mov = UnsafeLoadToReg(reg_value, opAddress, accessSize, offset, signExtend);

		registersInUseAtLoc[mov] = registersInUse;
	}
	else
#endif
	{
		u32 mem_mask = Memory::ADDR_MASK_HW_ACCESS;
		if (Core::g_CoreStartupParameter.bMMU || Core::g_CoreStartupParameter.bTLBHack)
		{
			mem_mask |= Memory::ADDR_MASK_MEM1;
		}

#ifdef ENABLE_MEM_CHECK
		if (Core::g_CoreStartupParameter.bEnableDebugging)
		{
			mem_mask |= Memory::EXRAM_MASK;
		}
#endif

		if (opAddress.IsImm())
		{
			u32 address = (u32)opAddress.offset + offset;
			if ((address & mem_mask) == 0)
			{
				UnsafeLoadToReg(reg_value, opAddress, accessSize, offset, signExtend);
			}
			else
			{
				ABI_PushRegistersAndAdjustStack(registersInUse, false);
				switch (accessSize)
				{
				case 32: ABI_CallFunctionC((void *)&Memory::Read_U32, address); break;
				case 16: ABI_CallFunctionC((void *)&Memory::Read_U16_ZX, address); break;
				case 8:  ABI_CallFunctionC((void *)&Memory::Read_U8_ZX, address); break;
				}
				ABI_PopRegistersAndAdjustStack(registersInUse, false);

				MEMCHECK_START

				if (signExtend && accessSize < 32)
				{
					// Need to sign extend values coming from the Read_U* functions.
					MOVSX(32, accessSize, reg_value, R(EAX));
				}
				else if (reg_value != EAX)
				{
					MOVZX(32, accessSize, reg_value, R(EAX));
				}

				MEMCHECK_END
			}
		}
		else
		{
			if (offset)
			{
				MOV(32, R(EAX), opAddress);
				ADD(32, R(EAX), Imm32(offset));
				TEST(32, R(EAX), Imm32(mem_mask));
				FixupBranch fast = J_CC(CC_Z, true);

				ABI_PushRegistersAndAdjustStack(registersInUse, false);
				switch (accessSize)
				{
				case 32: ABI_CallFunctionR((void *)&Memory::Read_U32, EAX); break;
				case 16: ABI_CallFunctionR((void *)&Memory::Read_U16_ZX, EAX); break;
				case 8:  ABI_CallFunctionR((void *)&Memory::Read_U8_ZX, EAX);  break;
				}
				ABI_PopRegistersAndAdjustStack(registersInUse, false);

				MEMCHECK_START

				if (signExtend && accessSize < 32)
				{
					// Need to sign extend values coming from the Read_U* functions.
					MOVSX(32, accessSize, reg_value, R(EAX));
				}
				else if (reg_value != EAX)
				{
					MOVZX(32, accessSize, reg_value, R(EAX));
				}

				MEMCHECK_END

				FixupBranch exit = J();
				SetJumpTarget(fast);
				UnsafeLoadToReg(reg_value, R(EAX), accessSize, 0, signExtend);
				SetJumpTarget(exit);
			}
			else
			{
				TEST(32, opAddress, Imm32(mem_mask));
				FixupBranch fast = J_CC(CC_Z, true);

				ABI_PushRegistersAndAdjustStack(registersInUse, false);
				switch (accessSize)
				{
				case 32: ABI_CallFunctionA((void *)&Memory::Read_U32, opAddress); break;
				case 16: ABI_CallFunctionA((void *)&Memory::Read_U16_ZX, opAddress); break;
				case 8:  ABI_CallFunctionA((void *)&Memory::Read_U8_ZX, opAddress);  break;
				}
				ABI_PopRegistersAndAdjustStack(registersInUse, false);

				MEMCHECK_START

				if (signExtend && accessSize < 32)
				{
					// Need to sign extend values coming from the Read_U* functions.
					MOVSX(32, accessSize, reg_value, R(EAX));
				}
				else if (reg_value != EAX)
				{
					MOVZX(32, accessSize, reg_value, R(EAX));
				}

				MEMCHECK_END

				FixupBranch exit = J();
				SetJumpTarget(fast);
				UnsafeLoadToReg(reg_value, opAddress, accessSize, offset, signExtend);
				SetJumpTarget(exit);
			}
		}
	}
}

u8 *EmuCodeBlock::UnsafeWriteRegToReg(X64Reg reg_value, X64Reg reg_addr, int accessSize, s32 offset, bool swap)
{
	u8 *result;
	if (accessSize == 8 && reg_value >= 4) {
		PanicAlert("WARNING: likely incorrect use of UnsafeWriteRegToReg!");
	}
	if (swap) BSWAP(accessSize, reg_value);
#ifdef _M_X64
	result = GetWritableCodePtr();
	MOV(accessSize, MComplex(RBX, reg_addr, SCALE_1, offset), R(reg_value));
#else
	AND(32, R(reg_addr), Imm32(Memory::MEMVIEW32_MASK));
	result = GetWritableCodePtr();
	MOV(accessSize, MDisp(reg_addr, (u32)Memory::base + offset), R(reg_value));
#endif
	return result;
}

// Destroys both arg registers
void EmuCodeBlock::SafeWriteRegToReg(X64Reg reg_value, X64Reg reg_addr, int accessSize, s32 offset, u32 registersInUse, int flags)
{
	registersInUse &= ~(1 << RAX);
#if defined(_M_X64)
	if (!Core::g_CoreStartupParameter.bMMU &&
	    Core::g_CoreStartupParameter.bFastmem &&
	    !(flags & (SAFE_LOADSTORE_NO_SWAP | SAFE_LOADSTORE_NO_FASTMEM))
#ifdef ENABLE_MEM_CHECK
	    && !Core::g_CoreStartupParameter.bEnableDebugging
#endif
	    )
	{
		MOV(32, M(&PC), Imm32(jit->js.compilerPC)); // Helps external systems know which instruction triggered the write
		u8 *mov = UnsafeWriteRegToReg(reg_value, reg_addr, accessSize, offset, !(flags & SAFE_LOADSTORE_NO_SWAP));
		if (accessSize == 8)
		{
			NOP(1);
			NOP(1);
		}

		registersInUseAtLoc[mov] = registersInUse;
		return;
	}
#endif

	if (offset)
		ADD(32, R(reg_addr), Imm32((u32)offset));

	u32 mem_mask = Memory::ADDR_MASK_HW_ACCESS;

	if (Core::g_CoreStartupParameter.bMMU || Core::g_CoreStartupParameter.bTLBHack)
	{
		mem_mask |= Memory::ADDR_MASK_MEM1;
	}

#ifdef ENABLE_MEM_CHECK
	if (Core::g_CoreStartupParameter.bEnableDebugging)
	{
		mem_mask |= Memory::EXRAM_MASK;
	}
#endif

	MOV(32, M(&PC), Imm32(jit->js.compilerPC)); // Helps external systems know which instruction triggered the write
	TEST(32, R(reg_addr), Imm32(mem_mask));
	FixupBranch fast = J_CC(CC_Z, true);
	bool noProlog = (0 != (flags & SAFE_LOADSTORE_NO_PROLOG));
	bool swap = !(flags & SAFE_LOADSTORE_NO_SWAP);
	ABI_PushRegistersAndAdjustStack(registersInUse, noProlog);
	switch (accessSize)
	{
	case 32: ABI_CallFunctionRR(swap ? ((void *)&Memory::Write_U32) : ((void *)&Memory::Write_U32_Swap), reg_value, reg_addr, false); break;
	case 16: ABI_CallFunctionRR(swap ? ((void *)&Memory::Write_U16) : ((void *)&Memory::Write_U16_Swap), reg_value, reg_addr, false); break;
	case 8:  ABI_CallFunctionRR((void *)&Memory::Write_U8, reg_value, reg_addr, false);  break;
	}
	ABI_PopRegistersAndAdjustStack(registersInUse, noProlog);
	FixupBranch exit = J();
	SetJumpTarget(fast);
	UnsafeWriteRegToReg(reg_value, reg_addr, accessSize, 0, swap);
	SetJumpTarget(exit);
}

void EmuCodeBlock::SafeWriteFloatToReg(X64Reg xmm_value, X64Reg reg_addr, u32 registersInUse, int flags)
{
	if (false && cpu_info.bSSSE3) {
		// This path should be faster but for some reason it causes errors so I've disabled it.
		u32 mem_mask = Memory::ADDR_MASK_HW_ACCESS;

		if (Core::g_CoreStartupParameter.bMMU || Core::g_CoreStartupParameter.bTLBHack)
			mem_mask |= Memory::ADDR_MASK_MEM1;

#ifdef ENABLE_MEM_CHECK
		if (Core::g_CoreStartupParameter.bEnableDebugging)
			mem_mask |= Memory::EXRAM_MASK;
#endif
		TEST(32, R(reg_addr), Imm32(mem_mask));
		FixupBranch argh = J_CC(CC_Z);
		MOVSS(M(&float_buffer), xmm_value);
		MOV(32, R(EAX), M(&float_buffer));
		BSWAP(32, EAX);
		MOV(32, M(&PC), Imm32(jit->js.compilerPC)); // Helps external systems know which instruction triggered the write
		ABI_PushRegistersAndAdjustStack(registersInUse, false);
		ABI_CallFunctionRR((void *)&Memory::Write_U32, EAX, reg_addr);
		ABI_PopRegistersAndAdjustStack(registersInUse, false);
		FixupBranch arg2 = J();
		SetJumpTarget(argh);
		PSHUFB(xmm_value, M((void *)pbswapShuffle1x4));
#ifdef _M_X64
		MOVD_xmm(MComplex(RBX, reg_addr, SCALE_1, 0), xmm_value);
#else
		AND(32, R(reg_addr), Imm32(Memory::MEMVIEW32_MASK));
		MOVD_xmm(MDisp(reg_addr, (u32)Memory::base), xmm_value);
#endif
		SetJumpTarget(arg2);
	} else {
		MOVSS(M(&float_buffer), xmm_value);
		MOV(32, R(EAX), M(&float_buffer));
		SafeWriteRegToReg(EAX, reg_addr, 32, 0, registersInUse, flags);
	}
}

void EmuCodeBlock::WriteToConstRamAddress(int accessSize, const Gen::OpArg& arg, u32 address)
{
#ifdef _M_X64
 	MOV(accessSize, MDisp(RBX, address & 0x3FFFFFFF), arg);
#else
	MOV(accessSize, M((void*)(Memory::base + (address & Memory::MEMVIEW32_MASK))), arg);
#endif
}

void EmuCodeBlock::WriteFloatToConstRamAddress(const Gen::X64Reg& xmm_reg, u32 address)
{
#ifdef _M_X64
	MOV(32, R(RAX), Imm32(address));
	MOVSS(MComplex(RBX, RAX, 1, 0), xmm_reg);
#else
	MOVSS(M((void*)((u32)Memory::base + (address & Memory::MEMVIEW32_MASK))), xmm_reg);
#endif
}

void EmuCodeBlock::ForceSinglePrecisionS(X64Reg xmm) {
	// Most games don't need these. Zelda requires it though - some platforms get stuck without them.
	if (jit->jo.accurateSinglePrecision)
	{
		CVTSD2SS(xmm, R(xmm));
		CVTSS2SD(xmm, R(xmm));
	}
}

void EmuCodeBlock::ForceSinglePrecisionP(X64Reg xmm) {
	// Most games don't need these. Zelda requires it though - some platforms get stuck without them.
	if (jit->jo.accurateSinglePrecision)
	{
		CVTPD2PS(xmm, R(xmm));
		CVTPS2PD(xmm, R(xmm));
	}
}

static u32 GC_ALIGNED16(temp32);
static u64 GC_ALIGNED16(temp64);
#ifdef _WIN32
#include <intrin.h>
#ifdef _M_X64
static const __m128i GC_ALIGNED16(single_qnan_bit) = _mm_set_epi64x(0, 0x0000000000400000);
static const __m128i GC_ALIGNED16(single_exponent) = _mm_set_epi64x(0, 0x000000007f800000);
static const __m128i GC_ALIGNED16(double_qnan_bit) = _mm_set_epi64x(0, 0x0008000000000000);
static const __m128i GC_ALIGNED16(double_exponent) = _mm_set_epi64x(0, 0x7ff0000000000000);
#else
static const __m128i GC_ALIGNED16(single_qnan_bit) = _mm_set_epi32(0, 0, 0x00000000, 0x00400000);
static const __m128i GC_ALIGNED16(single_exponent) = _mm_set_epi32(0, 0, 0x00000000, 0x7f800000);
static const __m128i GC_ALIGNED16(double_qnan_bit) = _mm_set_epi32(0, 0, 0x00080000, 0x00000000);
static const __m128i GC_ALIGNED16(double_exponent) = _mm_set_epi32(0, 0, 0x7ff00000, 0x00000000);
#endif
#else
static const __uint128_t GC_ALIGNED16(single_qnan_bit) = 0x0000000000400000;
static const __uint128_t GC_ALIGNED16(single_exponent) = 0x000000007f800000;
static const __uint128_t GC_ALIGNED16(double_qnan_bit) = 0x0008000000000000;
static const __uint128_t GC_ALIGNED16(double_exponent) = 0x7ff0000000000000;
#endif

// Since the following float conversion functions are used in non-arithmetic PPC float instructions,
// they must convert floats bitexact and never flush denormals to zero or turn SNaNs into QNaNs.
// This means we can't use CVTSS2SD/CVTSD2SS :(
// The x87 FPU doesn't even support flush-to-zero so we can use FLD+FSTP even on denormals.
// If the number is a NaN, make sure to set the QNaN bit back to its original value.

// Another problem is that officially, converting doubles to single format results in undefined behavior.
// Relying on undefined behavior is a bug so no software should ever do this.
// In case it does happen, phire's more accurate implementation of ConvertDoubleToSingle() is reproduced below.

//#define MORE_ACCURATE_DOUBLETOSINGLE
#ifdef MORE_ACCURATE_DOUBLETOSINGLE

#ifdef _WIN32
#ifdef _M_X64
static const __m128i GC_ALIGNED16(double_fraction) = _mm_set_epi64x(0, 0x000fffffffffffff);
static const __m128i GC_ALIGNED16(double_sign_bit) = _mm_set_epi64x(0, 0x8000000000000000);
static const __m128i GC_ALIGNED16(double_explicit_top_bit) = _mm_set_epi64x(0, 0x0010000000000000);
static const __m128i GC_ALIGNED16(double_top_two_bits) = _mm_set_epi64x(0, 0xc000000000000000);
static const __m128i GC_ALIGNED16(double_bottom_bits)  = _mm_set_epi64x(0, 0x07ffffffe0000000);
#else
static const __m128i GC_ALIGNED16(double_fraction) = _mm_set_epi32(0, 0, 0x000fffff, 0xffffffff);
static const __m128i GC_ALIGNED16(double_sign_bit) = _mm_set_epi32(0, 0, 0x80000000, 0x00000000);
static const __m128i GC_ALIGNED16(double_explicit_top_bit) = _mm_set_epi32(0, 0, 0x00100000, 0x00000000);
static const __m128i GC_ALIGNED16(double_top_two_bits) = _mm_set_epi32(0, 0, 0xc0000000, 0x00000000);
static const __m128i GC_ALIGNED16(double_bottom_bits)  = _mm_set_epi32(0, 0, 0x07ffffff, 0xe0000000);
#endif
#else
static const __uint128_t GC_ALIGNED16(double_fraction) = 0x000fffffffffffff;
static const __uint128_t GC_ALIGNED16(double_sign_bit) = 0x8000000000000000;
static const __uint128_t GC_ALIGNED16(double_explicit_top_bit) = 0x0010000000000000;
static const __uint128_t GC_ALIGNED16(double_top_two_bits) = 0xc000000000000000;
static const __uint128_t GC_ALIGNED16(double_bottom_bits)  = 0x07ffffffe0000000;
#endif

// This is the same algorithm used in the interpreter (and actual hardware)
// The documentation states that the conversion of a double with an outside the
// valid range for a single (or a single denormal) is undefined.
// But testing on actual hardware shows it always picks bits 0..1 and 5..34
// unless the exponent is in the range of 874 to 896.
void EmuCodeBlock::ConvertDoubleToSingle(X64Reg dst, X64Reg src)
{
	MOVSD(XMM1, R(src));

	// Grab Exponent
	PAND(XMM1, M((void *)&double_exponent));
	PSRLQ(XMM1, 52);
	MOVD_xmm(R(EAX), XMM1);


	// Check if the double is in the range of valid single subnormal
	CMP(16, R(EAX), Imm16(896));
	FixupBranch NoDenormalize = J_CC(CC_G);
	CMP(16, R(EAX), Imm16(874));
	FixupBranch NoDenormalize2 = J_CC(CC_L);

	// Denormalise

	// shift = (905 - Exponent) plus the 21 bit double to single shift
	MOV(16, R(EAX), Imm16(905 + 21));
	MOVD_xmm(XMM0, R(EAX));
	PSUBQ(XMM0, R(XMM1));

	// xmm1 = fraction | 0x0010000000000000
	MOVSD(XMM1, R(src));
	PAND(XMM1, M((void *)&double_fraction));
	POR(XMM1, M((void *)&double_explicit_top_bit));

	// fraction >> shift
	PSRLQ(XMM1, R(XMM0));

	// OR the sign bit in.
	MOVSD(XMM0, R(src));
	PAND(XMM0, M((void *)&double_sign_bit));
	PSRLQ(XMM0, 32);
	POR(XMM1, R(XMM0));

	FixupBranch end = J(false); // Goto end

	SetJumpTarget(NoDenormalize);
	SetJumpTarget(NoDenormalize2);

	// Don't Denormalize

	// We want bits 0, 1
	MOVSD(XMM1, R(src));
	PAND(XMM1, M((void *)&double_top_two_bits));
	PSRLQ(XMM1, 32);

	// And 5 through to 34
	MOVSD(XMM0, R(src));
	PAND(XMM0, M((void *)&double_bottom_bits));
	PSRLQ(XMM0, 29);

	// OR them togther
	POR(XMM1, R(XMM0));

	// End
	SetJumpTarget(end);
	MOVDDUP(dst, R(XMM1));
}

#else // MORE_ACCURATE_DOUBLETOSINGLE

void EmuCodeBlock::ConvertDoubleToSingle(X64Reg dst, X64Reg src)
{
	MOVSD(M(&temp64), src);
	MOVSD(XMM1, R(src));
	FLD(64, M(&temp64));
	CCFlags cond;
	if (cpu_info.bSSE4_1) {
		PTEST(XMM1, M((void *)&double_exponent));
		cond = CC_NC;
	} else {
		FNSTSW_AX();
		TEST(16, R(AX), Imm16(x87_InvalidOperation));
		cond = CC_Z;
	}
	FSTP(32, M(&temp32));
	MOVSS(XMM0, M(&temp32));
	FixupBranch dont_reset_qnan_bit = J_CC(cond);

	PANDN(XMM1, M((void *)&double_qnan_bit));
	PSRLQ(XMM1, 29);
	if (cpu_info.bAVX) {
		VPANDN(XMM0, XMM1, R(XMM0));
	} else {
		PANDN(XMM1, R(XMM0));
		MOVSS(XMM0, R(XMM1));
	}

	SetJumpTarget(dont_reset_qnan_bit);
	MOVDDUP(dst, R(XMM0));
}
#endif // MORE_ACCURATE_DOUBLETOSINGLE

void EmuCodeBlock::ConvertSingleToDouble(X64Reg dst, X64Reg src, bool src_is_gpr)
{
	if (src_is_gpr) {
		MOV(32, M(&temp32), R(src));
		MOVD_xmm(XMM1, R(src));
	} else {
		MOVSS(M(&temp32), src);
		MOVSS(R(XMM1), src);
	}
	FLD(32, M(&temp32));
	CCFlags cond;
	if (cpu_info.bSSE4_1) {
		PTEST(XMM1, M((void *)&single_exponent));
		cond = CC_NC;
	} else {
		FNSTSW_AX();
		TEST(16, R(AX), Imm16(x87_InvalidOperation));
		cond = CC_Z;
	}
	FSTP(64, M(&temp64));
	MOVSD(dst, M(&temp64));
	FixupBranch dont_reset_qnan_bit = J_CC(cond);

	PANDN(XMM1, M((void *)&single_qnan_bit));
	PSLLQ(XMM1, 29);
	if (cpu_info.bAVX) {
		VPANDN(dst, XMM1, R(dst));
	} else {
		PANDN(XMM1, R(dst));
		MOVSD(dst, R(XMM1));
	}

	SetJumpTarget(dont_reset_qnan_bit);
	MOVDDUP(dst, R(dst));
}

void EmuCodeBlock::JitClearCA()
{
	AND(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(~XER_CA_MASK)); //XER.CA = 0
}

void EmuCodeBlock::JitSetCA()
{
	OR(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(XER_CA_MASK)); //XER.CA = 1
}

void EmuCodeBlock::JitClearCAOV(bool oe)
{
	if (oe)
		AND(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(~XER_CA_MASK & ~XER_OV_MASK)); //XER.CA, XER.OV = 0
	else
		AND(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(~XER_CA_MASK)); //XER.CA = 0
}
