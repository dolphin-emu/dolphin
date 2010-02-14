// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <map>

#include "Common.h"
#include "x64Emitter.h"
#include "MemoryUtil.h"
#include "ABI.h"
#include "Thunk.h"

#define THUNK_ARENA_SIZE 1024*1024*1

namespace
{

static u8 GC_ALIGNED32(saved_fp_state[16 * 4 * 4]);
static u8 GC_ALIGNED32(saved_gpr_state[16 * 8]);
static u16 saved_mxcsr;

}  // namespace

using namespace Gen;

void ThunkManager::Init()
{
	AllocCodeSpace(THUNK_ARENA_SIZE);
	save_regs = GetCodePtr();
	for (int i = 2; i < ABI_GetNumXMMRegs(); i++)
		MOVAPS(M(saved_fp_state + i * 16), (X64Reg)(XMM0 + i));
	STMXCSR(M(&saved_mxcsr));
#ifdef _M_X64
	MOV(64, M(saved_gpr_state + 0 ), R(RCX));
	MOV(64, M(saved_gpr_state + 8 ), R(RDX));
	MOV(64, M(saved_gpr_state + 16), R(R8) );
	MOV(64, M(saved_gpr_state + 24), R(R9) );
	MOV(64, M(saved_gpr_state + 32), R(R10));
	MOV(64, M(saved_gpr_state + 40), R(R11));
#ifndef _WIN32
	MOV(64, M(saved_gpr_state + 48), R(RSI));
	MOV(64, M(saved_gpr_state + 56), R(RDI));
#endif
	MOV(64, M(saved_gpr_state + 64), R(RBX));
#else
	MOV(32, M(saved_gpr_state + 0 ), R(RCX));
	MOV(32, M(saved_gpr_state + 4 ), R(RDX));
#endif
	RET();
	load_regs = GetCodePtr();
	LDMXCSR(M(&saved_mxcsr));
	for (int i = 2; i < ABI_GetNumXMMRegs(); i++)
		MOVAPS((X64Reg)(XMM0 + i), M(saved_fp_state + i * 16));
#ifdef _M_X64
	MOV(64, R(RCX), M(saved_gpr_state + 0 ));
	MOV(64, R(RDX), M(saved_gpr_state + 8 ));
	MOV(64, R(R8) , M(saved_gpr_state + 16));
	MOV(64, R(R9) , M(saved_gpr_state + 24));
	MOV(64, R(R10), M(saved_gpr_state + 32));
	MOV(64, R(R11), M(saved_gpr_state + 40));
#ifndef _WIN32
	MOV(64, R(RSI), M(saved_gpr_state + 48));
	MOV(64, R(RDI), M(saved_gpr_state + 56));
#endif
	MOV(64, R(RBX), M(saved_gpr_state + 64));
#else
	MOV(32, R(RCX), M(saved_gpr_state + 0 ));
	MOV(32, R(RDX), M(saved_gpr_state + 4 ));
#endif
	RET();
}

void ThunkManager::Reset()
{
	thunks.clear();
	ResetCodePtr();
}

void ThunkManager::Shutdown()
{
	Reset();
	FreeCodeSpace();
}

void *ThunkManager::ProtectFunction(void *function, int num_params)
{
	std::map<void *, const u8 *>::iterator iter;
	iter = thunks.find(function);
	if (iter != thunks.end())
		return (void *)iter->second;
	if (!region)
		PanicAlert("Trying to protect functions before the emu is started. Bad bad bad.");

	const u8 *call_point = GetCodePtr();
	// Make sure to align stack.
#ifdef _M_X64
#ifdef _WIN32
	SUB(64, R(ESP), Imm8(0x28));
#else
	SUB(64, R(ESP), Imm8(0x8));
#endif
	CALL((void*)save_regs);
	CALL((void*)function);
	CALL((void*)load_regs);
#ifdef _WIN32
	ADD(64, R(ESP), Imm8(0x28));
#else
	ADD(64, R(ESP), Imm8(0x8));
#endif
	RET();
#else
	CALL((void*)save_regs);
	// Since parameters are in the previous stack frame, not in registers, this takes some
	// trickery : we simply re-push the parameters. might not be optimal, but that doesn't really
	// matter.
	ABI_AlignStack(num_params * 4);
	unsigned int alignedSize = ABI_GetAlignedFrameSize(num_params * 4);
	for (int i = 0; i < num_params; i++) {
		// ESP is changing, so we do not need i
		PUSH(32, MDisp(ESP, alignedSize - 4));
	}
	CALL(function);
	ABI_RestoreStack(num_params * 4);
	CALL((void*)load_regs);
	RET();
#endif

	thunks[function] = call_point;
	return (void *)call_point;
}
