// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/Intrinsics.h"
#include "Common/MathUtil.h"

#include "Core/HW/MMIO.h"
#include "Core/PowerPC/JitCommon/Jit_Util.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

using namespace Gen;

void EmuCodeBlock::MemoryExceptionCheck()
{
	if (jit->js.generatingTrampoline)
	{
		if (jit->js.trampolineExceptionHandler)
		{
			TEST(32, PPCSTATE(Exceptions), Gen::Imm32(EXCEPTION_DSI));
			J_CC(CC_NZ, jit->js.trampolineExceptionHandler);
		}
		return;
	}

	if (jit->jo.memcheck && !jit->js.fastmemLoadStore && !jit->js.fixupExceptionHandler)
	{
		TEST(32, PPCSTATE(Exceptions), Gen::Imm32(EXCEPTION_DSI));
		jit->js.exceptionHandler = J_CC(Gen::CC_NZ, true);
		jit->js.fixupExceptionHandler = true;
	}
}

void EmuCodeBlock::UnsafeLoadRegToReg(X64Reg reg_addr, X64Reg reg_value, int accessSize, s32 offset, bool signExtend)
{
	OpArg src = MComplex(RMEM, reg_addr, SCALE_1, offset);
	LoadAndSwap(accessSize, reg_value, src, signExtend);
}

void EmuCodeBlock::UnsafeLoadRegToRegNoSwap(X64Reg reg_addr, X64Reg reg_value, int accessSize, s32 offset, bool signExtend)
{
	if (signExtend)
		MOVSX(32, accessSize, reg_value, MComplex(RMEM, reg_addr, SCALE_1, offset));
	else
		MOVZX(32, accessSize, reg_value, MComplex(RMEM, reg_addr, SCALE_1, offset));
}

void EmuCodeBlock::UnsafeLoadToReg(X64Reg reg_value, OpArg opAddress, int accessSize, s32 offset, bool signExtend, MovInfo* info)
{
	OpArg memOperand;
	if (opAddress.IsSimpleReg())
	{
		// Deal with potential wraparound.  (This is just a heuristic, and it would
		// be more correct to actually mirror the first page at the end, but the
		// only case where it probably actually matters is JitIL turning adds into
		// offsets with the wrong sign, so whatever.  Since the original code
		// *could* try to wrap an address around, however, this is the correct
		// place to address the issue.)
		if ((u32) offset >= 0x1000)
		{
			LEA(32, reg_value, MDisp(opAddress.GetSimpleReg(), offset));
			opAddress = R(reg_value);
			offset = 0;
		}
		memOperand = MComplex(RMEM, opAddress.GetSimpleReg(), SCALE_1, offset);
	}
	else if (opAddress.IsImm())
	{
		MOV(32, R(reg_value), Imm32((u32)(opAddress.Imm32() + offset)));
		memOperand = MRegSum(RMEM, reg_value);
	}
	else
	{
		MOV(32, R(reg_value), opAddress);
		memOperand = MComplex(RMEM, reg_value, SCALE_1, offset);
	}

	LoadAndSwap(accessSize, reg_value, memOperand, signExtend, info);
}

// Visitor that generates code to read a MMIO value.
template <typename T>
class MMIOReadCodeGenerator : public MMIO::ReadHandlingMethodVisitor<T>
{
public:
	MMIOReadCodeGenerator(Gen::X64CodeBlock* code, BitSet32 registers_in_use,
	                      Gen::X64Reg dst_reg, u32 address, bool sign_extend)
		: m_code(code), m_registers_in_use(registers_in_use), m_dst_reg(dst_reg),
		  m_address(address), m_sign_extend(sign_extend)
	{
	}

	void VisitConstant(T value) override
	{
		LoadConstantToReg(8 * sizeof (T), value);
	}
	void VisitDirect(const T* addr, u32 mask) override
	{
		LoadAddrMaskToReg(8 * sizeof (T), addr, mask);
	}
	void VisitComplex(const std::function<T(u32)>* lambda) override
	{
		CallLambda(8 * sizeof (T), lambda);
	}

private:
	// Generates code to load a constant to the destination register. In
	// practice it would be better to avoid using a register for this, but it
	// would require refactoring a lot of JIT code.
	void LoadConstantToReg(int sbits, u32 value)
	{
		if (m_sign_extend)
		{
			u32 sign = !!(value & (1 << (sbits - 1)));
			value |= sign * ((0xFFFFFFFF >> sbits) << sbits);
		}
		m_code->MOV(32, R(m_dst_reg), Gen::Imm32(value));
	}

	// Generate the proper MOV instruction depending on whether the read should
	// be sign extended or zero extended.
	void MoveOpArgToReg(int sbits, const Gen::OpArg& arg)
	{
		if (m_sign_extend)
			m_code->MOVSX(32, sbits, m_dst_reg, arg);
		else
			m_code->MOVZX(32, sbits, m_dst_reg, arg);
	}

	void LoadAddrMaskToReg(int sbits, const void* ptr, u32 mask)
	{
		m_code->MOV(64, R(RSCRATCH), ImmPtr(ptr));
		// If we do not need to mask, we can do the sign extend while loading
		// from memory. If masking is required, we have to first zero extend,
		// then mask, then sign extend if needed (1 instr vs. 2/3).
		u32 all_ones = (1ULL << sbits) - 1;
		if ((all_ones & mask) == all_ones)
		{
			MoveOpArgToReg(sbits, MatR(RSCRATCH));
		}
		else
		{
			m_code->MOVZX(32, sbits, m_dst_reg, MatR(RSCRATCH));
			m_code->AND(32, R(m_dst_reg), Imm32(mask));
			if (m_sign_extend)
				m_code->MOVSX(32, sbits, m_dst_reg, R(m_dst_reg));
		}
	}

