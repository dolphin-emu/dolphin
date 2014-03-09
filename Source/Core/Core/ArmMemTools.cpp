// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include <cstdio>
#include <signal.h>
#ifdef ANDROID
#include <asm/sigcontext.h>
#else
#include <execinfo.h>
#include <sys/ucontext.h>   // Look in here for the context definition.
#endif

#include "Common/Common.h"
#include "Core/MemTools.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

namespace EMM
{
#ifdef ANDROID
typedef struct sigcontext mcontext_t;
typedef struct ucontext {
	uint32_t uc_flags;
	struct ucontext* uc_link;
	stack_t uc_stack;
	mcontext_t uc_mcontext;
	// Other fields are not used by Google Breakpad. Don't define them.
} ucontext_t;
#endif

void sigsegv_handler(int signal, siginfo_t *info, void *raw_context)
{
	if (signal != SIGSEGV)
	{
		// We are not interested in other signals - handle it as usual.
		return;
	}
	ucontext_t *context = (ucontext_t *)raw_context;
	int sicode = info->si_code;
	if (sicode != SEGV_MAPERR && sicode != SEGV_ACCERR)
	{
		// Huh? Return.
		return;
	}


	// Get all the information we can out of the context.
	mcontext_t *ctx = &context->uc_mcontext;

	void *fault_memory_ptr = (void*)ctx->arm_r10;
	u8 *fault_instruction_ptr = (u8 *)ctx->arm_pc;

	if (!JitInterface::IsInCodeSpace(fault_instruction_ptr)) {
		// Let's not prevent debugging.
		return;
	}

	u64 bad_address = (u64)fault_memory_ptr;
	u64 memspace_bottom = (u64)Memory::base;
	if (bad_address < memspace_bottom) {
		PanicAlertT("Exception handler - access below memory space. %08llx%08llx",
			bad_address >> 32, bad_address);
	}

	u32 em_address = (u32)(bad_address - memspace_bottom);

	const u8 *new_rip = jit->BackPatch(fault_instruction_ptr, em_address, ctx);
	if (new_rip) {
		ctx->arm_pc = (u32) new_rip;
	}
}

void InstallExceptionHandler()
{
	struct sigaction sa;
	sa.sa_handler = 0;
	sa.sa_sigaction = &sigsegv_handler;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGSEGV, &sa, nullptr);
}
}  // namespace
