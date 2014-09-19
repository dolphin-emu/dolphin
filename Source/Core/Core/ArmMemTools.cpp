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

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
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

static void sigsegv_handler(int sig, siginfo_t *info, void *raw_context)
{
	if (sig != SIGSEGV)
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

	// comex says hello, and is most curious whether this is arm_r10 for a
	// reason as opposed to si_addr like the x64MemTools.cpp version.  Is there
	// even a need for this file to be architecture specific?
	uintptr_t fault_memory_ptr = (uintptr_t)ctx->arm_r10;

	if (!JitInterface::HandleFault(fault_memory_ptr, ctx))
	{
		// retry and crash
		signal(SIGSEGV, SIG_DFL);
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

void UninstallExceptionHandler() {}

}  // namespace
