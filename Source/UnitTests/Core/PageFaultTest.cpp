// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>
#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/ScopeGuard.h"
#include "Common/Timer.h"
#include "Core/Core.h"
#include "Core/MemTools.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/System.h"

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
  explicit PageFaultFakeJit(Core::System& system) : JitBase(system) {}

  // CPUCoreBase methods
  void Init() override {}
  void Shutdown() override {}
  void ClearCache() override {}
  void Run() override {}
  void SingleStep() override {}
  const char* GetName() const override { return nullptr; }
  // JitBase methods
  JitBaseBlockCache* GetBlockCache() override { return nullptr; }
  void Jit(u32 em_address) override {}
  void EraseSingleBlock(const JitBlock&) override {}
  std::vector<MemoryStats> GetMemoryStats() const override { return {}; }
  std::size_t DisasmNearCode(const JitBlock&, std::ostream&) const override { return 0; }
  std::size_t DisasmFarCode(const JitBlock&, std::ostream&) const override { return 0; }
  const CommonAsmRoutinesBase* GetAsmRoutines() override { return nullptr; }
  virtual bool HandleFault(uintptr_t access_address, SContext* ctx) override
  {
    m_pre_unprotect_time = std::chrono::high_resolution_clock::now();
    Common::UnWriteProtectMemory(m_data, PAGE_GRAN, /*allowExecute*/ false);
    m_post_unprotect_time = std::chrono::high_resolution_clock::now();
    return true;
  }

  void* m_data = nullptr;
  std::chrono::time_point<std::chrono::high_resolution_clock> m_pre_unprotect_time,
      m_post_unprotect_time;
};

#ifdef _MSC_VER
#define ASAN_DISABLE __declspec(no_sanitize_address)
#else
#define ASAN_DISABLE
#endif

static void ASAN_DISABLE perform_invalid_access(void* data)
{
  *(volatile int*)data = 5;
}

TEST(PageFault, PageFault)
{
  if (!EMM::IsExceptionHandlerSupported())
    GTEST_SKIP() << "Skipping PageFault test because exception handler is unsupported.";

  EMM::InstallExceptionHandler();
  void* data = Common::AllocateMemoryPages(PAGE_GRAN);
  EXPECT_NE(data, nullptr);
  Common::WriteProtectMemory(data, PAGE_GRAN, false);

  Core::DeclareAsCPUThread();
  Common::ScopeGuard cpu_thread_guard([] { Core::UndeclareAsCPUThread(); });

  auto& system = Core::System::GetInstance();
  auto unique_pfjit = std::make_unique<PageFaultFakeJit>(system);
  auto& pfjit = *unique_pfjit;
  system.GetJitInterface().SetJit(std::move(unique_pfjit));
  pfjit.m_data = data;

  auto start = std::chrono::high_resolution_clock::now();
  perform_invalid_access(data);
  auto end = std::chrono::high_resolution_clock::now();

  auto difference_in_nanoseconds = [](auto start, auto end) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  };

  EMM::UninstallExceptionHandler();

  fmt::print("page fault timing:\n");
  fmt::print("start->HandleFault     {} ns\n",
             difference_in_nanoseconds(start, pfjit.m_pre_unprotect_time));
  fmt::print("UnWriteProtectMemory   {} ns\n",
             difference_in_nanoseconds(pfjit.m_pre_unprotect_time, pfjit.m_post_unprotect_time));
  fmt::print("HandleFault->end       {} ns\n",
             difference_in_nanoseconds(pfjit.m_post_unprotect_time, end));
  fmt::print("total                  {} ns\n", difference_in_nanoseconds(start, end));

  system.GetJitInterface().SetJit(nullptr);
}
