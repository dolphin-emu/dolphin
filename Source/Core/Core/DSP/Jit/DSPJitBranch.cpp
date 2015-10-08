// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/DSP/DSPAnalyzer.h"
#include "Core/DSP/DSPEmitter.h"
#include "Core/DSP/DSPMemoryMap.h"
#include "Core/DSP/DSPStacks.h"

using namespace Gen;

template <void(*jitCode)(const UDSPInstruction, DSPEmitter&)>
static void ReJitConditional(const UDSPInstruction opc, DSPEmitter& emitter)
{
	u8 cond = opc & 0xf;
	if (cond == 0xf) // Always true.
	{
		jitCode(opc,emitter);
		return;
	}

	emitter.dsp_op_read_reg(DSP_REG_SR, EAX);

	switch (cond)
	{
	case 0x0: // GE - Greater Equal
	case 0x1: // L - Less
		emitter.LEA(16, EDX, MScaled(EAX, SCALE_4, 0));
		emitter.XOR(16, R(EAX), R(EDX));
		emitter.TEST(16, R(EAX), Imm16(8));
		break;
	case 0x2: // G - Greater
	case 0x3: // LE - Less Equal
		emitter.LEA(16, EDX, MScaled(EAX, SCALE_4, 0));
		emitter.XOR(16, R(EAX), R(EDX));
		emitter.LEA(16, EAX, MScaled(EAX, SCALE_2, 0));
		emitter.OR(16, R(EAX), R(EDX));
		emitter.TEST(16, R(EAX), Imm16(0x10));
		break;
	case 0x4: // NZ - Not Zero
	case 0x5: // Z - Zero
		emitter.TEST(16, R(EAX), Imm16(SR_ARITH_ZERO));
		break;
	case 0x6: // NC - Not carry
	case 0x7: // C - Carry
		emitter.TEST(16, R(EAX), Imm16(SR_CARRY));
		break;
	case 0x8: // ? - Not over s32
	case 0x9: // ? - Over s32
		emitter.TEST(16, R(EAX), Imm16(SR_OVER_S32));
		break;
	case 0xa: // ?
	case 0xb: // ?
		emitter.LEA(16, EDX, MScaled(EAX, SCALE_2, 0));
		emitter.OR(16, R(EAX), R(EDX));
		emitter.LEA(16, EDX, MScaled(EDX, SCALE_8, 0));
		emitter.NOT(16, R(EAX));
		emitter.OR(16, R(EAX), R(EDX));
		emitter.TEST(16, R(EAX), Imm16(0x20));
		break;
	case 0xc: // LNZ  - Logic Not Zero
	case 0xd: // LZ - Logic Zero
		emitter.TEST(16, R(EAX), Imm16(SR_LOGIC_ZERO));
		break;
	case 0xe: // 0 - Overflow
		emitter.TEST(16, R(EAX), Imm16(SR_OVERFLOW));
		break;
	}
	DSPJitRegCache c1(emitter.gpr);
	FixupBranch skipCode = cond == 0xe ? emitter.J_CC(CC_E,true) : emitter.J_CC((CCFlags)(CC_NE - (cond & 1)),true);
	jitCode(opc,emitter);
	emitter.gpr.FlushRegs(c1);
	emitter.SetJumpTarget(skipCode);
}

static void WriteBranchExit(DSPEmitter& emitter)
{
	DSPJitRegCache c(emitter.gpr);
	emitter.gpr.SaveRegs();
	if (DSPAnalyzer::code_flags[emitter.startAddr] & DSPAnalyzer::CODE_IDLE_SKIP)
	{
		emitter.MOV(16, R(EAX), Imm16(0x1000));
	}
	else
	{
		emitter.MOV(16, R(EAX), Imm16(emitter.blockSize[emitter.startAddr]));
	}
	emitter.JMP(emitter.returnDispatcher, true);
	emitter.gpr.LoadRegs(false);
	emitter.gpr.FlushRegs(c,false);
}

