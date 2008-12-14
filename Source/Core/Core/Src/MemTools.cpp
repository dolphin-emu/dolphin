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


#ifdef _WIN32

#include <windows.h>

#else

#include <execinfo.h>
#include <stdio.h>
#include <signal.h>
#include <sys/ucontext.h>   // Look in here for the context definition.

#endif

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

#ifdef _WIN32

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
#ifdef _M_X64
			u64 memspaceTop = memspaceBottom + 0x100000000ULL;
#else
			u64 memspaceTop = memspaceBottom + 0x40000000;
#endif
			if (badAddress < memspaceBottom || badAddress >= memspaceTop) {
				PanicAlert("Exception handler - access outside memory space. %08x%08x",
					badAddress >> 32, badAddress);
			}

			u32 emAddress = (u32)(badAddress - memspaceBottom);

			//Now we have the emulated address.

			CONTEXT *ctx = pPtrs->ContextRecord;
			//opportunity to play with the context - we can change the debug regs!

			//We could emulate the memory accesses here, but then they would still be around to take up
			//execution resources. Instead, we backpatch into a generic memory call and retry.
			u8 *new_rip = Jit64::BackPatch(codePtr, accessType, emAddress, ctx);

			// Rip/Eip needs to be updated.
			if (new_rip)
#ifdef _M_X64
				ctx->Rip = (DWORD_PTR)new_rip;
#else
				ctx->Eip = (DWORD_PTR)new_rip;
#endif
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

#else  // _WIN32)

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

void sigsegv_handler(int signal, siginfo_t *info, void *raw_context)
{
	if (signal != SIGSEGV)
	{
		// We are not interested in other signals - handle it as usual.
		return;
	}
	ucontext_t *context = (ucontext_t *)raw_context;
	int si_code = info->si_code;
	if (si_code != SEGV_MAPERR && si_code != SEGV_ACCERR)
	{
		// Huh? Return.
		return;
	}
	void *fault_memory_ptr = (void *)info->si_addr;

	// Get all the information we can out of the context.
	mcontext_t *ctx = &context->uc_mcontext;
	u8 *fault_instruction_ptr = (u8 *)ctx->gregs[REG_RIP];

	if (!Jit64::IsInJitCode((const u8 *)fault_instruction_ptr)) {
		// Let's not prevent debugging.
		return;
	}
	
	u64 bad_address = (u64)fault_memory_ptr;
	u64 memspace_bottom = (u64)Memory::base;
	if (bad_address < memspace_bottom) {
		PanicAlert("Exception handler - access below memory space. %08x%08x",
			bad_address >> 32, bad_address);
	}
	u32 em_address = (u32)(bad_address - memspace_bottom);

	// Backpatch time.
	// Seems we'll need to disassemble to get access_type - that work is probably
	// best done and debugged on the Windows side.
	int access_type = 0;

	CONTEXT fake_ctx;
#ifdef _M_X64
	fake_ctx.Rax = ctx->gregs[REG_RAX];
	fake_ctx.Rip = ctx->gregs[REG_RIP];
#else
	fake_ctx.Eax = ctx->gregs[REG_EAX];
	fake_ctx.Eip = ctx->gregs[REG_EIP];
#endif
	u8 *new_rip = Jit64::BackPatch(fault_instruction_ptr, access_type, em_address, &fake_ctx);
	if (new_rip) {
#ifdef _M_X64
		ctx->gregs[REG_RAX] = fake_ctx.Rax;
		ctx->gregs[REG_RIP]	= (u64)new_rip;
#else
		ctx->gregs[REG_EAX] = fake_ctx.Rax;
		ctx->gregs[REG_EIP]	= (u32)new_rip;
#endif
	}
}

void InstallExceptionHandler()
{
#ifdef _M_IX86
	PanicAlert("InstallExceptionHandler called, but this platform does not yet support it.");
	return;
#endif
	struct sigaction sa;
	sa.sa_handler = 0;
	sa.sa_sigaction = &sigsegv_handler;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGSEGV, &sa, NULL);
}

#endif

}  // namespace
