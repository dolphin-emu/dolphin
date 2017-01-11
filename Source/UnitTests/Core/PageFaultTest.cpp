// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <chrono>

#include "Common/CommonTypes.h"
#include "Common/Timer.h"
#include "Core/MemTools.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

// include order is important
#include <gtest/gtest.h>  // NOLINT

enum
{
#ifdef _WIN32
  PAGE_GRAN = 0x10000
#else
  PAGE_GRAN = 0x1000
#endif
};

class PageFaultFakeJit : public JitBase
{
public:
  // CPUCoreBase methods
  void Init() override {}
  void Shutdown() override {}
  void ClearCache() override {}
  void Run() override {}
  void SingleStep() override {}
  const char* GetName() override { return nullptr; }
  // JitBase methods
  JitBaseBlockCache* GetBlockCache() override { return nullptr; }
  void Jit(u32 em_address) override {}
  const CommonAsmRoutinesBase* GetAsmRoutines() override { return nullptr; }
  virtual bool HandleFault(uintptr_t access_address, SContext* ctx) override
  {
    m_pre_unprotect_time = std::chrono::high_resolution_clock::now();
    Common::UnWriteProtectMemory(m_data, PAGE_GRAN, /*allowExecute*/ false);
    m_post_unprotect_time = std::chrono::high_resolution_clock::now();
    return true;
  }

  void* m_data;
  std::chrono::time_point<std::chrono::high_resolution_clock> m_pre_unprotect_time,
      m_post_unprotect_time;
};

TEST(PageFault, PageFault)
{
  EMM::InstallExceptionHandler();
  void* data = Common::AllocateMemoryPages(PAGE_GRAN);
  EXPECT_NE(data, nullptr);
  Common::WriteProtectMemory(data, PAGE_GRAN, false);

  PageFaultFakeJit pfjit;
  g_jit = &pfjit;
  pfjit.m_data = data;

  auto start = std::chrono::high_resolution_clock::now();
  *(volatile int*)data = 5;
  auto end = std::chrono::high_resolution_clock::now();

#define AS_NS(diff)                                                                                \
  ((unsigned long long)std::chrono::duration_cast<std::chrono::nanoseconds>(diff).count())

  EMM::UninstallExceptionHandler();
  g_jit = nullptr;

  printf("page fault timing:\n");
  printf("start->HandleFault     %llu ns\n", AS_NS(pfjit.m_pre_unprotect_time - start));
  printf("UnWriteProtectMemory   %llu ns\n",
         AS_NS(pfjit.m_post_unprotect_time - pfjit.m_pre_unprotect_time));
  printf("HandleFault->end       %llu ns\n", AS_NS(end - pfjit.m_post_unprotect_time));
  printf("total                  %llu ns\n", AS_NS(end - start));
}