static void WriteBlockLink(DSPEmitter& emitter, u16 dest)
{
	// Jump directly to the called block if it has already been compiled.
	if (!(dest >= emitter.startAddr && dest <= emitter.compilePC))
	{
		if (emitter.blockLinks[dest] != nullptr )
		{
			emitter.gpr.FlushRegs();
			// Check if we have enough cycles to execute the next block
			emitter.MOV(16, R(ECX), M(&cyclesLeft));
			emitter.CMP(16, R(ECX), Imm16(emitter.blockSize[emitter.startAddr] + emitter.blockSize[dest]));
			FixupBranch notEnoughCycles = emitter.J_CC(CC_BE);

			emitter.SUB(16, R(ECX), Imm16(emitter.blockSize[emitter.startAddr]));
			emitter.MOV(16, M(&cyclesLeft), R(ECX));
			emitter.JMP(emitter.blockLinks[dest], true);
			emitter.SetJumpTarget(notEnoughCycles);
		}
		else
		{
			// The destination has not been compiled yet.  Add it to the list
			// of blocks that this block is waiting on.
			emitter.unresolvedJumps[emitter.startAddr].push_back(dest);
		}
	}
}

static void r_jcc(const UDSPInstruction opc, DSPEmitter& emitter)
{
	u16 dest = dsp_imem_read(emitter.compilePC + 1);
	const DSPOPCTemplate *opcode = GetOpTemplate(opc);

	// If the block is unconditional, attempt to link block
	if (opcode->uncond_branch)
		WriteBlockLink(emitter, dest);
	emitter.MOV(16, M(&(g_dsp.pc)), Imm16(dest));
	WriteBranchExit(emitter);
}
// Generic jmp implementation
// Jcc addressA
// 0000 0010 1001 cccc
// aaaa aaaa aaaa aaaa
// Jump to addressA if condition cc has been met. Set program counter to
// address represented by value that follows this "jmp" instruction.
// NOTE: Cannot use FallBackToInterpreter(opc) here because of the need to write branch exit
void DSPEmitter::jcc(const UDSPInstruction opc)
{
	MOV(16, M(&(g_dsp.pc)), Imm16(compilePC + 2));
	ReJitConditional<r_jcc>(opc, *this);
}

static void r_jmprcc(const UDSPInstruction opc, DSPEmitter& emitter)
{
	u8 reg = (opc >> 5) & 0x7;
	//reg can only be DSP_REG_ARx and DSP_REG_IXx now,
	//no need to handle DSP_REG_STx.
	emitter.dsp_op_read_reg(reg, RAX, NONE);
	emitter.MOV(16, M(&g_dsp.pc), R(EAX));
	WriteBranchExit(emitter);
}
// Generic jmpr implementation
// JMPcc $R
// 0001 0111 rrr0 cccc
// Jump to address; set program counter to a value from register $R.
// NOTE: Cannot use FallBackToInterpreter(opc) here because of the need to write branch exit
void DSPEmitter::jmprcc(const UDSPInstruction opc)
{
	MOV(16, M(&g_dsp.pc), Imm16(compilePC + 1));
	ReJitConditional<r_jmprcc>(opc, *this);
}

static void r_call(const UDSPInstruction opc, DSPEmitter& emitter)
{
	emitter.MOV(16, R(DX), Imm16(emitter.compilePC + 2));
	emitter.dsp_reg_store_stack(DSP_STACK_C);
	u16 dest = dsp_imem_read(emitter.compilePC + 1);
	const DSPOPCTemplate *opcode = GetOpTemplate(opc);

	// If the block is unconditional, attempt to link block
	if (opcode->uncond_branch)
		WriteBlockLink(emitter, dest);
	emitter.MOV(16, M(&(g_dsp.pc)), Imm16(dest));
	WriteBranchExit(emitter);
}
// Generic call implementation
// CALLcc addressA
// 0000 0010 1011 cccc
// aaaa aaaa aaaa aaaa
// Call function if condition cc has been met. Push program counter of
// instruction following "call" to $st0. Set program counter to address
// represented by value that follows this "call" instruction.
// NOTE: Cannot use FallBackToInterpreter(opc) here because of the need to write branch exit
void DSPEmitter::call(const UDSPInstruction opc)
{
	MOV(16, M(&(g_dsp.pc)), Imm16(compilePC + 2));
	ReJitConditional<r_call>(opc, *this);
}

