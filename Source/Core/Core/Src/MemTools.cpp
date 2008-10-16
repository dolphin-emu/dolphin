// Copyright (C) 2003-2008 Dolphin Project.

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


// TODO: create a working OS-neutral version of this file and put it in Common.


#ifdef _WIN32

#include <windows.h>
#include <vector>

#include "Common.h"
#include "MemTools.h"
#include "HW/Memmap.h"
#include "PowerPC/PowerPC.h"
#include "PowerPC/Jit64/Jit.h"
#include "PowerPC/Jit64/JitBackpatch.h"
#include "x64Analyzer.h"

namespace EMM
{

LONG NTAPI Handler(PEXCEPTION_POINTERS pPtrs)
{
	switch (pPtrs->ExceptionRecord->ExceptionCode)
	{
	case EXCEPTION_ACCESS_VIOLATION:
		{
			int accessType = (int)pPtrs->ExceptionRecord->ExceptionInformation[0];
			if (accessType == 8) //Rule out DEP
			{
				if(PowerPC::state == PowerPC::CPU_POWERDOWN) // Access violation during
															 // violent shutdown is fine
					return EXCEPTION_CONTINUE_EXECUTION;

				MessageBox(0, _T("Tried to execute code that's not marked executable. This is likely a JIT bug.\n"), 0, 0);
				return EXCEPTION_CONTINUE_SEARCH;
			}
			
			//Where in the x86 code are we?
			PVOID codeAddr = pPtrs->ExceptionRecord->ExceptionAddress;
			unsigned char *codePtr = (unsigned char*)codeAddr;
			
			if (!Jit64::IsInJitCode(codePtr)) {
				// Let's not prevent debugging.
				return (DWORD)EXCEPTION_CONTINUE_SEARCH;
			}

			//Figure out what address was hit
			u64 badAddress = (u64)pPtrs->ExceptionRecord->ExceptionInformation[1];
			//TODO: First examine the address, make sure it's within the emulated memory space
			u64 memspaceBottom = (u64)Memory::base;
			if (badAddress < memspaceBottom) {
				PanicAlert("Exception handler - access below memory space. %08x%08x",
					badAddress >> 32, badAddress);
			}
			u32 emAddress = (u32)(badAddress - memspaceBottom);

			//Now we have the emulated address.
			//Let's notify everyone who wants to be notified
			//Notify(emAddress, accessType == 0 ? Read : Write);

			CONTEXT *ctx = pPtrs->ContextRecord;
			//opportunity to play with the context - we can change the debug regs!

			//We could emulate the memory accesses here, but then they would still be around to take up
			//execution resources. Instead, we backpatch into a generic memory call and retry.
			Jit64::BackPatch(codePtr, accessType, emAddress);
			
			// We no longer touch Rip, since we return back to the instruction, after overwriting it with a
			// trampoline jump and some nops
			//ctx->Rip = (DWORD_PTR)codeAddr + info.instructionSize;
		}
		return (DWORD)EXCEPTION_CONTINUE_EXECUTION;

	case EXCEPTION_STACK_OVERFLOW:
		MessageBox(0, _T("Stack overflow!"), 0,0);
		return EXCEPTION_CONTINUE_SEARCH;

	case EXCEPTION_ILLEGAL_INSTRUCTION:
		//No SSE support? Or simply bad codegen?
		return EXCEPTION_CONTINUE_SEARCH;
		
	case EXCEPTION_PRIV_INSTRUCTION:
		//okay, dynarec codegen is obviously broken.
		return EXCEPTION_CONTINUE_SEARCH;

	case EXCEPTION_IN_PAGE_ERROR:
		//okay, something went seriously wrong, out of memory?
		return EXCEPTION_CONTINUE_SEARCH;

	case EXCEPTION_BREAKPOINT:
		//might want to do something fun with this one day?
		return EXCEPTION_CONTINUE_SEARCH;

	default:
		return EXCEPTION_CONTINUE_SEARCH;
	}
}

void InstallExceptionHandler()
{
#ifdef _M_X64
	// Make sure this is only called once per process execution
	// Instead, could make a Uninstall function, but whatever..
	static bool handlerInstalled = false;
	if (handlerInstalled)
		return;

	AddVectoredExceptionHandler(TRUE, Handler);
	handlerInstalled = true;
#endif
}

}
#else

namespace EMM {

#if 0
//
// backtrace useful function
//
void print_trace(const char * msg)
{
	void *array[100];
	size_t size;
	char **strings;
	size_t i;

	size = backtrace(array, 100);
	strings = backtrace_symbols(array, size);
	printf("%s Obtained %zd stack frames.\n", msg, size);
	for (i = 0; i < size; i++)
		printf("--> %s\n", strings[i]);
	free(strings);
}

void sigsegv_handler(int signal, int siginfo_t *info, void *raw_context)
{
	if (signal != SIGSEGV)
	{
		// We are not interested in other signals - handle it as usual.
		return;
	}
	ucontext_t *context = (ucontext_t)raw_context;
	int si_code = info->si_code;
	if (si_code != SEGV_MAPERR)
	{
		// Huh? Return.
		return;
	}
	mcontext_t *ctx = &context->uc_mcontext;
	void *fault_memory_ptr = (void *)info->si_addr;
	void *fault_instruction_ptr = (void *)ctx->mc_rip;

	if (!Jit64::IsInJitCode(fault_instruction_ptr)) {
		// Let's not prevent debugging.
		return;
	}

	u64 memspaceBottom = (u64)Memory::base;
	if (badAddress < memspaceBottom) {
		PanicAlert("Exception handler - access below memory space. %08x%08x",
			badAddress >> 32, badAddress);
	}
	u32 emAddress = (u32)(badAddress - memspaceBottom);

	// Backpatch time.
	Jit64::BackPatch(fault_instruction_ptr, accessType, emAddress);
}

#endif

void InstallExceptionHandler()
{
#ifdef _M_IX86
	PanicAlert("InstallExceptionHandler called, but this platform does not yet support it.");
	return;
#endif

#if 0
	sighandler_t old_signal_handler = signal(SIGSEGV , sigsegv_handler);
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigsegv_handler;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGSEGV, &sa, NULL);
#endif

	/*
 * signal(xyz);
 */
}

}

#endif

