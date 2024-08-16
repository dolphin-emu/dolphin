// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Thread.h"

#ifdef _WIN32
#include <Windows.h>
#include <processthreadsapi.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach/mach.h>
#elif defined BSD4_4 || defined __FreeBSD__ || defined __OpenBSD__
#include <pthread_np.h>
#elif defined __NetBSD__
#include <sched.h>
#elif defined __HAIKU__
#include <OS.h>
#endif

#ifdef USE_VTUNE
#include <ittnotify.h>
#pragma comment(lib, "libittnotify.lib")
#endif

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

namespace Common
{
int CurrentThreadId()
{
#ifdef _WIN32
  return GetCurrentThreadId();
#elif defined __APPLE__
  return mach_thread_self();
#else
  return 0;
#endif
}

#ifdef _WIN32

void SetThreadAffinity(std::thread::native_handle_type thread, u32 mask)
{
  SetThreadAffinityMask(thread, mask);
}

void SetCurrentThreadAffinity(u32 mask)
{
  SetThreadAffinityMask(GetCurrentThread(), mask);
}

// Supporting functions
void SleepCurrentThread(int ms)
{
  Sleep(ms);
}

void SwitchCurrentThread()
{
  SwitchToThread();
}

// Sets the debugger-visible name of the current thread.
// Uses trick documented in:
// https://docs.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code
static void SetCurrentThreadNameViaException(const char* name)
{
  static constexpr DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push, 8)
  struct THREADNAME_INFO
  {
    DWORD dwType;      // must be 0x1000
    LPCSTR szName;     // pointer to name (in user addr space)
    DWORD dwThreadID;  // thread ID (-1=caller thread)
    DWORD dwFlags;     // reserved for future use, must be zero
  } info;
#pragma pack(pop)

  info.dwType = 0x1000;
  info.szName = name;
  info.dwThreadID = static_cast<DWORD>(-1);
  info.dwFlags = 0;

  __try
  {
    RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
  }
  __except (EXCEPTION_CONTINUE_EXECUTION)
  {
  }
}

static void SetCurrentThreadNameViaApi(const char* name)
{
  // If possible, also set via the newer API. On some versions of Server it needs to be manually
  // resolved. This API allows being able to observe the thread name even if a debugger wasn't
  // attached when the name was set (see above link for more info).
  static auto pSetThreadDescription = (decltype(&SetThreadDescription))GetProcAddress(
      GetModuleHandleA("kernel32"), "SetThreadDescription");
  if (pSetThreadDescription)
  {
    pSetThreadDescription(GetCurrentThread(), UTF8ToWString(name).c_str());
  }
}

void SetCurrentThreadName(const char* name)
{
  SetCurrentThreadNameViaException(name);
  SetCurrentThreadNameViaApi(name);
}

#else  // !WIN32, so must be POSIX threads

void SetThreadAffinity(std::thread::native_handle_type thread, u32 mask)
{
#ifdef __APPLE__
  thread_policy_set(pthread_mach_thread_np(thread), THREAD_AFFINITY_POLICY, (integer_t*)&mask, 1);
#elif (defined __linux__ || defined BSD4_4 || defined __FreeBSD__ || defined __NetBSD__) &&        \
    !(defined ANDROID)
#ifndef __NetBSD__
#ifdef __FreeBSD__
  cpuset_t cpu_set;
#else
  cpu_set_t cpu_set;
#endif
  CPU_ZERO(&cpu_set);

  for (int i = 0; i != sizeof(mask) * 8; ++i)
    if ((mask >> i) & 1)
      CPU_SET(i, &cpu_set);

  pthread_setaffinity_np(thread, sizeof(cpu_set), &cpu_set);
#else
  cpuset_t* cpu_set = cpuset_create();

  for (int i = 0; i != sizeof(mask) * 8; ++i)
    if ((mask >> i) & 1)
      cpuset_set(i, cpu_set);

  pthread_setaffinity_np(thread, cpuset_size(cpu_set), cpu_set);
  cpuset_destroy(cpu_set);
#endif
#endif
}

void SetCurrentThreadAffinity(u32 mask)
{
  SetThreadAffinity(pthread_self(), mask);
}

void SleepCurrentThread(int ms)
{
  usleep(1000 * ms);
}

void SwitchCurrentThread()
{
  usleep(1000 * 1);
}

void SetCurrentThreadName(const char* name)
{
#ifdef __APPLE__
  pthread_setname_np(name);
#elif defined __FreeBSD__ || defined __OpenBSD__
  pthread_set_name_np(pthread_self(), name);
#elif defined(__NetBSD__)
  pthread_setname_np(pthread_self(), "%s", const_cast<char*>(name));
#elif defined __HAIKU__
  rename_thread(find_thread(nullptr), name);
#else
  // linux doesn't allow to set more than 16 bytes, including \0.
  pthread_setname_np(pthread_self(), std::string(name).substr(0, 15).c_str());
#endif
#ifdef USE_VTUNE
  // VTune uses OS thread names by default but probably supports longer names when set via its own
  // API.
  __itt_thread_set_name(name);
#endif
}

std::tuple<void*, size_t> GetCurrentThreadStack()
{
  void* stack_addr;
  size_t stack_size;

  pthread_t self = pthread_self();

#ifdef __APPLE__
  stack_size = pthread_get_stacksize_np(self);
  stack_addr = reinterpret_cast<u8*>(pthread_get_stackaddr_np(self)) - stack_size;
#elif defined __OpenBSD__
  stack_t stack;
  pthread_stackseg_np(self, &stack);

  stack_addr = reinterpret_cast<u8*>(stack.ss_sp) - stack.ss_size;
  stack_size = stack.ss_size;
#else
  pthread_attr_t attr;

#ifdef __FreeBSD__
  pthread_attr_init(&attr);
  pthread_attr_get_np(self, &attr);
#else
  // Linux and NetBSD
  pthread_getattr_np(self, &attr);
#endif

  pthread_attr_getstack(&attr, &stack_addr, &stack_size);

  pthread_attr_destroy(&attr);
#endif

  return std::make_tuple(stack_addr, stack_size);
}

#endif

}  // namespace Common