static void r_callr(const UDSPInstruction opc, DSPEmitter& emitter)
{
	u8 reg = (opc >> 5) & 0x7;
	emitter.MOV(16, R(DX), Imm16(emitter.compilePC + 1));
	emitter.dsp_reg_store_stack(DSP_STACK_C);
	emitter.dsp_op_read_reg(reg, RAX, NONE);
	emitter.MOV(16, M(&g_dsp.pc), R(EAX));
	WriteBranchExit(emitter);
}
// Generic callr implementation
// CALLRcc $R
// 0001 0111 rrr1 cccc
// Call function if condition cc has been met. Push program counter of
// instruction following "call" to call stack $st0. Set program counter to
// register $R.
// NOTE: Cannot use FallBackToInterpreter(opc) here because of the need to write branch exit
void DSPEmitter::callr(const UDSPInstruction opc)
{
	MOV(16, M(&g_dsp.pc), Imm16(compilePC + 1));
	ReJitConditional<r_callr>(opc, *this);
}

static void r_ifcc(const UDSPInstruction opc, DSPEmitter& emitter)
{
	emitter.MOV(16, M(&g_dsp.pc), Imm16(emitter.compilePC + 1));
}
// Generic if implementation
// IFcc
// 0000 0010 0111 cccc
// Execute following opcode if the condition has been met.
// NOTE: Cannot use FallBackToInterpreter(opc) here because of the need to write branch exit
void DSPEmitter::ifcc(const UDSPInstruction opc)
{
	MOV(16, M(&g_dsp.pc), Imm16((compilePC + 1) + opTable[dsp_imem_read(compilePC + 1)]->size));
	ReJitConditional<r_ifcc>(opc, *this);
	WriteBranchExit(*this);
}

static void r_ret(const UDSPInstruction opc, DSPEmitter& emitter)
{
	emitter.dsp_reg_load_stack(DSP_STACK_C);
	emitter.MOV(16, M(&g_dsp.pc), R(DX));
	WriteBranchExit(emitter);
}

// Generic ret implementation
// RETcc
// 0000 0010 1101 cccc
// Return from subroutine if condition cc has been met. Pops stored PC
// from call stack $st0 and sets $pc to this location.
// NOTE: Cannot use FallBackToInterpreter(opc) here because of the need to write branch exit
void DSPEmitter::ret(const UDSPInstruction opc)
{
	MOV(16, M(&g_dsp.pc), Imm16(compilePC + 1));
	ReJitConditional<r_ret>(opc, *this);
}

// RTI
// 0000 0010 1111 1111
// Return from exception. Pops stored status register $sr from data stack
// $st1 and program counter PC from call stack $st0 and sets $pc to this
// location.
void DSPEmitter::rti(const UDSPInstruction opc)
{
//	g_dsp.r[DSP_REG_SR] = dsp_reg_load_stack(DSP_STACK_D);
	dsp_reg_load_stack(DSP_STACK_D);
	dsp_op_write_reg(DSP_REG_SR, RDX);
//	g_dsp.pc = dsp_reg_load_stack(DSP_STACK_C);
	dsp_reg_load_stack(DSP_STACK_C);
	MOV(16, M(&g_dsp.pc), R(DX));
}

// HALT
// 0000 0000 0020 0001
// Stops execution of DSP code. Sets bit DSP_CR_HALT in register DREG_CR.
void DSPEmitter::halt(const UDSPInstruction opc)
{
	OR(16, M(&g_dsp.cr), Imm16(4));
	//	g_dsp.pc = dsp_reg_load_stack(DSP_STACK_C);
	dsp_reg_load_stack(DSP_STACK_C);
	MOV(16, M(&g_dsp.pc), R(DX));
}