	void CallLambda(int sbits, const std::function<T(u32)>* lambda)
	{
		m_code->ABI_PushRegistersAndAdjustStack(m_registers_in_use, 0);
		m_code->ABI_CallLambdaC(lambda, m_address);
		m_code->ABI_PopRegistersAndAdjustStack(m_registers_in_use, 0);
		MoveOpArgToReg(sbits, R(ABI_RETURN));
	}

	Gen::X64CodeBlock* m_code;
	BitSet32 m_registers_in_use;
	Gen::X64Reg m_dst_reg;
	u32 m_address;
	bool m_sign_extend;
};

void EmuCodeBlock::MMIOLoadToReg(MMIO::Mapping* mmio, Gen::X64Reg reg_value,
                                 BitSet32 registers_in_use, u32 address,
                                 int access_size, bool sign_extend)
{
	switch (access_size)
	{
	case 8:
		{
			MMIOReadCodeGenerator<u8> gen(this, registers_in_use, reg_value,
			                              address, sign_extend);
			mmio->GetHandlerForRead<u8>(address).Visit(gen);
			break;
		}
	case 16:
		{
			MMIOReadCodeGenerator<u16> gen(this, registers_in_use, reg_value,
			                               address, sign_extend);
			mmio->GetHandlerForRead<u16>(address).Visit(gen);
			break;
		}
	case 32:
		{
			MMIOReadCodeGenerator<u32> gen(this, registers_in_use, reg_value,
			                               address, sign_extend);
			mmio->GetHandlerForRead<u32>(address).Visit(gen);
			break;
		}
	}
}

FixupBranch EmuCodeBlock::CheckIfSafeAddress(const OpArg& reg_value, X64Reg reg_addr, BitSet32 registers_in_use, u32 mem_mask)
{
	registers_in_use[reg_addr] = true;
	if (reg_value.IsSimpleReg())
		registers_in_use[reg_value.GetSimpleReg()] = true;

	// Get ourselves a free register; try to pick one that doesn't involve pushing, if we can.
	X64Reg scratch = RSCRATCH;
	if (!registers_in_use[RSCRATCH])
		scratch = RSCRATCH;
	else if (!registers_in_use[RSCRATCH_EXTRA])
		scratch = RSCRATCH_EXTRA;
	else
		scratch = reg_addr;

	// On Gamecube games with MMU, do a little bit of extra work to make sure we're not accessing the
	// 0x81800000 to 0x83FFFFFF range.
	// It's okay to take a shortcut and not check this range on non-MMU games, since we're already
	// assuming they'll never do an invalid memory access.
	// The slightly more complex check needed for Wii games using the space just above MEM1 isn't
	// implemented here yet, since there are no known working Wii MMU games to test it with.
	if (jit->jo.memcheck && !SConfig::GetInstance().bWii)
	{
		if (scratch == reg_addr)
			PUSH(scratch);
		else
			MOV(32, R(scratch), R(reg_addr));
		AND(32, R(scratch), Imm32(0x3FFFFFFF));
		CMP(32, R(scratch), Imm32(0x01800000));
		if (scratch == reg_addr)
			POP(scratch);
		return J_CC(CC_AE, farcode.Enabled());
	}
	else
	{
		TEST(32, R(reg_addr), Imm32(mem_mask));
		return J_CC(CC_NZ, farcode.Enabled());
	}
}

