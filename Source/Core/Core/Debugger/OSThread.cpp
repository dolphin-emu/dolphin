// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Debugger/OSThread.h"

#include <algorithm>
#include <fmt/format.h>

#include "Core/PowerPC/MMU.h"

namespace Core::Debug
{
// Context offsets based on the following functions:
//  - OSSaveContext
//  - OSSaveFPUContext
//  - OSDumpContext
//  - OSClearContext
//  - OSExceptionVector
void OSContext::Read(const Core::CPUThreadGuard& guard, u32 addr)
{
  for (std::size_t i = 0; i < gpr.size(); i++)
    gpr[i] = PowerPC::MMU::HostRead_U32(guard, addr + u32(i * sizeof(int)));
  cr = PowerPC::MMU::HostRead_U32(guard, addr + 0x80);
  lr = PowerPC::MMU::HostRead_U32(guard, addr + 0x84);
  ctr = PowerPC::MMU::HostRead_U32(guard, addr + 0x88);
  xer = PowerPC::MMU::HostRead_U32(guard, addr + 0x8C);
  for (std::size_t i = 0; i < fpr.size(); i++)
    fpr[i] = PowerPC::MMU::HostRead_F64(guard, addr + 0x90 + u32(i * sizeof(double)));
  fpscr = PowerPC::MMU::HostRead_U64(guard, addr + 0x190);
  srr0 = PowerPC::MMU::HostRead_U32(guard, addr + 0x198);
  srr1 = PowerPC::MMU::HostRead_U32(guard, addr + 0x19c);
  dummy = PowerPC::MMU::HostRead_U16(guard, addr + 0x1a0);
  state = static_cast<OSContext::State>(PowerPC::MMU::HostRead_U16(guard, addr + 0x1a2));
  for (std::size_t i = 0; i < gqr.size(); i++)
    gqr[i] = PowerPC::MMU::HostRead_U32(guard, addr + 0x1a4 + u32(i * sizeof(int)));
  psf_padding = 0;
  for (std::size_t i = 0; i < psf.size(); i++)
    psf[i] = PowerPC::MMU::HostRead_F64(guard, addr + 0x1c8 + u32(i * sizeof(double)));
}

// Mutex offsets based on the following functions:
//  - OSInitMutex
//  - OSLockMutex
//  - __OSUnlockAllMutex
void OSMutex::Read(const Core::CPUThreadGuard& guard, u32 addr)
{
  thread_queue.head = PowerPC::MMU::HostRead_U32(guard, addr);
  thread_queue.tail = PowerPC::MMU::HostRead_U32(guard, addr + 0x4);
  owner_addr = PowerPC::MMU::HostRead_U32(guard, addr + 0x8);
  lock_count = PowerPC::MMU::HostRead_U32(guard, addr + 0xc);
  link.next = PowerPC::MMU::HostRead_U32(guard, addr + 0x10);
  link.prev = PowerPC::MMU::HostRead_U32(guard, addr + 0x14);
}

// Thread offsets based on the following functions:
//  - OSCreateThread
//  - OSIsThreadTerminated
//  - OSJoinThread
//  - OSSuspendThread
//  - OSSetPriority
//  - OSExitThread
//  - OSLockMutex
//  - __OSUnlockAllMutex
//  - __OSThreadInit
//  - OSSetThreadSpecific
//  - SOInit (for errno)
void OSThread::Read(const Core::CPUThreadGuard& guard, u32 addr)
{
  context.Read(guard, addr);
  state = PowerPC::MMU::HostRead_U16(guard, addr + 0x2c8);
  is_detached = PowerPC::MMU::HostRead_U16(guard, addr + 0x2ca);
  suspend = PowerPC::MMU::HostRead_U32(guard, addr + 0x2cc);
  effective_priority = PowerPC::MMU::HostRead_U32(guard, addr + 0x2d0);
  base_priority = PowerPC::MMU::HostRead_U32(guard, addr + 0x2d4);
  exit_code_addr = PowerPC::MMU::HostRead_U32(guard, addr + 0x2d8);

  queue_addr = PowerPC::MMU::HostRead_U32(guard, addr + 0x2dc);
  queue_link.next = PowerPC::MMU::HostRead_U32(guard, addr + 0x2e0);
  queue_link.prev = PowerPC::MMU::HostRead_U32(guard, addr + 0x2e4);

  join_queue.head = PowerPC::MMU::HostRead_U32(guard, addr + 0x2e8);
  join_queue.tail = PowerPC::MMU::HostRead_U32(guard, addr + 0x2ec);

  mutex_addr = PowerPC::MMU::HostRead_U32(guard, addr + 0x2f0);
  mutex_queue.head = PowerPC::MMU::HostRead_U32(guard, addr + 0x2f4);
  mutex_queue.tail = PowerPC::MMU::HostRead_U32(guard, addr + 0x2f8);

  thread_link.next = PowerPC::MMU::HostRead_U32(guard, addr + 0x2fc);
  thread_link.prev = PowerPC::MMU::HostRead_U32(guard, addr + 0x300);

  stack_addr = PowerPC::MMU::HostRead_U32(guard, addr + 0x304);
  stack_end = PowerPC::MMU::HostRead_U32(guard, addr + 0x308);
  error = PowerPC::MMU::HostRead_U32(guard, addr + 0x30c);
  specific[0] = PowerPC::MMU::HostRead_U32(guard, addr + 0x310);
  specific[1] = PowerPC::MMU::HostRead_U32(guard, addr + 0x314);
}

bool OSThread::IsValid(const Core::CPUThreadGuard& guard) const
{
  return PowerPC::MMU::HostIsRAMAddress(guard, stack_end) &&
         PowerPC::MMU::HostRead_U32(guard, stack_end) == STACK_MAGIC;
}

OSThreadView::OSThreadView(const Core::CPUThreadGuard& guard, u32 addr)
{
  m_address = addr;
  m_thread.Read(guard, addr);
}

const OSThread& OSThreadView::Data() const
{
  return m_thread;
}

Common::Debug::PartialContext OSThreadView::GetContext(const Core::CPUThreadGuard& guard) const
{
  Common::Debug::PartialContext context;

  if (!IsValid(guard))
    return context;

  context.gpr = m_thread.context.gpr;
  context.cr = m_thread.context.cr;
  context.lr = m_thread.context.lr;
  context.ctr = m_thread.context.ctr;
  context.xer = m_thread.context.xer;
  context.fpr = m_thread.context.fpr;
  context.fpscr = m_thread.context.fpscr;
  context.srr0 = m_thread.context.srr0;
  context.srr1 = m_thread.context.srr1;
  context.dummy = m_thread.context.dummy;
  context.state = static_cast<u16>(m_thread.context.state);
  context.gqr = m_thread.context.gqr;
  context.psf = m_thread.context.psf;

  return context;
}

u32 OSThreadView::GetAddress() const
{
  return m_address;
}

u16 OSThreadView::GetState() const
{
  return m_thread.state;
}

bool OSThreadView::IsSuspended() const
{
  return m_thread.suspend > 0;
}

bool OSThreadView::IsDetached() const
{
  return m_thread.is_detached != 0;
}

s32 OSThreadView::GetBasePriority() const
{
  return m_thread.base_priority;
}

s32 OSThreadView::GetEffectivePriority() const
{
  return m_thread.effective_priority;
}

u32 OSThreadView::GetStackStart() const
{
  return m_thread.stack_addr;
}

u32 OSThreadView::GetStackEnd() const
{
  return m_thread.stack_end;
}

std::size_t OSThreadView::GetStackSize() const
{
  return GetStackStart() - GetStackEnd();
}

s32 OSThreadView::GetErrno() const
{
  return m_thread.error;
}

std::string OSThreadView::GetSpecific(const Core::CPUThreadGuard& guard) const
{
  std::string specific;

  for (u32 addr : m_thread.specific)
  {
    if (!PowerPC::MMU::HostIsRAMAddress(guard, addr))
      break;
    specific += fmt::format("{:08x} \"{}\"\n", addr, PowerPC::MMU::HostGetString(guard, addr));
  }

  return specific;
}

bool OSThreadView::IsValid(const Core::CPUThreadGuard& guard) const
{
  return m_thread.IsValid(guard);
}

}  // namespace Core::Debug
