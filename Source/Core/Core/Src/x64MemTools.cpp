// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifdef _WIN32
#include <windows.h>
#else
#include <stdio.h>
#include <signal.h>
#ifndef ANDROID
#include <sys/ucontext.h>   // Look in here for the context definition.
#endif
#endif

#ifdef __APPLE__
#define CREG_RAX(ctx) (*(ctx))->__ss.__rax
#define CREG_RIP(ctx) (*(ctx))->__ss.__rip
#define CREG_EAX(ctx) (*(ctx))->__ss.__eax
#define CREG_EIP(ctx) (*(ctx))->__ss.__eip
#elif defined __FreeBSD__
#define CREG_RAX(ctx) (ctx)->mc_rax
#define CREG_RIP(ctx) (ctx)->mc_rip
#define CREG_EAX(ctx) (ctx)->mc_eax
#define CREG_EIP(ctx) (ctx)->mc_eip
#elif defined __linux__
#define CREG_RAX(ctx) (ctx)->gregs[REG_RAX]
#define CREG_RIP(ctx) (ctx)->gregs[REG_RIP]
#define CREG_EAX(ctx) (ctx)->gregs[REG_EAX]
#define CREG_EIP(ctx) (ctx)->gregs[REG_EIP]
#elif defined __NetBSD__
#define CREG_RAX(ctx) (ctx)->__gregs[_REG_RAX]
#define CREG_RIP(ctx) (ctx)->__gregs[_REG_RIP]
#define CREG_EAX(ctx) (ctx)->__gregs[_REG_EAX]
#define CREG_EIP(ctx) (ctx)->__gregs[_REG_EIP]
#endif

#include <vector>

#include "Common.h"
#include "MemTools.h"
#include "HW/Memmap.h"
#include "PowerPC/PowerPC.h"
#include "PowerPC/JitInterface.h"
#ifndef _M_GENERIC
#include "PowerPC/JitCommon/JitBase.h"
#endif
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
				return (DWORD)EXCEPTION_CONTINUE_SEARCH;
			}
			
			//Where in the x86 code are we?
			PVOID codeAddr = pPtrs->ExceptionRecord->ExceptionAddress;
			unsigned char *codePtr = (unsigned char*)codeAddr;
			
			if (!JitInterface::IsInCodeSpace(codePtr))
			{
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
			if (badAddress < memspaceBottom || badAddress >= memspaceTop)
			{
				return (DWORD)EXCEPTION_CONTINUE_SEARCH;
				//PanicAlert("Exception handler - access outside memory space. %08x%08x",
				//	badAddress >> 32, badAddress);
			}

			u32 emAddress = (u32)(badAddress - memspaceBottom);

			//Now we have the emulated address.

			CONTEXT *ctx = pPtrs->ContextRecord;
			//opportunity to play with the context - we can change the debug regs!

			//We could emulate the memory accesses here, but then they would still be around to take up
			//execution resources. Instead, we backpatch into a generic memory call and retry.
			const u8 *new_rip = JitInterface::BackPatch(codePtr, accessType, emAddress, ctx);

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

#else  // _WIN32

#ifndef ANDROID
#if defined __APPLE__ || defined __linux__ || defined __FreeBSD__ || defined _WIN32
#ifndef _WIN32
#include <execinfo.h>
#endif
void print_trace(const char * msg)
{
	void *array[100];
	size_t size;
	char **strings;
	size_t i;

	size = backtrace(array, 100);
	strings = backtrace_symbols(array, size);
	printf("%s Obtained %u stack frames.\n", msg, (unsigned int)size);
	for (i = 0; i < size; i++)
		printf("--> %s\n", strings[i]);
	free(strings);
}
#endif

void sigsegv_handler(int signal, siginfo_t *info, void *raw_context)
{
#ifndef _M_GENERIC
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
	void *fault_memory_ptr = (void *)info->si_addr;

	// Get all the information we can out of the context.
	mcontext_t *ctx = &context->uc_mcontext;
#ifdef _M_X64
	u8 *fault_instruction_ptr = (u8 *)CREG_RIP(ctx);
#else
	u8 *fault_instruction_ptr = (u8 *)CREG_EIP(ctx);
#endif
	if (!JitInterface::IsInCodeSpace(fault_instruction_ptr))
	{
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

	// Backpatch time.
	// Seems we'll need to disassemble to get access_type - that work is probably
	// best done and debugged on the Windows side.
	int access_type = 0;

	CONTEXT fake_ctx;

#ifdef _M_X64
	fake_ctx.Rax = CREG_RAX(ctx);
	fake_ctx.Rip = CREG_RIP(ctx);
#else
	fake_ctx.Eax = CREG_EAX(ctx);
	fake_ctx.Eip = CREG_EIP(ctx);
#endif
	const u8 *new_rip = jit->BackPatch(fault_instruction_ptr, access_type, em_address, &fake_ctx);
	if (new_rip)
	{
#ifdef _M_X64
		CREG_RAX(ctx) = fake_ctx.Rax;
		CREG_RIP(ctx) = fake_ctx.Rip;
#else
		CREG_EAX(ctx) = fake_ctx.Eax;
		CREG_EIP(ctx) = fake_ctx.Eip;
#endif
	}
#endif
}
#endif

void InstallExceptionHandler()
{
#ifdef _M_IX86
	PanicAlertT("InstallExceptionHandler called, but this platform does not yet support it.");
	return;
#else
	struct sigaction sa;
	sa.sa_handler = 0;
	sa.sa_sigaction = &sigsegv_handler;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGSEGV, &sa, NULL);
#endif
}

#endif

}  // namespace