void EmuCodeBlock::SafeLoadToReg(X64Reg reg_value, const Gen::OpArg & opAddress, int accessSize, s32 offset, BitSet32 registersInUse, bool signExtend, int flags)
{
	bool slowmem = (flags & SAFE_LOADSTORE_FORCE_SLOWMEM) != 0;

	registersInUse[reg_value] = false;
	if (jit->jo.fastmem && !(flags & SAFE_LOADSTORE_NO_FASTMEM) && !slowmem)
	{
		u8* backpatchStart = GetWritableCodePtr();
		MovInfo mov;
		UnsafeLoadToReg(reg_value, opAddress, accessSize, offset, signExtend, &mov);
		TrampolineInfo& info = backPatchInfo[mov.address];
		info.pc = jit->js.compilerPC;
		info.mov = mov;
		info.start = backpatchStart;
		info.read = true;
		info.op_reg = reg_value;
		info.op_arg = opAddress;
		info.accessSize = accessSize;
		info.offset = offset;
		info.registersInUse = registersInUse;
		info.flags = flags;
		info.signExtend = signExtend;
		ptrdiff_t padding = BACKPATCH_SIZE - (GetCodePtr() - backpatchStart);
		if (padding > 0)
		{
			NOP(padding);
		}
		info.end = GetCodePtr();

		jit->js.fastmemLoadStore = mov.address;
		return;
	}

	if (opAddress.IsImm())
	{
		SafeLoadToRegImmediate(reg_value, opAddress, accessSize, offset, registersInUse, signExtend, slowmem);
		return;
	}

	_assert_msg_(DYNA_REC, opAddress.IsSimpleReg(), "Incorrect use of SafeLoadToReg (address isn't register or immediate)");
	X64Reg reg_addr = opAddress.GetSimpleReg();
	if (offset)
	{
		reg_addr = RSCRATCH;
		LEA(32, RSCRATCH, MDisp(opAddress.GetSimpleReg(), offset));
	}

	FixupBranch exit;
	if (!jit->jo.alwaysUseMemFuncs && !slowmem)
	{
		u32 mem_mask = Memory::ADDR_MASK_HW_ACCESS;

		// The following masks the region used by the GC/Wii virtual memory lib
		mem_mask |= Memory::ADDR_MASK_MEM1;

		FixupBranch slow = CheckIfSafeAddress(R(reg_value), reg_addr, registersInUse, mem_mask);
		UnsafeLoadToReg(reg_value, R(reg_addr), accessSize, 0, signExtend);
		if (farcode.Enabled())
			SwitchToFarCode();
		else
			exit = J(true);
		SetJumpTarget(slow);
	}
	size_t rsp_alignment = (flags & SAFE_LOADSTORE_NO_PROLOG) ? 8 : 0;
	ABI_PushRegistersAndAdjustStack(registersInUse, rsp_alignment);
	switch (accessSize)
	{
	case 64:
		ABI_CallFunctionR((void *)&PowerPC::Read_U64, reg_addr);
		break;
	case 32:
		ABI_CallFunctionR((void *)&PowerPC::Read_U32, reg_addr);
		break;
	case 16:
		ABI_CallFunctionR((void *)&PowerPC::Read_U16_ZX, reg_addr);
		break;
	case 8:
		ABI_CallFunctionR((void *)&PowerPC::Read_U8_ZX, reg_addr);
		break;
	}
	ABI_PopRegistersAndAdjustStack(registersInUse, rsp_alignment);

	MemoryExceptionCheck();
	if (signExtend && accessSize < 32)
	{
		// Need to sign extend values coming from the Read_U* functions.
		MOVSX(32, accessSize, reg_value, R(ABI_RETURN));
	}
	else if (reg_value != ABI_RETURN)
	{
		MOVZX(64, accessSize, reg_value, R(ABI_RETURN));
	}

	if (!jit->jo.alwaysUseMemFuncs && !slowmem)
	{
		if (farcode.Enabled())
		{
			exit = J(true);
			SwitchToNearCode();
		}
		SetJumpTarget(exit);
	}
}

void EmuCodeBlock::SafeLoadToRegImmediate(X64Reg reg_value, const Gen::OpArg & opAddress, int accessSize, s32 offset, BitSet32 registersInUse, bool signExtend, bool slowmem)
{
	u32 address = opAddress.Imm32() + offset;

	if (!slowmem)
	{
		// If the address is known to be RAM, just load it directly.
		if (PowerPC::IsOptimizableRAMAddress(address))
		{
			UnsafeLoadToReg(reg_value, opAddress, accessSize, offset, signExtend);
			return;
		}

		// If the address maps to an MMIO register, inline MMIO read code.
		u32 mmioAddress = PowerPC::IsOptimizableMMIOAccess(address, accessSize);
		if (accessSize != 64 && mmioAddress)
		{
			MMIOLoadToReg(Memory::mmio_mapping, reg_value, registersInUse,
			              mmioAddress, accessSize, signExtend);
			return;
		}
	}

	// Fall back to general-case code.
	ABI_PushRegistersAndAdjustStack(registersInUse, 0);
	switch (accessSize)
	{
	case 64: ABI_CallFunctionC((void *)&PowerPC::Read_U64, address); break;
	case 32: ABI_CallFunctionC((void *)&PowerPC::Read_U32, address); break;
	case 16: ABI_CallFunctionC((void *)&PowerPC::Read_U16_ZX, address); break;
	case 8:  ABI_CallFunctionC((void *)&PowerPC::Read_U8_ZX, address); break;
	}
	ABI_PopRegistersAndAdjustStack(registersInUse, 0);

	MemoryExceptionCheck();
	if (signExtend && accessSize < 32)
	{
		// Need to sign extend values coming from the Read_U* functions.
		MOVSX(32, accessSize, reg_value, R(ABI_RETURN));
	}
	else if (reg_value != ABI_RETURN)
	{
		MOVZX(64, accessSize, reg_value, R(ABI_RETURN));
	}

	return;
}

static OpArg SwapImmediate(int accessSize, const OpArg& reg_value)
{
	if (accessSize == 32)
		return Imm32(Common::swap32(reg_value.Imm32()));
	else if (accessSize == 16)
		return Imm16(Common::swap16(reg_value.Imm16()));
	else
		return Imm8(reg_value.Imm8());
}

void EmuCodeBlock::UnsafeWriteRegToReg(OpArg reg_value, X64Reg reg_addr, int accessSize, s32 offset, bool swap, MovInfo* info)
{
	if (info)
	{
		info->address = GetWritableCodePtr();
		info->nonAtomicSwapStore = false;
	}

	OpArg dest = MComplex(RMEM, reg_addr, SCALE_1, offset);
	if (reg_value.IsImm())
	{
		if (swap)
			reg_value = SwapImmediate(accessSize, reg_value);
		MOV(accessSize, dest, reg_value);
	}
	else if (swap)
	{
		SwapAndStore(accessSize, dest, reg_value.GetSimpleReg(), info);
	}
	else
	{
		MOV(accessSize, dest, reg_value);
	}
}

