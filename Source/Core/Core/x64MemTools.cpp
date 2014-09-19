// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstdio>
#include <vector>

#ifdef __APPLE__
#include "Common/Thread.h"
#endif

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/x64Analyzer.h"

#include "Core/MemTools.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#ifndef _M_GENERIC
#include "Core/PowerPC/JitCommon/JitBase.h"
#endif

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

			// virtual address of the inaccessible data
			uintptr_t badAddress = (uintptr_t)pPtrs->ExceptionRecord->ExceptionInformation[1];
			CONTEXT *ctx = pPtrs->ContextRecord;

			if (JitInterface::HandleFault(badAddress, ctx))
			{
				return (DWORD)EXCEPTION_CONTINUE_EXECUTION;
			}
			else
			{
				// Let's not prevent debugging.
				return (DWORD)EXCEPTION_CONTINUE_SEARCH;
			}
		}

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
	// Make sure this is only called once per process execution
	// Instead, could make a Uninstall function, but whatever..
	static bool handlerInstalled = false;
	if (handlerInstalled)
		return;

	AddVectoredExceptionHandler(TRUE, Handler);
	handlerInstalled = true;
}

void UninstallExceptionHandler() {}

#elif defined(__APPLE__)

void CheckKR(const char* name, kern_return_t kr)
{
	if (kr)
	{
		PanicAlertT("%s failed: kr=%x", name, kr);
	}
}

void ExceptionThread(mach_port_t port)
{
	Common::SetCurrentThreadName("Mach exception thread");
	#pragma pack(4)
	struct
	{
		mach_msg_header_t Head;
		NDR_record_t NDR;
		exception_type_t exception;
		mach_msg_type_number_t codeCnt;
		int64_t code[2];
		int flavor;
		mach_msg_type_number_t old_stateCnt;
		natural_t old_state[x86_THREAD_STATE64_COUNT];
		mach_msg_trailer_t trailer;
	} msg_in;

	struct
	{
		mach_msg_header_t Head;
		NDR_record_t NDR;
		kern_return_t RetCode;
		int flavor;
		mach_msg_type_number_t new_stateCnt;
		natural_t new_state[x86_THREAD_STATE64_COUNT];
	} msg_out;
	#pragma pack()
	memset(&msg_in, 0xee, sizeof(msg_in));
	memset(&msg_out, 0xee, sizeof(msg_out));
	mach_msg_header_t *send_msg = nullptr;
	mach_msg_size_t send_size = 0;
	mach_msg_option_t option = MACH_RCV_MSG;
	while (1)
	{
		// If this isn't the first run, send the reply message.  Then, receive
		// a message: either a mach_exception_raise_state RPC due to
		// thread_set_exception_ports, or MACH_NOTIFY_NO_SENDERS due to
		// mach_port_request_notification.
		CheckKR("mach_msg_overwrite", mach_msg_overwrite(send_msg, option, send_size, sizeof(msg_in), port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL, &msg_in.Head, 0));

		if (msg_in.Head.msgh_id == MACH_NOTIFY_NO_SENDERS)
		{
			// the other thread exited
			mach_port_destroy(mach_task_self(), port);
			return;
		}

		if (msg_in.Head.msgh_id != 2406)
		{
			PanicAlertT("unknown message received");
			return;
		}

		if (msg_in.flavor != x86_THREAD_STATE64)
		{
			PanicAlertT("unknown flavor %d (expected %d)", msg_in.flavor, x86_THREAD_STATE64);
			return;
		}

		x86_thread_state64_t *state = (x86_thread_state64_t *) msg_in.old_state;

		bool ok = JitInterface::HandleFault((uintptr_t) msg_in.code[1], state);

		// Set up the reply.
		msg_out.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(msg_in.Head.msgh_bits), 0);
		msg_out.Head.msgh_remote_port = msg_in.Head.msgh_remote_port;
		msg_out.Head.msgh_local_port = MACH_PORT_NULL;
		msg_out.Head.msgh_id = msg_in.Head.msgh_id + 100;
		msg_out.NDR = msg_in.NDR;
		if (ok)
		{
			msg_out.RetCode = KERN_SUCCESS;
			msg_out.flavor = x86_THREAD_STATE64;
			msg_out.new_stateCnt = x86_THREAD_STATE64_COUNT;
			memcpy(msg_out.new_state, msg_in.old_state, x86_THREAD_STATE64_COUNT * sizeof(natural_t));
		}
		else
		{
			// Pass the exception to the next handler (debugger or crash).
			msg_out.RetCode = KERN_FAILURE;
			msg_out.flavor = 0;
			msg_out.new_stateCnt = 0;
		}
		msg_out.Head.msgh_size = offsetof(typeof(msg_out), new_state) + msg_out.new_stateCnt * sizeof(natural_t);

		send_msg = &msg_out.Head;
		send_size = msg_out.Head.msgh_size;
		option |= MACH_SEND_MSG;
	}
}

void InstallExceptionHandler()
{
	mach_port_t port;
	CheckKR("mach_port_allocate", mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port));
	std::thread exc_thread(ExceptionThread, port);
	exc_thread.detach();
	// Obtain a send right for thread_set_exception_ports to copy...
	CheckKR("mach_port_insert_right", mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND));
	// Mach tries the following exception ports in order: thread, task, host.
	// Debuggers set the task port, so we grab the thread port.
	CheckKR("thread_set_exception_ports", thread_set_exception_ports(mach_thread_self(), EXC_MASK_BAD_ACCESS, port, EXCEPTION_STATE | MACH_EXCEPTION_CODES, x86_THREAD_STATE64));
	// ...and get rid of our copy so that MACH_NOTIFY_NO_SENDERS works.
	CheckKR("mach_port_mod_refs", mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, -1));
	mach_port_t previous;
	CheckKR("mach_port_request_notification", mach_port_request_notification(mach_task_self(), port, MACH_NOTIFY_NO_SENDERS, 0, port, MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous));
}

void UninstallExceptionHandler() {}

#elif defined(_POSIX_VERSION)

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
	uintptr_t bad_address = (uintptr_t)info->si_addr;

	// Get all the information we can out of the context.
	mcontext_t *ctx = &context->uc_mcontext;
	// assume it's not a write
	if (!JitInterface::HandleFault(bad_address, ctx))
	{
		// retry and crash
		signal(SIGSEGV, SIG_DFL);
	}
}

void InstallExceptionHandler()
{
	stack_t signal_stack;
	signal_stack.ss_sp = malloc(SIGSTKSZ);
	signal_stack.ss_size = SIGSTKSZ;
	signal_stack.ss_flags = 0;
	if (sigaltstack(&signal_stack, nullptr))
		PanicAlert("sigaltstack failed");
	struct sigaction sa;
	sa.sa_handler = nullptr;
	sa.sa_sigaction = &sigsegv_handler;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGSEGV, &sa, nullptr);
}

void UninstallExceptionHandler()
{
	stack_t signal_stack, old_stack;
	signal_stack.ss_flags = SS_DISABLE;
	if (!sigaltstack(&signal_stack, &old_stack) &&
	    !(old_stack.ss_flags & SS_DISABLE))
	{
		free(old_stack.ss_sp);
	}
}
#else

#error Unsupported x86_64 platform! Report this if you support sigaction

#endif

}  // namespace
