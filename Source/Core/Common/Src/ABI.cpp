#include "Common.h"
#include "x64Emitter.h"
#include "ABI.h"

using namespace Gen;

#ifdef _M_IX86 // All32

// Shared code between Win32 and Unix32
// ====================================

void ABI_CallFunctionC(void *func, u32 param1) {
	ABI_AlignStack(1 * 4);
	PUSH(32, Imm32(param1));
	CALL(func);
	ABI_RestoreStack(1 * 4);
}

void ABI_CallFunctionCC(void *func, u32 param1, u32 param2) {
	ABI_AlignStack(2 * 4);
	PUSH(32, Imm32(param2));
	PUSH(32, Imm32(param1));
	CALL(func);
	ABI_RestoreStack(2 * 4);
}

// Pass a register as a paremeter.
void ABI_CallFunctionR(void *func, X64Reg reg1) {
	ABI_AlignStack(1 * 4);
	PUSH(32, R(reg1));
	CALL(func);
	ABI_RestoreStack(1 * 4);
}

void ABI_CallFunctionRR(void *func, Gen::X64Reg reg1, Gen::X64Reg reg2)
{
	ABI_AlignStack(2 * 4);
	PUSH(32, R(reg2));
	PUSH(32, R(reg1));
	CALL(func);
	ABI_RestoreStack(2 * 4);
}

void ABI_CallFunctionAC(void *func, const Gen::OpArg &arg1, u32 param2)
{
	ABI_AlignStack(2 * 4);
	PUSH(32, arg1);
	PUSH(32, Imm32(param2));
	CALL(func);
	ABI_RestoreStack(2 * 4);
}

void ABI_PushAllCalleeSavedRegsAndAdjustStack() {
	// Note: 4 * 4 = 16 bytes, so alignment is preserved.
	PUSH(EBP);
	PUSH(EBX);
	PUSH(ESI);
	PUSH(EDI);
}

void ABI_PopAllCalleeSavedRegsAndAdjustStack() {
	POP(EDI);
	POP(ESI);
	POP(EBX);
	POP(EBP);
}

unsigned int ABI_GetAlignedFrameSize(unsigned int frameSize) {
	frameSize += 4; // reserve space for return address
	unsigned int alignedSize =
#ifdef __GNUC__
		(frameSize + 15) & -16;
#else
		frameSize;
#endif
	return alignedSize;
}

void ABI_AlignStack(unsigned int frameSize) {
// Mac OS X requires the stack to be 16-byte aligned before every call.
// Linux requires the stack to be 16-byte aligned before calls that put SSE
// vectors on the stack, but since we do not keep track of which calls do that,
// it is effectively every call as well.
// Windows binaries compiled with MSVC do not have such a restriction, but I
// expect that GCC on Windows acts the same as GCC on Linux in this respect.
// It would be nice if someone could verify this.
#ifdef __GNUC__
	unsigned int fillSize =
		ABI_GetAlignedFrameSize(frameSize) - (frameSize + 4);
	if (fillSize != 0) {
		SUB(32, R(ESP), Imm8(fillSize));
	}
#endif
}

void ABI_RestoreStack(unsigned int frameSize) {
	unsigned int alignedSize = ABI_GetAlignedFrameSize(frameSize);
	alignedSize -= 4; // return address is POPped at end of call
	if (alignedSize != 0) {
		ADD(32, R(ESP), Imm8(alignedSize));
	}
}

#else

// Shared code between Win64 and Unix64
// ====================================

void ABI_CallFunctionC(void *func, u32 param1) {
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	CALL(func);
}

void ABI_CallFunctionCC(void *func, u32 param1, u32 param2) {
	MOV(32, R(ABI_PARAM1), Imm32(param1));
	MOV(32, R(ABI_PARAM2), Imm32(param2));
	CALL(func);
}

// Pass a register as a paremeter.
void ABI_CallFunctionR(void *func, X64Reg reg1) {
	if (reg1 != ABI_PARAM1)
		MOV(32, R(ABI_PARAM1), R(reg1));
	CALL(func);
}

// Pass a register as a paremeter.
void ABI_CallFunctionRR(void *func, X64Reg reg1, X64Reg reg2) {
	if (reg1 != ABI_PARAM1)
		MOV(32, R(ABI_PARAM1), R(reg1));
	if (reg2 != ABI_PARAM2)
		MOV(32, R(ABI_PARAM2), R(reg2));
	CALL(func);
}

void ABI_CallFunctionAC(void *func, const Gen::OpArg &arg1, u32 param2)
{
	if (!arg1.IsSimpleReg(ABI_PARAM1))
		MOV(32, R(ABI_PARAM1), arg1);
	MOV(32, R(ABI_PARAM2), Imm32(param2));
	CALL(func);
}

unsigned int ABI_GetAlignedFrameSize(unsigned int frameSize) {
	return frameSize;
}

void ABI_AlignStack(unsigned int /*frameSize*/) {
}

void ABI_RestoreStack(unsigned int /*frameSize*/) {
}

#ifdef _WIN32

// Win64 Specific Code
// ====================================
void ABI_PushAllCalleeSavedRegsAndAdjustStack() {
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
	SUB(64, R(RSP), Imm8(0x28));
}

void ABI_PopAllCalleeSavedRegsAndAdjustStack() {
	ADD(64, R(RSP), Imm8(0x28));
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
// ====================================
void ABI_PushAllCallerSavedRegsAndAdjustStack() {
	PUSH(RCX);
	PUSH(RDX);
	PUSH(RSI); 
	PUSH(RDI);
	PUSH(R8);
	PUSH(R9);
	PUSH(R10);
	PUSH(R11);
	//TODO: Also preserve XMM0-15?
	SUB(64, R(RSP), Imm8(0x28));
}

void ABI_PopAllCallerSavedRegsAndAdjustStack() {
	ADD(64, R(RSP), Imm8(0x28));
	POP(R11);
	POP(R10);
	POP(R9);
	POP(R8);
	POP(RDI); 
	POP(RSI); 
	POP(RDX);
	POP(RCX);
}

#else
// Unix64 Specific Code
// ====================================
void ABI_PushAllCalleeSavedRegsAndAdjustStack() {
	PUSH(RBX); 
	PUSH(RBP);
	PUSH(R12); 
	PUSH(R13); 
	PUSH(R14); 
	PUSH(R15);
	PUSH(R15); //just to align stack. duped push/pop doesn't hurt.
}

void ABI_PopAllCalleeSavedRegsAndAdjustStack() {
	POP(R15);
	POP(R15);
	POP(R14); 
	POP(R13); 
	POP(R12);
	POP(RBP);
	POP(RBX); 
}

void ABI_PushAllCallerSavedRegsAndAdjustStack() {
	INT3();
	//not yet supported
}

void ABI_PopAllCallerSavedRegsAndAdjustStack() {
	INT3();
	//not yet supported
}

#endif

#endif

