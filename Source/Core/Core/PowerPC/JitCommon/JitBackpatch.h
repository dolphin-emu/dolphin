// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/Common.h"
#include "Common/x64Analyzer.h"
#include "Common/x64Emitter.h"

// We need at least this many bytes for backpatching.
const int BACKPATCH_SIZE = 5;

// meh.
#if defined(_WIN32)
	#include <windows.h>
	typedef CONTEXT SContext;
	#if _M_X86_64
		#define CTX_RAX Rax
		#define CTX_RBX Rbx
		#define CTX_RCX Rcx
		#define CTX_RDX Rdx
		#define CTX_RDI Rdi
		#define CTX_RSI Rsi
		#define CTX_RBP Rbp
		#define CTX_RSP Rsp
		#define CTX_R8  R8
		#define CTX_R9  R9
		#define CTX_R10 R10
		#define CTX_R11 R11
		#define CTX_R12 R12
		#define CTX_R13 R13
		#define CTX_R14 R14
		#define CTX_R15 R15
		#define CTX_RIP Rip
	#elif _M_X86_32
		#define CTX_EAX Eax
		#define CTX_EBX Ebx
		#define CTX_ECX Ecx
		#define CTX_EDX Edx
		#define CTX_EDI Edi
		#define CTX_ESI Esi
		#define CTX_EBP Ebp
		#define CTX_ESP Esp
		#define CTX_EIP Eip
	#else
		#error No context definition for OS
	#endif
#elif defined(__APPLE__)
	#include <mach/mach.h>
	#include <mach/message.h>
	#if _M_X86_64
		typedef x86_thread_state64_t SContext;
		#define CTX_RAX __rax
		#define CTX_RBX __rbx
		#define CTX_RCX __rcx
		#define CTX_RDX __rdx
		#define CTX_RDI __rdi
		#define CTX_RSI __rsi
		#define CTX_RBP __rbp
		#define CTX_RSP __rsp
		#define CTX_R8  __r8
		#define CTX_R9  __r9
		#define CTX_R10 __r10
		#define CTX_R11 __r11
		#define CTX_R12 __r12
		#define CTX_R13 __r13
		#define CTX_R14 __r14
		#define CTX_R15 __r15
		#define CTX_RIP __rip
	#elif _M_X86_32
		typedef x86_thread_state32_t SContext;
		#define CTX_EAX __eax
		#define CTX_EBX __ebx
		#define CTX_ECX __ecx
		#define CTX_EDX __edx
		#define CTX_EDI __edi
		#define CTX_ESI __esi
		#define CTX_EBP __ebp
		#define CTX_ESP __esp
		#define CTX_EIP __eip
	#else
		#error No context definition for OS
	#endif
#elif defined(__linux__)
	#include <signal.h>
	#if _M_X86_64
		#include <ucontext.h>
		typedef mcontext_t SContext;
		#define CTX_RAX gregs[REG_RAX]
		#define CTX_RBX gregs[REG_RBX]
		#define CTX_RCX gregs[REG_RCX]
		#define CTX_RDX gregs[REG_RDX]
		#define CTX_RDI gregs[REG_RDI]
		#define CTX_RSI gregs[REG_RSI]
		#define CTX_RBP gregs[REG_RBP]
		#define CTX_RSP gregs[REG_RSP]
		#define CTX_R8  gregs[REG_R8]
		#define CTX_R9  gregs[REG_R9]
		#define CTX_R10 gregs[REG_R10]
		#define CTX_R11 gregs[REG_R11]
		#define CTX_R12 gregs[REG_R12]
		#define CTX_R13 gregs[REG_R13]
		#define CTX_R14 gregs[REG_R14]
		#define CTX_R15 gregs[REG_R15]
		#define CTX_RIP gregs[REG_RIP]
	#elif _M_X86_32
		#ifdef ANDROID
		#include <asm/sigcontext.h>
		typedef sigcontext SContext;
		#define CTX_EAX eax
		#define CTX_EBX ebx
		#define CTX_ECX ecx
		#define CTX_EDX edx
		#define CTX_EDI edi
		#define CTX_ESI esi
		#define CTX_EBP ebp
		#define CTX_ESP esp
		#define CTX_EIP eip
		#else
		#include <ucontext.h>
		typedef mcontext_t SContext;
		#define CTX_EAX gregs[REG_EAX]
		#define CTX_EBX gregs[REG_EBX]
		#define CTX_ECX gregs[REG_ECX]
		#define CTX_EDX gregs[REG_EDX]
		#define CTX_EDI gregs[REG_EDI]
		#define CTX_ESI gregs[REG_ESI]
		#define CTX_EBP gregs[REG_EBP]
		#define CTX_ESP gregs[REG_ESP]
		#define CTX_EIP gregs[REG_EIP]
		#endif
	#elif _M_ARM_32
		// Add others if required.
		typedef struct sigcontext SContext;
		#define CTX_PC  arm_pc
	#else
		#warning No context definition for OS
	#endif