static OpArg FixImmediate(int accessSize, OpArg arg)
{
	if (arg.IsImm())
	{
		arg = accessSize == 8  ? Imm8((u8)arg.Imm32()) :
		      accessSize == 16 ? Imm16((u16)arg.Imm32()) :
		                         Imm32((u32)arg.Imm32());
	}
	return arg;
}

void EmuCodeBlock::UnsafeWriteGatherPipe(int accessSize)
{
	// No need to protect these, they don't touch any state
	// question - should we inline them instead? Pro: Lose a CALL   Con: Code bloat
	switch (accessSize)
	{
	case 8:
		CALL(jit->GetAsmRoutines()->fifoDirectWrite8);
		break;
	case 16:
		CALL(jit->GetAsmRoutines()->fifoDirectWrite16);
		break;
	case 32:
		CALL(jit->GetAsmRoutines()->fifoDirectWrite32);
		break;
	case 64:
		CALL(jit->GetAsmRoutines()->fifoDirectWrite64);
		break;
	}
	jit->js.fifoBytesThisBlock += accessSize >> 3;
}

bool EmuCodeBlock::WriteToConstAddress(int accessSize, OpArg arg, u32 address, BitSet32 registersInUse)
{
	arg = FixImmediate(accessSize, arg);

	// If we already know the address through constant folding, we can do some
	// fun tricks...
	if (jit->jo.optimizeGatherPipe && PowerPC::IsOptimizableGatherPipeWrite(address))
	{
		if (!arg.IsSimpleReg(RSCRATCH))
			MOV(accessSize, R(RSCRATCH), arg);

		UnsafeWriteGatherPipe(accessSize);
		return false;
	}
	else if (PowerPC::IsOptimizableRAMAddress(address))
	{
		WriteToConstRamAddress(accessSize, arg, address);
		return false;
	}
	else
	{
		// Helps external systems know which instruction triggered the write
		MOV(32, PPCSTATE(pc), Imm32(jit->js.compilerPC));

		ABI_PushRegistersAndAdjustStack(registersInUse, 0);
		switch (accessSize)
		{
		case 64:
			ABI_CallFunctionAC(64, (void *)&PowerPC::Write_U64, arg, address);
			break;
		case 32:
			ABI_CallFunctionAC(32, (void *)&PowerPC::Write_U32, arg, address);
			break;
		case 16:
			ABI_CallFunctionAC(16, (void *)&PowerPC::Write_U16, arg, address);
			break;
		case 8:
			ABI_CallFunctionAC(8, (void *)&PowerPC::Write_U8, arg, address);
			break;
		}
		ABI_PopRegistersAndAdjustStack(registersInUse, 0);
		return true;
	}
}

void EmuCodeBlock::SafeWriteRegToReg(OpArg reg_value, X64Reg reg_addr, int accessSize, s32 offset, BitSet32 registersInUse, int flags)
{
	bool swap = !(flags & SAFE_LOADSTORE_NO_SWAP);
	bool slowmem = (flags & SAFE_LOADSTORE_FORCE_SLOWMEM) != 0;

	// set the correct immediate format
	reg_value = FixImmediate(accessSize, reg_value);

	if (jit->jo.fastmem && !(flags & SAFE_LOADSTORE_NO_FASTMEM) && !slowmem)
	{
		u8* backpatchStart = GetWritableCodePtr();
		MovInfo mov;
		UnsafeWriteRegToReg(reg_value, reg_addr, accessSize, offset, swap, &mov);
		TrampolineInfo& info = backPatchInfo[mov.address];
		info.pc = jit->js.compilerPC;
		info.mov = mov;
		info.start = backpatchStart;
		info.read = false;
		info.op_arg = reg_value;
		info.op_reg = reg_addr;
		info.accessSize = accessSize;
		info.offset = offset;
		info.registersInUse = registersInUse;
		info.flags = flags;
		ptrdiff_t padding = BACKPATCH_SIZE - (GetCodePtr() - backpatchStart);
		if (padding > 0)
		{
			NOP(padding);
		}
		info.end = GetCodePtr();

		jit->js.fastmemLoadStore = mov.address;

		return;
	}

	if (offset)
	{
		if (flags & SAFE_LOADSTORE_CLOBBER_RSCRATCH_INSTEAD_OF_ADDR)
		{
			LEA(32, RSCRATCH, MDisp(reg_addr, (u32)offset));
			reg_addr = RSCRATCH;
		}
		else
		{
			ADD(32, R(reg_addr), Imm32((u32)offset));
		}
	}

	FixupBranch slow, exit;
	if (!slowmem)
	{
		u32 mem_mask = Memory::ADDR_MASK_HW_ACCESS;

		// The following masks the region used by the GC/Wii virtual memory lib
		mem_mask |= Memory::ADDR_MASK_MEM1;

		slow = CheckIfSafeAddress(reg_value, reg_addr, registersInUse, mem_mask);
		UnsafeWriteRegToReg(reg_value, reg_addr, accessSize, 0, swap);
		if (farcode.Enabled())
			SwitchToFarCode();
		else
			exit = J(true);
		SetJumpTarget(slow);

		// PC is used by memory watchpoints (if enabled) or to print accurate PC locations in debug logs
		MOV(32, PPCSTATE(pc), Imm32(jit->js.compilerPC));
	}

	size_t rsp_alignment = (flags & SAFE_LOADSTORE_NO_PROLOG) ? 8 : 0;
	ABI_PushRegistersAndAdjustStack(registersInUse, rsp_alignment);

	// If the input is an immediate, we need to put it in a register.
	X64Reg reg;
	if (reg_value.IsImm())
	{
		reg = reg_addr == ABI_PARAM1 ? RSCRATCH : ABI_PARAM1;
		MOV(accessSize, R(reg), reg_value);
	}
	else
	{
		reg = reg_value.GetSimpleReg();
	}

	switch (accessSize)
	{
	case 64:
		ABI_CallFunctionRR(swap ? ((void *)&PowerPC::Write_U64) : ((void *)&PowerPC::Write_U64_Swap), reg, reg_addr);
		break;
	case 32:
		ABI_CallFunctionRR(swap ? ((void *)&PowerPC::Write_U32) : ((void *)&PowerPC::Write_U32_Swap), reg, reg_addr);
		break;
	case 16:
		ABI_CallFunctionRR(swap ? ((void *)&PowerPC::Write_U16) : ((void *)&PowerPC::Write_U16_Swap), reg, reg_addr);
		break;
	case 8:
		ABI_CallFunctionRR((void *)&PowerPC::Write_U8, reg, reg_addr);
		break;
	}
	ABI_PopRegistersAndAdjustStack(registersInUse, rsp_alignment);

	MemoryExceptionCheck();

	if (!slowmem)
	{
		if (farcode.Enabled())
		{
			exit = J(true);
			SwitchToNearCode();
		}
		SetJumpTarget(exit);
	}
}