// LOOP handling: Loop stack is used to control execution of repeated blocks of
// instructions. Whenever there is value on stack $st2 and current PC is equal
// value at $st2, then value at stack $st3 is decremented. If value is not zero
// then PC is modified with value from call stack $st0. Otherwise values from
// call stack $st0 and both loop stacks $st2 and $st3 are popped and execution
// continues at next opcode.
void DSPEmitter::HandleLoop()
{
	MOVZX(32, 16, EAX, M(&g_dsp.r.st[2]));
	MOVZX(32, 16, ECX, M(&g_dsp.r.st[3]));

	TEST(32, R(RCX), R(RCX));
	FixupBranch rLoopCntG = J_CC(CC_LE, true);
	CMP(16, R(RAX), Imm16(compilePC - 1));
	FixupBranch rLoopAddrG = J_CC(CC_NE, true);

	SUB(16, M(&(g_dsp.r.st[3])), Imm16(1));
	CMP(16, M(&(g_dsp.r.st[3])), Imm16(0));

	FixupBranch loadStack = J_CC(CC_LE, true);
	MOVZX(32, 16, ECX, M(&(g_dsp.r.st[0])));
	MOV(16, M(&g_dsp.pc), R(RCX));
	FixupBranch loopUpdated = J(true);

	SetJumpTarget(loadStack);
	DSPJitRegCache c(gpr);
	dsp_reg_load_stack(0);
	dsp_reg_load_stack(2);
	dsp_reg_load_stack(3);
	gpr.FlushRegs(c);

	SetJumpTarget(loopUpdated);
	SetJumpTarget(rLoopAddrG);
	SetJumpTarget(rLoopCntG);
}

// LOOP $R
// 0000 0000 010r rrrr
// Repeatedly execute following opcode until counter specified by value
// from register $R reaches zero. Each execution decrement counter. Register
// $R remains unchanged. If register $R is set to zero at the beginning of loop
// then looped instruction will not get executed.
// Actually, this instruction simply prepares the loop stacks for the above.
// The looping hardware takes care of the rest.
void DSPEmitter::loop(const UDSPInstruction opc)
{
	u16 reg = opc & 0x1f;
//	u16 cnt = g_dsp.r[reg];
//todo: check if we can use normal variant here
	dsp_op_read_reg_dont_saturate(reg, RDX, ZERO);
	u16 loop_pc = compilePC + 1;

	TEST(16, R(EDX), R(EDX));
	DSPJitRegCache c(gpr);
	FixupBranch cnt = J_CC(CC_Z, true);
	dsp_reg_store_stack(3);
	MOV(16, R(RDX), Imm16(compilePC + 1));
	dsp_reg_store_stack(0);
	MOV(16, R(RDX), Imm16(loop_pc));
	dsp_reg_store_stack(2);
	gpr.FlushRegs(c);
	MOV(16, M(&(g_dsp.pc)), Imm16(compilePC + 1));
	FixupBranch exit = J(true);

	SetJumpTarget(cnt);
	//		dsp_skip_inst();
	MOV(16, M(&g_dsp.pc), Imm16(loop_pc + opTable[dsp_imem_read(loop_pc)]->size));
	WriteBranchExit(*this);
	gpr.FlushRegs(c,false);
	SetJumpTarget(exit);
}

