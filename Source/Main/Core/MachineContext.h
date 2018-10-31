// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

// meh.
#if defined(_M_GENERIC)
// JitBase uses SContext; it should have no concrete implementations in a
// generic build.
struct FakeGenericContext;
typedef FakeGenericContext SContext;
#elif defined(_WIN32)
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
#define CTX_R8 R8
#define CTX_R9 R9
#define CTX_R10 R10
#define CTX_R11 R11
#define CTX_R12 R12
#define CTX_R13 R13
#define CTX_R14 R14
#define CTX_R15 R15
#define CTX_RIP Rip
#else
#error No context definition for architecture
#endif
#elif defined(__APPLE__) && !defined(USE_SIGACTION_ON_APPLE)
// for modules:
#define _XOPEN_SOURCE
#include <ucontext.h>

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
#define CTX_R8 __r8
#define CTX_R9 __r9
#define CTX_R10 __r10
#define CTX_R11 __r11
#define CTX_R12 __r12
#define CTX_R13 __r13
#define CTX_R14 __r14
#define CTX_R15 __r15
#define CTX_RIP __rip
#else
#error No context definition for architecture
#endif
#elif defined(__APPLE__)
#include <signal.h>
typedef _STRUCT_MCONTEXT64 SContext;
#define CTX_RAX __ss.__rax
#define CTX_RBX __ss.__rbx
#define CTX_RCX __ss.__rcx
#define CTX_RDX __ss.__rdx
#define CTX_RDI __ss.__rdi
#define CTX_RSI __ss.__rsi
#define CTX_RBP __ss.__rbp
#define CTX_RSP __ss.__rsp
#define CTX_R8 __ss.__r8
#define CTX_R9 __ss.__r9
#define CTX_R10 __ss.__r10
#define CTX_R11 __ss.__r11
#define CTX_R12 __ss.__r12
#define CTX_R13 __ss.__r13
#define CTX_R14 __ss.__r14
#define CTX_R15 __ss.__r15
#define CTX_RIP __ss.__rip
#elif defined(__linux__)
#include <signal.h>

#include <ucontext.h>
typedef mcontext_t SContext;

#if _M_X86_64
#define CTX_RAX gregs[REG_RAX]
#define CTX_RBX gregs[REG_RBX]
#define CTX_RCX gregs[REG_RCX]
#define CTX_RDX gregs[REG_RDX]
#define CTX_RDI gregs[REG_RDI]
#define CTX_RSI gregs[REG_RSI]
#define CTX_RBP gregs[REG_RBP]
#define CTX_RSP gregs[REG_RSP]
#define CTX_R8 gregs[REG_R8]
#define CTX_R9 gregs[REG_R9]
#define CTX_R10 gregs[REG_R10]
#define CTX_R11 gregs[REG_R11]
#define CTX_R12 gregs[REG_R12]
#define CTX_R13 gregs[REG_R13]
#define CTX_R14 gregs[REG_R14]
#define CTX_R15 gregs[REG_R15]
#define CTX_RIP gregs[REG_RIP]
#elif _M_ARM_64
#define CTX_REG(x) regs[x]
#define CTX_SP sp
#define CTX_PC pc
#else
#error No context definition for architecture
#endif
#elif defined(__OpenBSD__)
#include <signal.h>
typedef ucontext_t SContext;
#if _M_X86_64
#define CTX_RAX sc_rax
#define CTX_RBX sc_rbx
#define CTX_RCX sc_rcx
#define CTX_RDX sc_rdx
#define CTX_RDI sc_rdi
#define CTX_RSI sc_rsi
#define CTX_RBP sc_rbp
#define CTX_RSP sc_rsp
#define CTX_R8 sc_r8
#define CTX_R9 sc_r9
#define CTX_R10 sc_r10
#define CTX_R11 sc_r11
#define CTX_R12 sc_r12
#define CTX_R13 sc_r13
#define CTX_R14 sc_r14
#define CTX_R15 sc_r15
#define CTX_RIP sc_rip
#else
#error No context definition for architecture
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
#define CTX_R8 __gregs[_REG_R8]
#define CTX_R9 __gregs[_REG_R9]
#define CTX_R10 __gregs[_REG_R10]
#define CTX_R11 __gregs[_REG_R11]
#define CTX_R12 __gregs[_REG_R12]
#define CTX_R13 __gregs[_REG_R13]
#define CTX_R14 __gregs[_REG_R14]
#define CTX_R15 __gregs[_REG_R15]
#define CTX_RIP __gregs[_REG_RIP]
#else
#error No context definition for architecture
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
#define CTX_R8 mc_r8
#define CTX_R9 mc_r9
#define CTX_R10 mc_r10
#define CTX_R11 mc_r11
#define CTX_R12 mc_r12
#define CTX_R13 mc_r13
#define CTX_R14 mc_r14
#define CTX_R15 mc_r15
#define CTX_RIP mc_rip
#else
#error No context definition for architecture
#endif
#elif defined(__HAIKU__)
#include <signal.h>
typedef mcontext_t SContext;
#if _M_X86_64
#define CTX_RAX rax
#define CTX_RBX rbx
#define CTX_RCX rcx
#define CTX_RDX rdx
#define CTX_RDI rdi
#define CTX_RSI rsi
#define CTX_RBP rbp
#define CTX_RSP rsp
#define CTX_R8 r8
#define CTX_R9 r9
#define CTX_R10 r10
#define CTX_R11 r11
#define CTX_R12 r12
#define CTX_R13 r13
#define CTX_R14 r14
#define CTX_R15 r15
#define CTX_RIP rip
#else
#error No context definition for machine
#endif
#else
#error No context definition for OS
#endif

#if _M_X86_64
#include <stddef.h>
#define CTX_PC CTX_RIP
static inline u64* ContextRN(SContext* ctx, int n)
{
  static const u8 offsets[] = {
      offsetof(SContext, CTX_RAX), offsetof(SContext, CTX_RCX), offsetof(SContext, CTX_RDX),
      offsetof(SContext, CTX_RBX), offsetof(SContext, CTX_RSP), offsetof(SContext, CTX_RBP),
      offsetof(SContext, CTX_RSI), offsetof(SContext, CTX_RDI), offsetof(SContext, CTX_R8),
      offsetof(SContext, CTX_R9),  offsetof(SContext, CTX_R10), offsetof(SContext, CTX_R11),
      offsetof(SContext, CTX_R12), offsetof(SContext, CTX_R13), offsetof(SContext, CTX_R14),
      offsetof(SContext, CTX_R15)};
  return (u64*)((char*)ctx + offsets[n]);
}
#endif