#elif defined(__NetBSD__)
	#include <ucontext.h>
	typedef mcontext_t SContext;
	#if _M_X86_64
		#define CTX_RAX __gregs[_REG_RAX]
		#define CTX_RBX __gregs[_REG_RBX]
		#define CTX_RCX __gregs[_REG_RCX]
		#define CTX_RDX __gregs[_REG_RDX]
		#define CTX_RDI __gregs[_REG_RDI]
		#define CTX_RSI __gregs[_REG_RSI]
		#define CTX_RBP __gregs[_REG_RBP]
		#define CTX_RSP __gregs[_REG_RSP]
		#define CTX_R8  __gregs[_REG_R8]
		#define CTX_R9  __gregs[_REG_R9]
		#define CTX_R10 __gregs[_REG_R10]
		#define CTX_R11 __gregs[_REG_R11]
		#define CTX_R12 __gregs[_REG_R12]
		#define CTX_R13 __gregs[_REG_R13]
		#define CTX_R14 __gregs[_REG_R14]
		#define CTX_R15 __gregs[_REG_R15]
		#define CTX_RIP __gregs[_REG_RIP]
	#elif _M_X86_32
		#define CTX_EAX __gregs[__REG_EAX]
		#define CTX_EBX __gregs[__REG_EBX]
		#define CTX_ECX __gregs[__REG_ECX]
		#define CTX_EDX __gregs[__REG_EDX]
		#define CTX_EDI __gregs[__REG_EDI]
		#define CTX_ESI __gregs[__REG_ESI]
		#define CTX_EBP __gregs[__REG_EBP]
		#define CTX_ESP __gregs[__REG_ESP]
		#define CTX_EIP __gregs[__REG_EIP]
	#else
		#error No context definition for OS
	#endif
#elif defined(__FreeBSD__)
	#include <ucontext.h>
	typedef mcontext_t SContext;
	#if _M_X86_64
		#define CTX_RAX mc_rax
		#define CTX_RBX mc_rbx
		#define CTX_RCX mc_rcx
		#define CTX_RDX mc_rdx
		#define CTX_RDI mc_rdi
		#define CTX_RSI mc_rsi
		#define CTX_RBP mc_rbp
		#define CTX_RSP mc_rsp
		#define CTX_R8  mc_r8
		#define CTX_R9  mc_r9
		#define CTX_R10 mc_r10
		#define CTX_R11 mc_r11
		#define CTX_R12 mc_r12
		#define CTX_R13 mc_r13
		#define CTX_R14 mc_r14
		#define CTX_R15 mc_r15
		#define CTX_RIP mc_rip
	#elif _M_X86_32
		#define CTX_EAX mc_eax
		#define CTX_EBX mc_ebx
		#define CTX_ECX mc_ecx
		#define CTX_EDX mc_edx
		#define CTX_EDI mc_edi
		#define CTX_ESI mc_esi
		#define CTX_EBP mc_ebp
		#define CTX_ESP mc_esp
		#define CTX_EIP mc_eip
	#else
		#error No context definition for OS
	#endif
#endif

#if _M_X86_64
#define CTX_PC CTX_RIP
#include <stddef.h>
static inline u64 *ContextRN(SContext* ctx, int n)
{
	static const u8 offsets[] =
	{
		offsetof(SContext, CTX_RAX),
		offsetof(SContext, CTX_RCX),
		offsetof(SContext, CTX_RDX),
		offsetof(SContext, CTX_RBX),
		offsetof(SContext, CTX_RSP),
		offsetof(SContext, CTX_RBP),
		offsetof(SContext, CTX_RSI),
		offsetof(SContext, CTX_RDI),
		offsetof(SContext, CTX_R8),
		offsetof(SContext, CTX_R9),
		offsetof(SContext, CTX_R10),
		offsetof(SContext, CTX_R11),
		offsetof(SContext, CTX_R12),
		offsetof(SContext, CTX_R13),
		offsetof(SContext, CTX_R14),
		offsetof(SContext, CTX_R15)
	};
	return (u64 *) ((char *) ctx + offsets[n]);
}
#elif _M_X86_32
#define CTX_PC CTX_EIP
#endif

class TrampolineCache : public Gen::X64CodeBlock
{
public:
	void Init();
	void Shutdown();

	const u8 *GetReadTrampoline(const InstructionInfo &info, u32 registersInUse);
	const u8 *GetWriteTrampoline(const InstructionInfo &info, u32 registersInUse);
};