void EmuCodeBlock::WriteToConstRamAddress(int accessSize, OpArg arg, u32 address, bool swap)
{
	X64Reg reg;
	if (arg.IsImm())
	{
		arg = SwapImmediate(accessSize, arg);
		MOV(32, R(RSCRATCH), Imm32(address));
		MOV(accessSize, MRegSum(RMEM, RSCRATCH), arg);
		return;
	}

	if (!arg.IsSimpleReg() || (!cpu_info.bMOVBE && swap && arg.GetSimpleReg() != RSCRATCH))
	{
		MOV(accessSize, R(RSCRATCH), arg);
		reg = RSCRATCH;
	}
	else
	{
		reg = arg.GetSimpleReg();
	}

	MOV(32, R(RSCRATCH2), Imm32(address));
	if (swap)
		SwapAndStore(accessSize, MRegSum(RMEM, RSCRATCH2), reg);
	else
		MOV(accessSize, MRegSum(RMEM, RSCRATCH2), R(reg));
}

void EmuCodeBlock::ForceSinglePrecision(X64Reg output, const OpArg& input, bool packed, bool duplicate)
{
	// Most games don't need these. Zelda requires it though - some platforms get stuck without them.
	if (jit->jo.accurateSinglePrecision)
	{
		if (packed)
		{
			CVTPD2PS(output, input);
			CVTPS2PD(output, R(output));
		}
		else
		{
			CVTSD2SS(output, input);
			CVTSS2SD(output, R(output));
			if (duplicate)
				MOVDDUP(output, R(output));
		}
	}
	else if (!input.IsSimpleReg(output))
	{
		if (duplicate)
			MOVDDUP(output, input);
		else
			MOVAPD(output, input);
	}
}

// Abstract between AVX and SSE: automatically handle 3-operand instructions
void EmuCodeBlock::avx_op(void (XEmitter::*avxOp)(X64Reg, X64Reg, const OpArg&), void (XEmitter::*sseOp)(X64Reg, const OpArg&),
                          X64Reg regOp, const OpArg& arg1, const OpArg& arg2, bool packed, bool reversible)
{
	if (arg1.IsSimpleReg(regOp))
	{
		(this->*sseOp)(regOp, arg2);
	}
	else if (arg1.IsSimpleReg() && cpu_info.bAVX)
	{
		(this->*avxOp)(regOp, arg1.GetSimpleReg(), arg2);
	}
	else if (arg2.IsSimpleReg(regOp))
	{
		if (reversible)
		{
			(this->*sseOp)(regOp, arg1);
		}
		else
		{
			// The ugly case: regOp == arg2 without AVX, or with arg1 == memory
			if (!arg1.IsSimpleReg(XMM0))
				MOVAPD(XMM0, arg1);
			if (cpu_info.bAVX)
			{
				(this->*avxOp)(regOp, XMM0, arg2);
			}
			else
			{
				(this->*sseOp)(XMM0, arg2);
				if (packed)
					MOVAPD(regOp, R(XMM0));
				else
					MOVSD(regOp, R(XMM0));
			}
		}
	}
	else
	{
		if (packed)
			MOVAPD(regOp, arg1);
		else
			MOVSD(regOp, arg1);
		(this->*sseOp)(regOp, arg1 == arg2 ? R(regOp) : arg2);
	}
}

