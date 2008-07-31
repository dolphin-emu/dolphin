#include "Common.h"
#include "x64Emitter.h"
#include "ABI.h"

using namespace Gen;

#ifdef _M_IX86 // All32

// Shared code between Win32 and Unix32
// ====================================

void ABI_CallFunctionC(void *func, u32 param1) {
	PUSH(32, Imm32(param1));
	CALL(func);
	ADD(32, R(ESP), Imm8(4));
}

void ABI_CallFunctionCC(void *func, u32 param1, u32 param2) {
	PUSH(32, Imm32(param2));
	PUSH(32, Imm32(param1));
	CALL(func);
	ADD(32, R(ESP), Imm8(8));
}

// Pass a register as a paremeter.
void ABI_CallFunctionR(void *func, X64Reg reg1) {
	PUSH(32, R(reg1));
	CALL(func);
	ADD(32, R(ESP), Imm8(4));
}

void ABI_CallFunctionAC(void *func, const Gen::OpArg &arg1, u32 param2)
{
	PUSH(32, arg1);
	PUSH(32, Imm32(param2));
	CALL(func);
	ADD(32, R(ESP), Imm8(8));
}

void ABI_PushAllCalleeSavedRegsAndAdjustStack() {
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

void ABI_CallFunctionAC(void *func, const Gen::OpArg &arg1, u32 param2)
{
	if (!arg1.IsSimpleReg(ABI_PARAM1))
		MOV(32, R(ABI_PARAM1), arg1);
	MOV(32, R(ABI_PARAM2), Imm32(param2));
	CALL(func);
}

#ifdef _WIN32
// Win64 Specific Code
// ====================================
void ABI_PushAllCalleeSavedRegsAndAdjustStack() {
	//we only want to do this once
	PUSH(RBX); 
	PUSH(RSI); 
	PUSH(RDI);
	//PUSH(RBP);
	PUSH(R12); 
	PUSH(R13); 
	PUSH(R14); 
	PUSH(R15);
	//TODO: Also preserve XMM0-3?
	SUB(64, R(RSP), Imm8(0x20));
}

void ABI_PopAllCalleeSavedRegsAndAdjustStack() {
	ADD(64, R(RSP), Imm8(0x20));
	POP(R15);
	POP(R14); 
	POP(R13); 
	POP(R12);
	//POP(RBP);
	POP(RDI);
	POP(RSI); 
	POP(RBX); 
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
}

void ABI_PopAllCalleeSavedRegsAndAdjustStack() {
	POP(R15);
	POP(R14); 
	POP(R13); 
	POP(R12);
	POP(RBP);
	POP(RBX); 
}

#endif

#endif