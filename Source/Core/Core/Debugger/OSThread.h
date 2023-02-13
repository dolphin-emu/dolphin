// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>
#include <type_traits>

#include "Common/CommonTypes.h"
#include "Common/Debug/Threads.h"

namespace Core
{
class CPUThreadGuard;
};

namespace Core::Debug
{
template <class C>
struct OSQueue
{
  u32 head;
  u32 tail;
};
template <class C>
struct OSLink
{
  u32 next;
  u32 prev;
};

struct OSMutex;
struct OSThread;

using OSThreadQueue = OSQueue<OSThread>;
using OSThreadLink = OSLink<OSThread>;

using OSMutexQueue = OSQueue<OSMutex>;
using OSMutexLink = OSLink<OSMutex>;

struct OSContext
{
  enum class State : u16
  {
    HasFPU = 1,
    HasException = 2,
  };
  std::array<u32, 32> gpr;
  u32 cr;
  u32 lr;
  u32 ctr;
  u32 xer;
  std::array<double, 32> fpr;
  u64 fpscr;
  u32 srr0;
  u32 srr1;
  u16 dummy;
  State state;
  std::array<u32, 8> gqr;
  u32 psf_padding;
  std::array<double, 32> psf;

  void Read(const Core::CPUThreadGuard& guard, u32 addr);
};

static_assert(std::is_trivially_copyable_v<OSContext>);
static_assert(std::is_standard_layout_v<OSContext>);
static_assert(offsetof(OSContext, cr) == 0x80);
static_assert(offsetof(OSContext, fpscr) == 0x190);
static_assert(offsetof(OSContext, gqr) == 0x1a4);
static_assert(offsetof(OSContext, psf) == 0x1c8);

struct OSThread
{
  OSContext context;

  u16 state;               // Thread state (ready, running, waiting, moribund)
  u16 is_detached;         // Is thread detached
  s32 suspend;             // Suspended if greater than zero
  s32 effective_priority;  // Effective priority
  s32 base_priority;       // Base priority
  u32 exit_code_addr;      // Exit value address

  u32 queue_addr;           // Address of the queue the thread is on
  OSThreadLink queue_link;  // Used to traverse the thread queue
  // OSSleepThread uses it to insert the current thread at the end of the thread queue

  OSThreadQueue join_queue;  // Threads waiting to be joined

  u32 mutex_addr;            // Mutex waiting
  OSMutexQueue mutex_queue;  // Mutex owned

  OSThreadLink thread_link;  // Link containing all active threads

  // The STACK_MAGIC is written at stack_end
  u32 stack_addr;
  u32 stack_end;

  s32 error;                    // errno value
  std::array<u32, 2> specific;  // Pointers to data (can be used to store thread names)

  static constexpr u32 STACK_MAGIC = 0xDEADBABE;
  void Read(const Core::CPUThreadGuard& guard, u32 addr);
  bool IsValid(const Core::CPUThreadGuard& guard) const;
};

static_assert(std::is_trivially_copyable_v<OSThread>);
static_assert(std::is_standard_layout_v<OSThread>);
static_assert(offsetof(OSThread, state) == 0x2c8);
static_assert(offsetof(OSThread, mutex_addr) == 0x2f0);
static_assert(offsetof(OSThread, stack_addr) == 0x304);
static_assert(offsetof(OSThread, specific) == 0x310);

struct OSMutex
{
  OSThreadQueue thread_queue;  // Threads waiting to own the mutex
  u32 owner_addr;              // Thread owning the mutex
  s32 lock_count;              // Mutex lock count
  OSMutexLink link;            // Used to traverse the thread's mutex queue
  // OSLockMutex uses it to insert the acquired mutex at the end of the queue

  void Read(const Core::CPUThreadGuard& guard, u32 addr);
};

static_assert(std::is_trivially_copyable_v<OSMutex>);
static_assert(std::is_standard_layout_v<OSMutex>);
static_assert(offsetof(OSMutex, owner_addr) == 0x8);
static_assert(offsetof(OSMutex, link) == 0x10);

class OSThreadView : public Common::Debug::ThreadView
{
public:
  explicit OSThreadView(const Core::CPUThreadGuard& guard, u32 addr);
  ~OSThreadView() = default;

  const OSThread& Data() const;

  Common::Debug::PartialContext GetContext(const Core::CPUThreadGuard& guard) const override;
  u32 GetAddress() const override;
  u16 GetState() const override;
  bool IsSuspended() const override;
  bool IsDetached() const override;
  s32 GetBasePriority() const override;
  s32 GetEffectivePriority() const override;
  u32 GetStackStart() const override;
  u32 GetStackEnd() const override;
  std::size_t GetStackSize() const override;
  s32 GetErrno() const override;
  std::string GetSpecific(const Core::CPUThreadGuard& guard) const override;
  bool IsValid(const Core::CPUThreadGuard& guard) const override;

private:
  u32 m_address = 0;
  OSThread m_thread;
};

}  // namespace Core::Debug