// Abstract between AVX and SSE: automatically handle 3-operand instructions
void EmuCodeBlock::avx_op(void (XEmitter::*avxOp)(X64Reg, X64Reg, const OpArg&, u8), void (XEmitter::*sseOp)(X64Reg, const OpArg&, u8),
                          X64Reg regOp, const OpArg& arg1, const OpArg& arg2, u8 imm)
{
	if (arg1.IsSimpleReg(regOp))
	{
		(this->*sseOp)(regOp, arg2, imm);
	}
	else if (arg1.IsSimpleReg() && cpu_info.bAVX)
	{
		(this->*avxOp)(regOp, arg1.GetSimpleReg(), arg2, imm);
	}
	else if (arg2.IsSimpleReg(regOp))
	{
		// The ugly case: regOp == arg2 without AVX, or with arg1 == memory
		if (!arg1.IsSimpleReg(XMM0))
			MOVAPD(XMM0, arg1);
		if (cpu_info.bAVX)
		{
			(this->*avxOp)(regOp, XMM0, arg2, imm);
		}
		else
		{
			(this->*sseOp)(XMM0, arg2, imm);
			MOVAPD(regOp, R(XMM0));
		}
	}
	else
	{
		MOVAPD(regOp, arg1);
		(this->*sseOp)(regOp, arg1 == arg2 ? R(regOp) : arg2, imm);
	}
}

alignas(16) static const u64 psMantissaTruncate[2] = {0xFFFFFFFFF8000000ULL, 0xFFFFFFFFF8000000ULL};
alignas(16) static const u64 psRoundBit[2] = {0x8000000, 0x8000000};

// Emulate the odd truncation/rounding that the PowerPC does on the RHS operand before
// a single precision multiply. To be precise, it drops the low 28 bits of the mantissa,
// rounding to nearest as it does.
// It needs a temp, so let the caller pass that in.
void EmuCodeBlock::Force25BitPrecision(X64Reg output, const OpArg& input, X64Reg tmp)
{
	if (jit->jo.accurateSinglePrecision)
	{
		// mantissa = (mantissa & ~0xFFFFFFF) + ((mantissa & (1ULL << 27)) << 1);
		if (input.IsSimpleReg() && cpu_info.bAVX)
		{
			VPAND(tmp, input.GetSimpleReg(), M(psRoundBit));
			VPAND(output, input.GetSimpleReg(), M(psMantissaTruncate));
			PADDQ(output, R(tmp));
		}
		else
		{
			if (!input.IsSimpleReg(output))
				MOVAPD(output, input);
			avx_op(&XEmitter::VPAND, &XEmitter::PAND, tmp, R(output), M(psRoundBit), true, true);
			PAND(output, M(psMantissaTruncate));
			PADDQ(output, R(tmp));
		}
	}
	else if (!input.IsSimpleReg(output))
	{
		MOVAPD(output, input);
	}
}

alignas(16) static u32 temp32;
alignas(16) static u64 temp64;

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

alignas(16) static const __m128i double_exponent = _mm_set_epi64x(0, 0x7ff0000000000000);
alignas(16) static const __m128i double_fraction = _mm_set_epi64x(0, 0x000fffffffffffff);
alignas(16) static const __m128i double_sign_bit = _mm_set_epi64x(0, 0x8000000000000000);
alignas(16) static const __m128i double_explicit_top_bit = _mm_set_epi64x(0, 0x0010000000000000);
alignas(16) static const __m128i double_top_two_bits = _mm_set_epi64x(0, 0xc000000000000000);
alignas(16) static const __m128i double_bottom_bits  = _mm_set_epi64x(0, 0x07ffffffe0000000);

// This is the same algorithm used in the interpreter (and actual hardware)
// The documentation states that the conversion of a double with an outside the
// valid range for a single (or a single denormal) is undefined.
// But testing on actual hardware shows it always picks bits 0..1 and 5..34
// unless the exponent is in the range of 874 to 896.
void EmuCodeBlock::ConvertDoubleToSingle(X64Reg dst, X64Reg src)
{
	MOVSD(XMM1, R(src));

	// Grab Exponent
	PAND(XMM1, M(&double_exponent));
	PSRLQ(XMM1, 52);
	MOVD_xmm(R(RSCRATCH), XMM1);


	// Check if the double is in the range of valid single subnormal
	CMP(16, R(RSCRATCH), Imm16(896));
	FixupBranch NoDenormalize = J_CC(CC_G);
	CMP(16, R(RSCRATCH), Imm16(874));
	FixupBranch NoDenormalize2 = J_CC(CC_L);

	// Denormalise

	// shift = (905 - Exponent) plus the 21 bit double to single shift
	MOV(16, R(RSCRATCH), Imm16(905 + 21));
	MOVD_xmm(XMM0, R(RSCRATCH));
	PSUBQ(XMM0, R(XMM1));

	// xmm1 = fraction | 0x0010000000000000
	MOVSD(XMM1, R(src));
	PAND(XMM1, M(&double_fraction));
	POR(XMM1, M(&double_explicit_top_bit));

	// fraction >> shift
	PSRLQ(XMM1, R(XMM0));

	// OR the sign bit in.
	MOVSD(XMM0, R(src));
	PAND(XMM0, M(&double_sign_bit));
	PSRLQ(XMM0, 32);
	POR(XMM1, R(XMM0));

	FixupBranch end = J(false); // Goto end

	SetJumpTarget(NoDenormalize);
	SetJumpTarget(NoDenormalize2);

	// Don't Denormalize

	// We want bits 0, 1
	MOVSD(XMM1, R(src));
	PAND(XMM1, M(&double_top_two_bits));
	PSRLQ(XMM1, 32);

	// And 5 through to 34
	MOVSD(XMM0, R(src));
	PAND(XMM0, M(&double_bottom_bits));
	PSRLQ(XMM0, 29);

	// OR them togther
	POR(XMM1, R(XMM0));

	// End
	SetJumpTarget(end);
	MOVDDUP(dst, R(XMM1));
}

