#include <string>

#include "Common.h"
#include "disasm.h"
#include "JitBackpatch.h"
#include "../../HW/Memmap.h"

#include "x64Emitter.h"
#include "x64Analyzer.h"

#include "StringUtil.h"
#include "Jit.h"

using namespace Gen;

namespace Jit64 {

extern u8 *trampolineCodePtr;
	
void BackPatchError(const std::string &text, u8 *codePtr, u32 emAddress) {
	u64 code_addr = (u64)codePtr;
	disassembler disasm;
	char disbuf[256];
	memset(disbuf, 0, 256);
#ifdef _M_IX86
	disasm.disasm32
#else
	disasm.disasm64
#endif
		(0, code_addr, codePtr, disbuf);
	PanicAlert("%s\n\n"
       "Error encountered accessing emulated address %08x.\n"
	   "Culprit instruction: \n%s\nat %08x%08x",
	   text.c_str(), emAddress, disbuf, code_addr>>32, code_addr);
	return;
}

void BackPatch(u8 *codePtr, int accessType, u32 emAddress)
{
	if (!IsInJitCode(codePtr))
		return;  // this will become a regular crash real soon after this
	
	// TODO: also mark and remember the instruction address as known HW memory access, for use in later compiles.
	// But to do that we need to be able to reconstruct what instruction wrote this code, and we can't do that yet.
	u8 *oldCodePtr = GetWritableCodePtr();
	InstructionInfo info;
	if (!DisassembleMov(codePtr, info, accessType)) {
		BackPatchError("BackPatch - failed to disassemble MOV instruction", codePtr, emAddress);
	}
	if (info.operandSize != 4) {
		BackPatchError(StringFromFormat("BackPatch - no support for operand size %i", info.operandSize), codePtr, emAddress);
	}
	u64 code_addr = (u64)codePtr;
	X64Reg addrReg = (X64Reg)info.scaledReg;
	X64Reg dataReg = (X64Reg)info.regOperandReg;
	if (info.otherReg != RBX)
		PanicAlert("BackPatch : Base reg not RBX."
		           "\n\nAttempted to access %08x.", emAddress);
	if (accessType == OP_ACCESS_WRITE)
		PanicAlert("BackPatch : Currently only supporting reads."
		           "\n\nAttempted to write to %08x.", emAddress);
	//if (info.instructionSize < 5) 
	//	PanicAlert("Instruction at %08x Too Small : %i", (u32)codePtr, info.instructionSize);
	// OK, let's write a trampoline, and a jump to it.
	// Later, let's share trampolines.

	// In the first iteration, we assume that all accesses are 32-bit. We also only deal with reads.
	// Next step - support writes, special case FIFO writes. Also, support 32-bit mode.
	u8 *trampoline = trampolineCodePtr;
	SetCodePtr(trampolineCodePtr);
	// * Save all volatile regs
	PUSH(RCX);
	PUSH(RDX);
	PUSH(RSI); 
	PUSH(RDI); 
	PUSH(R8);
	PUSH(R9);
	PUSH(R10);
	PUSH(R11);
	//TODO: Also preserve XMM0-3?
	SUB(64, R(RSP), Imm8(0x20));
	// * Set up stack frame.
	// * Call ReadMemory32
	//LEA(32, ECX, MDisp((X64Reg)addrReg, info.displacement));
	MOV(32, R(ABI_PARAM1), R((X64Reg)addrReg));
	if (info.displacement) {
		ADD(32, R(ABI_PARAM1), Imm32(info.displacement));
	}
	switch (info.operandSize) {
	//case 1: 
	//	CALL((void *)&Memory::Read_U8);
	//	break;
	case 4: 
		CALL((void *)&Memory::Read_U32);
		break;
	default:
		BackPatchError(StringFromFormat("We don't handle the size %i yet in backpatch", info.operandSize), codePtr, emAddress);
		break;
	}
	// * Tear down stack frame.
	ADD(64, R(RSP), Imm8(0x20));
	POP(R11);
	POP(R10);
	POP(R9);
	POP(R8);
	POP(RDI); 
	POP(RSI); 
	POP(RDX);
	POP(RCX);
	MOV(32, R(dataReg), R(EAX));
	RET();
	trampolineCodePtr = GetWritableCodePtr();

	SetCodePtr(codePtr);
	int bswapNopCount;
	// Check the following BSWAP for REX byte
	if ((GetCodePtr()[info.instructionSize] & 0xF0) == 0x40)
		bswapNopCount = 3;
	else
		bswapNopCount = 2;
	CALL(trampoline);
	NOP((int)info.instructionSize + bswapNopCount - 5);
	SetCodePtr(oldCodePtr);
}

}  // namespace