// LOOPI #I
// 0001 0000 iiii iiii
// Repeatedly execute following opcode until counter specified by
// immediate value I reaches zero. Each execution decrement counter. If
// immediate value I is set to zero at the beginning of loop then looped
// instruction will not get executed.
// Actually, this instruction simply prepares the loop stacks for the above.
// The looping hardware takes care of the rest.
void DSPEmitter::loopi(const UDSPInstruction opc)
{
	u16 cnt = opc & 0xff;
	u16 loop_pc = compilePC + 1;

	if (cnt)
	{
		MOV(16, R(RDX), Imm16(compilePC + 1));
		dsp_reg_store_stack(0);
		MOV(16, R(RDX), Imm16(loop_pc));
		dsp_reg_store_stack(2);
		MOV(16, R(RDX), Imm16(cnt));
		dsp_reg_store_stack(3);

		MOV(16, M(&(g_dsp.pc)), Imm16(compilePC + 1));
	}
	else
	{
//		dsp_skip_inst();
		MOV(16, M(&g_dsp.pc), Imm16(loop_pc + opTable[dsp_imem_read(loop_pc)]->size));
		WriteBranchExit(*this);
	}
}


// BLOOP $R, addrA
// 0000 0000 011r rrrr
// aaaa aaaa aaaa aaaa
// Repeatedly execute block of code starting at following opcode until
// counter specified by value from register $R reaches zero. Block ends at
// specified address addrA inclusive, ie. opcode at addrA is the last opcode
// included in loop. Counter is pushed on loop stack $st3, end of block address
// is pushed on loop stack $st2 and repeat address is pushed on call stack $st0.
// Up to 4 nested loops are allowed.
void DSPEmitter::bloop(const UDSPInstruction opc)
{
	u16 reg = opc & 0x1f;
//	u16 cnt = g_dsp.r[reg];
//todo: check if we can use normal variant here
	dsp_op_read_reg_dont_saturate(reg, RDX, ZERO);
	u16 loop_pc = dsp_imem_read(compilePC + 1);

	TEST(16, R(EDX), R(EDX));
	DSPJitRegCache c(gpr);
	FixupBranch cnt = J_CC(CC_Z, true);
	dsp_reg_store_stack(3);
	MOV(16, R(RDX), Imm16(compilePC + 2));
	dsp_reg_store_stack(0);
	MOV(16, R(RDX), Imm16(loop_pc));
	dsp_reg_store_stack(2);
	MOV(16, M(&(g_dsp.pc)), Imm16(compilePC + 2));
	gpr.FlushRegs(c,true);
	FixupBranch exit = J(true);

	SetJumpTarget(cnt);
	//		g_dsp.pc = loop_pc;
	//		dsp_skip_inst();
	MOV(16, M(&g_dsp.pc), Imm16(loop_pc + opTable[dsp_imem_read(loop_pc)]->size));
	WriteBranchExit(*this);
	gpr.FlushRegs(c,false);
	SetJumpTarget(exit);
}

// BLOOPI #I, addrA
// 0001 0001 iiii iiii
// aaaa aaaa aaaa aaaa
// Repeatedly execute block of code starting at following opcode until
// counter specified by immediate value I reaches zero. Block ends at specified
// address addrA inclusive, ie. opcode at addrA is the last opcode included in
// loop. Counter is pushed on loop stack $st3, end of block address is pushed
// on loop stack $st2 and repeat address is pushed on call stack $st0. Up to 4
// nested loops are allowed.
void DSPEmitter::bloopi(const UDSPInstruction opc)
{
	u16 cnt = opc & 0xff;
//	u16 loop_pc = dsp_fetch_code();
	u16 loop_pc = dsp_imem_read(compilePC + 1);

	if (cnt)
	{
		MOV(16, R(RDX), Imm16(compilePC + 2));
		dsp_reg_store_stack(0);
		MOV(16, R(RDX), Imm16(loop_pc));
		dsp_reg_store_stack(2);
		MOV(16, R(RDX), Imm16(cnt));
		dsp_reg_store_stack(3);

		MOV(16, M(&(g_dsp.pc)), Imm16(compilePC + 2));
	}
	else
	{
//		g_dsp.pc = loop_pc;
//		dsp_skip_inst();
		MOV(16, M(&g_dsp.pc), Imm16(loop_pc + opTable[dsp_imem_read(loop_pc)]->size));
		WriteBranchExit(*this);
	}
}