#else // MORE_ACCURATE_DOUBLETOSINGLE

alignas(16) static const __m128i double_sign_bit = _mm_set_epi64x(0xffffffffffffffff, 0x7fffffffffffffff);
alignas(16) static const __m128i single_qnan_bit = _mm_set_epi64x(0xffffffffffffffff, 0xffffffffffbfffff);
alignas(16) static const __m128i double_qnan_bit = _mm_set_epi64x(0xffffffffffffffff, 0xfff7ffffffffffff);

// Smallest positive double that results in a normalized single.
alignas(16) static const double min_norm_single = std::numeric_limits<float>::min();

void EmuCodeBlock::ConvertDoubleToSingle(X64Reg dst, X64Reg src)
{
	// Most games have flush-to-zero enabled, which causes the single -> double -> single process here to be lossy.
	// This is a problem when games use float operations to copy non-float data.
	// Changing the FPU mode is very expensive, so we can't do that.
	// Here, check to see if the source is small enough that it will result in a denormal, and pass it to the x87 unit
	// if it is.
	avx_op(&XEmitter::VPAND, &XEmitter::PAND, XMM0, R(src), M(&double_sign_bit), true, true);
	UCOMISD(XMM0, M(&min_norm_single));
	FixupBranch nanConversion = J_CC(CC_P, true);
	FixupBranch denormalConversion = J_CC(CC_B, true);
	CVTSD2SS(dst, R(src));

	SwitchToFarCode();
		SetJumpTarget(nanConversion);
		MOVQ_xmm(R(RSCRATCH), src);
		// Put the quiet bit into CF.
		BT(64, R(RSCRATCH), Imm8(51));
		CVTSD2SS(dst, R(src));
		FixupBranch continue1 = J_CC(CC_C, true);
		// Clear the quiet bit of the SNaN, which was 0 (signalling) but got set to 1 (quiet) by conversion.
		ANDPS(dst, M(&single_qnan_bit));
		FixupBranch continue2 = J(true);

		SetJumpTarget(denormalConversion);
		MOVSD(M(&temp64), src);
		FLD(64, M(&temp64));
		FSTP(32, M(&temp32));
		MOVSS(dst, M(&temp32));
		FixupBranch continue3 = J(true);
	SwitchToNearCode();

	SetJumpTarget(continue1);
	SetJumpTarget(continue2);
	SetJumpTarget(continue3);
	// We'd normally need to MOVDDUP here to put the single in the top half of the output register too, but
	// this function is only used to go directly to a following store, so we omit the MOVDDUP here.
}
#endif // MORE_ACCURATE_DOUBLETOSINGLE

// Converting single->double is a bit easier because all single denormals are double normals.
void EmuCodeBlock::ConvertSingleToDouble(X64Reg dst, X64Reg src, bool src_is_gpr)
{
	X64Reg gprsrc = src_is_gpr ? src : RSCRATCH;
	if (src_is_gpr)
	{
		MOVD_xmm(dst, R(src));
	}
	else
	{
		if (dst != src)
			MOVAPS(dst, R(src));
		MOVD_xmm(R(RSCRATCH), src);
	}

	UCOMISS(dst, R(dst));
	CVTSS2SD(dst, R(dst));
	FixupBranch nanConversion = J_CC(CC_P, true);

	SwitchToFarCode();
		SetJumpTarget(nanConversion);
		TEST(32, R(gprsrc), Imm32(0x00400000));
		FixupBranch continue1 = J_CC(CC_NZ, true);
		ANDPD(dst, M(&double_qnan_bit));
		FixupBranch continue2 = J(true);
	SwitchToNearCode();

	SetJumpTarget(continue1);
	SetJumpTarget(continue2);
	MOVDDUP(dst, R(dst));
}

alignas(16) static const u64 psDoubleExp[2]  = {0x7FF0000000000000ULL, 0};
alignas(16) static const u64 psDoubleFrac[2] = {0x000FFFFFFFFFFFFFULL, 0};
alignas(16) static const u64 psDoubleNoSign[2] = {0x7FFFFFFFFFFFFFFFULL, 0};

// TODO: it might be faster to handle FPRF in the same way as CR is currently handled for integer, storing
// the result of each floating point op and calculating it when needed. This is trickier than for integers
// though, because there's 32 possible FPRF bit combinations but only 9 categories of floating point values,
// which makes the whole thing rather trickier.
// Fortunately, PPCAnalyzer can optimize out a large portion of FPRF calculations, so maybe this isn't
// quite that necessary.
void EmuCodeBlock::SetFPRF(Gen::X64Reg xmm)
{
	AND(32, PPCSTATE(fpscr), Imm32(~FPRF_MASK));

	FixupBranch continue1, continue2, continue3, continue4;
	if (cpu_info.bSSE4_1)
	{
		MOVQ_xmm(R(RSCRATCH), xmm);
		SHR(64, R(RSCRATCH), Imm8(63)); // Get the sign bit; almost all the branches need it.
		PTEST(xmm, M(psDoubleExp));
		FixupBranch maxExponent = J_CC(CC_C);
		FixupBranch zeroExponent = J_CC(CC_Z);

		// Nice normalized number: sign ? PPC_FPCLASS_NN : PPC_FPCLASS_PN;
		LEA(32, RSCRATCH, MScaled(RSCRATCH, MathUtil::PPC_FPCLASS_NN - MathUtil::PPC_FPCLASS_PN, MathUtil::PPC_FPCLASS_PN));
		continue1 = J();

		SetJumpTarget(maxExponent);
		PTEST(xmm, M(psDoubleFrac));
		FixupBranch notNAN = J_CC(CC_Z);

		// Max exponent + mantissa: PPC_FPCLASS_QNAN
		MOV(32, R(RSCRATCH), Imm32(MathUtil::PPC_FPCLASS_QNAN));
		continue2 = J();

		// Max exponent + no mantissa: sign ? PPC_FPCLASS_NINF : PPC_FPCLASS_PINF;
		SetJumpTarget(notNAN);
		LEA(32, RSCRATCH, MScaled(RSCRATCH, MathUtil::PPC_FPCLASS_NINF - MathUtil::PPC_FPCLASS_PINF, MathUtil::PPC_FPCLASS_PINF));
		continue3 = J();

		SetJumpTarget(zeroExponent);
		PTEST(xmm, R(xmm));
		FixupBranch zero = J_CC(CC_Z);

		// No exponent + mantissa: sign ? PPC_FPCLASS_ND : PPC_FPCLASS_PD;
		LEA(32, RSCRATCH, MScaled(RSCRATCH, MathUtil::PPC_FPCLASS_ND - MathUtil::PPC_FPCLASS_PD, MathUtil::PPC_FPCLASS_PD));
		continue4 = J();

		// Zero: sign ? PPC_FPCLASS_NZ : PPC_FPCLASS_PZ;
		SetJumpTarget(zero);
		SHL(32, R(RSCRATCH), Imm8(4));
		ADD(32, R(RSCRATCH), Imm8(MathUtil::PPC_FPCLASS_PZ));
	}
	else
	{
		MOVQ_xmm(R(RSCRATCH), xmm);
		TEST(64, R(RSCRATCH), M(psDoubleExp));
		FixupBranch zeroExponent = J_CC(CC_Z);
		AND(64, R(RSCRATCH), M(psDoubleNoSign));
		CMP(64, R(RSCRATCH), M(psDoubleExp));
		FixupBranch nan = J_CC(CC_G); // This works because if the sign bit is set, RSCRATCH is negative
		FixupBranch infinity = J_CC(CC_E);
		MOVQ_xmm(R(RSCRATCH), xmm);
		SHR(64, R(RSCRATCH), Imm8(63));
		LEA(32, RSCRATCH, MScaled(RSCRATCH, MathUtil::PPC_FPCLASS_NN - MathUtil::PPC_FPCLASS_PN, MathUtil::PPC_FPCLASS_PN));
		continue1 = J();
		SetJumpTarget(nan);
		MOVQ_xmm(R(RSCRATCH), xmm);
		SHR(64, R(RSCRATCH), Imm8(63));
		MOV(32, R(RSCRATCH), Imm32(MathUtil::PPC_FPCLASS_QNAN));
		continue2 = J();
		SetJumpTarget(infinity);
		MOVQ_xmm(R(RSCRATCH), xmm);
		SHR(64, R(RSCRATCH), Imm8(63));
		LEA(32, RSCRATCH, MScaled(RSCRATCH, MathUtil::PPC_FPCLASS_NINF - MathUtil::PPC_FPCLASS_PINF, MathUtil::PPC_FPCLASS_PINF));
		continue3 = J();
		SetJumpTarget(zeroExponent);
		TEST(64, R(RSCRATCH), R(RSCRATCH));
		FixupBranch zero = J_CC(CC_Z);
		SHR(64, R(RSCRATCH), Imm8(63));
		LEA(32, RSCRATCH, MScaled(RSCRATCH, MathUtil::PPC_FPCLASS_ND - MathUtil::PPC_FPCLASS_PD, MathUtil::PPC_FPCLASS_PD));
		continue4 = J();
		SetJumpTarget(zero);
		SHR(64, R(RSCRATCH), Imm8(63));
		SHL(32, R(RSCRATCH), Imm8(4));
		ADD(32, R(RSCRATCH), Imm8(MathUtil::PPC_FPCLASS_PZ));
	}

	SetJumpTarget(continue1);
	SetJumpTarget(continue2);
	SetJumpTarget(continue3);
	SetJumpTarget(continue4);
	SHL(32, R(RSCRATCH), Imm8(FPRF_SHIFT));
	OR(32, PPCSTATE(fpscr), R(RSCRATCH));
}

void EmuCodeBlock::JitGetAndClearCAOV(bool oe)
{
	if (oe)
		AND(8, PPCSTATE(xer_so_ov), Imm8(~XER_OV_MASK)); //XER.OV = 0
	SHR(8, PPCSTATE(xer_ca), Imm8(1)); //carry = XER.CA, XER.CA = 0
}

void EmuCodeBlock::JitSetCA()
{
	MOV(8, PPCSTATE(xer_ca), Imm8(1)); //XER.CA = 1
}

// Some testing shows CA is set roughly ~1/3 of the time (relative to clears), so
// branchless calculation of CA is probably faster in general.
void EmuCodeBlock::JitSetCAIf(CCFlags conditionCode)
{
	SETcc(conditionCode, PPCSTATE(xer_ca));
}

void EmuCodeBlock::JitClearCA()
{
	MOV(8, PPCSTATE(xer_ca), Imm8(0));
}
