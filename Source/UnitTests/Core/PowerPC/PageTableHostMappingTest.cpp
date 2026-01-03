// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <bit>
#include <set>
#include <utility>

#include <fmt/format.h>

#include "Common/Align.h"
#include "Common/CommonTypes.h"
#include "Common/Swap.h"
#include "Core/Core.h"
#include "Core/MemTools.h"
#include "Core/PowerPC/BreakPoints.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#include "../StubJit.h"

#include <gtest/gtest.h>

// All guest addresses used in this unit test are arbitrary, aside from alignment requirements
static constexpr u32 ALIGNED_PAGE_TABLE_BASE = 0x00020000;
static constexpr u32 ALIGNED_PAGE_TABLE_MASK_SMALL = 0x0000ffff;
static constexpr u32 ALIGNED_PAGE_TABLE_MASK_LARGE = 0x0001ffff;

static constexpr u32 MISALIGNED_PAGE_TABLE_BASE = 0x00050000;
static constexpr u32 MISALIGNED_PAGE_TABLE_BASE_ALIGNED = 0x00040000;
static constexpr u32 MISALIGNED_PAGE_TABLE_MASK = 0x0003ffff;

static constexpr u32 HOLE_MASK_PAGE_TABLE_BASE = 0x00080000;
static constexpr u32 HOLE_MASK_PAGE_TABLE_MASK = 0x0002ffff;
static constexpr u32 HOLE_MASK_PAGE_TABLE_MASK_WITHOUT_HOLE = 0x0003ffff;

static constexpr u32 MISALIGNED_HOLE_MASK_PAGE_TABLE_BASE = 0x000e0000;
static constexpr u32 MISALIGNED_HOLE_MASK_PAGE_TABLE_BASE_ALIGNED = 0x000d0000;
static constexpr u32 MISALIGNED_HOLE_MASK_PAGE_TABLE_MASK = 0x0002ffff;
static constexpr u32 MISALIGNED_HOLE_MASK_PAGE_TABLE_MASK_WITHOUT_HOLE = 0x0003ffff;

static constexpr u32 TEMPORARY_MEMORY = 0x00000000;
static u32 s_current_temporary_memory = TEMPORARY_MEMORY;

// This is the max that the unit test can handle, not the max that Core can handle
static constexpr u32 MAX_HOST_PAGE_SIZE = 64 * 1024;
static u32 s_minimum_mapping_size = 0;

static volatile const void* volatile s_detection_address = nullptr;
static volatile size_t s_detection_count = 0;
static u32 s_counter = 0;
static std::set<u32> s_temporary_mappings;

class PageFaultDetector : public StubJit
{
public:
  explicit PageFaultDetector(Core::System& system) : StubJit(system), m_block_cache(*this) {}

  bool HandleFault(uintptr_t access_address, SContext* ctx) override
  {
    if (access_address != reinterpret_cast<uintptr_t>(s_detection_address))
    {
      std::string logical_address;
      auto& memory = Core::System::GetInstance().GetMemory();
      auto logical_base = reinterpret_cast<uintptr_t>(memory.GetLogicalBase());
      if (access_address >= logical_base && access_address < logical_base + 0x1'0000'0000)
        logical_address = fmt::format(" (PPC {:#010x})", access_address - logical_base);

      ADD_FAILURE() << fmt::format("Unexpected segfault at {:#x}{}", access_address,
                                   logical_address);

      return false;
    }

    s_detection_address = nullptr;
    s_detection_count = s_detection_count + 1;

    // After we return from the signal handler, the memory access will happen again.
    // Let it succeed this time so the signal handler won't get called over and over.
    auto& memory = Core::System::GetInstance().GetMemory();
    const uintptr_t logical_base = reinterpret_cast<uintptr_t>(memory.GetLogicalBase());
    const u32 logical_address = static_cast<u32>(access_address - logical_base);
    const u32 mask = s_minimum_mapping_size - 1;
    for (u32 i = logical_address & mask; i < s_minimum_mapping_size; i += PowerPC::HW_PAGE_SIZE)
    {
      const u32 current_logical_address = (logical_address & ~mask) + i;
      memory.AddPageTableMapping(current_logical_address, s_current_temporary_memory + i, true);
      s_temporary_mappings.emplace(current_logical_address);
    }
    return true;
  }

  bool WantsPageTableMappings() const override { return true; }

  // PowerPC::MMU::DBATUpdated wants to clear blocks in the block cache,
  // so this can't just return nullptr
  JitBaseBlockCache* GetBlockCache() override { return &m_block_cache; }

private:
  StubBlockCache m_block_cache;
};

// This is used as a performance optimization. If several page table updates are performed while
// DR is disabled, MMU.cpp will only have to rescan the page table one time once DR is enabled again
// instead of after each page table update.
class DisableDR final
{
public:
  DisableDR()
  {
    auto& system = Core::System::GetInstance();
    system.GetPPCState().msr.DR = 0;
    system.GetPowerPC().MSRUpdated();
  }

  ~DisableDR()
  {
    auto& system = Core::System::GetInstance();
    system.GetPPCState().msr.DR = 1;
    system.GetPowerPC().MSRUpdated();
  }
};

class PageTableHostMappingTest : public ::testing::Test
{
public:
  static void SetUpTestSuite()
  {
    if (!EMM::IsExceptionHandlerSupported())
      GTEST_SKIP() << "Skipping PageTableHostMappingTest because exception handler is unsupported.";

    auto& system = Core::System::GetInstance();
    auto& memory = system.GetMemory();
    const u32 host_page_size = memory.GetHostPageSize();
    s_minimum_mapping_size = std::max<u32>(host_page_size, PowerPC::HW_PAGE_SIZE);

    if (!std::has_single_bit(host_page_size) || host_page_size > MAX_HOST_PAGE_SIZE)
    {
      GTEST_SKIP() << fmt::format(
          "Skipping PageTableHostMappingTest because page size {} is unsupported.", host_page_size);
    }

    memory.Init();
    if (!memory.InitFastmemArena())
    {
      memory.Shutdown();
      GTEST_SKIP() << "Skipping PageTableHostMappingTest because InitFastmemArena failed.";
    }

    Core::DeclareAsCPUThread();
    EMM::InstallExceptionHandler();
    system.GetJitInterface().SetJit(std::make_unique<PageFaultDetector>(system));

    // Make sure BATs and SRs are cleared
    auto& power_pc = system.GetPowerPC();
    power_pc.Reset();

    // Set up an SR
    SetSR(1, 123);

    // Specify a page table
    SetSDR(ALIGNED_PAGE_TABLE_BASE, ALIGNED_PAGE_TABLE_MASK_SMALL);

    // Enable address translation
    system.GetPPCState().msr.DR = 1;
    system.GetPPCState().msr.IR = 1;
    power_pc.MSRUpdated();
  }

  static void TearDownTestSuite()
  {
    auto& system = Core::System::GetInstance();

    system.GetJitInterface().SetJit(nullptr);
    EMM::UninstallExceptionHandler();
    Core::UndeclareAsCPUThread();
    system.GetMemory().Shutdown();
  }

  static void SetSR(size_t index, u32 vsid)
  {
    ASSERT_FALSE(index == 4 || index == 7)
        << fmt::format("sr{} has conflicts with fake VMEM mapping", index);

    UReg_SR sr{};
    sr.VSID = vsid;

    auto& system = Core::System::GetInstance();
    system.GetPPCState().sr[index] = sr.Hex;
    system.GetMMU().SRUpdated();
  }

  static void SetSDR(u32 page_table_base, u32 page_table_mask)
  {
    UReg_SDR1 sdr;
    sdr.htabmask = page_table_mask >> 16;
    sdr.reserved = 0;
    sdr.htaborg = page_table_base >> 16;

    auto& system = Core::System::GetInstance();
    system.GetPPCState().spr[SPR_SDR] = sdr.Hex;
    system.GetMMU().SDRUpdated();
  }

  static void SetBAT(u32 spr, UReg_BAT_Up batu, UReg_BAT_Lo batl)
  {
    auto& system = Core::System::GetInstance();
    auto& ppc_state = system.GetPPCState();
    auto& mmu = system.GetMMU();

    ppc_state.spr[spr + 1] = batl.Hex;
    ppc_state.spr[spr] = batu.Hex;

    if ((spr >= SPR_IBAT0U && spr <= SPR_IBAT3L) || (spr >= SPR_IBAT4U && spr <= SPR_IBAT7L))
      mmu.IBATUpdated();
    if ((spr >= SPR_DBAT0U && spr <= SPR_DBAT3L) || (spr >= SPR_DBAT4U && spr <= SPR_DBAT7L))
      mmu.DBATUpdated();
  }

  static void SetBAT(u32 spr, u32 logical_address, u32 physical_address, u32 size)
  {
    UReg_BAT_Up batu{};
    batu.VP = 1;
    batu.VS = 1;
    batu.BL = (size - 1) >> PowerPC::BAT_INDEX_SHIFT;
    batu.BEPI = logical_address >> PowerPC::BAT_INDEX_SHIFT;

    UReg_BAT_Lo batl{};
    batl.PP = 2;
    batl.WIMG = 0;
    batl.BRPN = physical_address >> PowerPC::BAT_INDEX_SHIFT;

    SetBAT(spr, batu, batl);
  }

  static void ExpectMapped(u32 logical_address, u32 physical_address)
  {
    SCOPED_TRACE(
        fmt::format("ExpectMapped({:#010x}, {:#010x})", logical_address, physical_address));

    auto& memory = Core::System::GetInstance().GetMemory();
    u8* physical_base = memory.GetPhysicalBase();
    u8* logical_base = memory.GetLogicalBase();

    auto* physical_ptr = reinterpret_cast<volatile u32*>(physical_base + physical_address);
    auto* logical_ptr = reinterpret_cast<volatile u32*>(logical_base + logical_address);

    *physical_ptr = ++s_counter;
    EXPECT_EQ(*logical_ptr, s_counter)
        << "Page was mapped to a different physical page than expected";

    *logical_ptr = ++s_counter;
    EXPECT_EQ(*physical_ptr, s_counter)
        << "Page was mapped to a different physical page than expected";
  }

#ifdef _MSC_VER
#define ASAN_DISABLE __declspec(no_sanitize_address)
#else
#define ASAN_DISABLE
#endif

  static void ASAN_DISABLE ExpectReadOnlyMapped(u32 logical_address, u32 physical_address)
  {
    SCOPED_TRACE(
        fmt::format("ExpectReadOnlyMapped({:#010x}, {:#010x})", logical_address, physical_address));

    auto& memory = Core::System::GetInstance().GetMemory();
    u8* physical_base = memory.GetPhysicalBase();
    u8* logical_base = memory.GetLogicalBase();

    auto* physical_ptr = reinterpret_cast<volatile u32*>(physical_base + physical_address);
    auto* logical_ptr = reinterpret_cast<volatile u32*>(logical_base + logical_address);

    *physical_ptr = ++s_counter;
    EXPECT_EQ(*logical_ptr, s_counter)
        << "Page was mapped to a different physical page than expected";

    s_detection_address = logical_ptr;
    s_detection_count = 0;

    // This line should fault
    *logical_ptr = ++s_counter;

    memory.RemovePageTableMappings(s_temporary_mappings);
    s_temporary_mappings.clear();
    EXPECT_EQ(s_detection_count, u32(1)) << "Page was mapped as writeable, against expectations";
  }

  static void ASAN_DISABLE ExpectNotMapped(u32 logical_address)
  {
    SCOPED_TRACE(fmt::format("ExpectNotMapped({:#010x})", logical_address));

    auto& memory = Core::System::GetInstance().GetMemory();
    u8* logical_base = memory.GetLogicalBase();

    auto* logical_ptr = reinterpret_cast<volatile u32*>(logical_base + logical_address);
    s_detection_address = logical_ptr;
    s_detection_count = 0;

    // This line should fault
    *logical_ptr;

    memory.RemovePageTableMappings(s_temporary_mappings);
    s_temporary_mappings.clear();
    EXPECT_EQ(s_detection_count, u32(1)) << "Page was mapped, against expectations";
  }

  static void ExpectMappedOnlyIf4KHostPages(u32 logical_address, u32 physical_address)
  {
    if (s_minimum_mapping_size > PowerPC::HW_PAGE_SIZE)
      ExpectNotMapped(logical_address);
    else
      ExpectMapped(logical_address, physical_address);
  }

  static std::pair<UPTE_Lo, UPTE_Hi> GetPTE(u32 logical_address, u32 index)
  {
    auto& system = Core::System::GetInstance();
    auto& ppc_state = system.GetPPCState();

    const UReg_SR sr(system.GetPPCState().sr[logical_address >> 28]);
    u32 hash = sr.VSID ^ (logical_address >> 12);
    if ((index & 0x8) != 0)
      hash = ~hash;

    const u32 pteg_addr = ((hash << 6) & ppc_state.pagetable_mask) | ppc_state.pagetable_base;
    const u32 pte_addr = (index & 0x7) * 8 + pteg_addr;

    const u8* physical_base = system.GetMemory().GetPhysicalBase();
    const UPTE_Lo pte1(Common::swap32(physical_base + pte_addr));
    const UPTE_Hi pte2(Common::swap32(physical_base + pte_addr + 4));

    return {pte1, pte2};
  }

  static void SetPTE(UPTE_Lo pte1, UPTE_Hi pte2, u32 logical_address, u32 index)
  {
    auto& system = Core::System::GetInstance();
    auto& ppc_state = system.GetPPCState();

    pte1.H = (index & 0x8) != 0;

    u32 hash = pte1.VSID ^ (logical_address >> 12);
    if (pte1.H)
      hash = ~hash;

    const u32 pteg_addr = ((hash << 6) & ppc_state.pagetable_mask) | ppc_state.pagetable_base;
    const u32 pte_addr = (index & 0x7) * 8 + pteg_addr;

    u8* physical_base = system.GetMemory().GetPhysicalBase();
    Common::WriteSwap32(physical_base + pte_addr, pte1.Hex);
    Common::WriteSwap32(physical_base + pte_addr + 4, pte2.Hex);

    system.GetMMU().InvalidateTLBEntry(logical_address);
  }

  static std::pair<UPTE_Lo, UPTE_Hi> CreateMapping(u32 logical_address, u32 physical_address)
  {
    auto& ppc_state = Core::System::GetInstance().GetPPCState();

    UPTE_Lo pte1{};
    pte1.API = logical_address >> 22;
    pte1.VSID = UReg_SR{ppc_state.sr[logical_address >> 28]}.VSID;
    pte1.V = 1;  // Mapping is valid

    UPTE_Hi pte2{};
    pte2.C = 1;  // Page has been written to (MMU.cpp won't map as writeable without this)
    pte2.R = 1;  // Page has been read from (MMU.cpp won't map at all without this)
    pte2.RPN = physical_address >> 12;

    return {pte1, pte2};
  }

  static void AddMapping(u32 logical_address, u32 physical_address, u32 index)
  {
    auto [pte1, pte2] = CreateMapping(logical_address, physical_address);
    SetPTE(pte1, pte2, logical_address, index);
  }

  static void AddHostSizedMapping(u32 logical_address, u32 physical_address, u32 index)
  {
    DisableDR disable_dr;
    for (u32 i = 0; i < s_minimum_mapping_size; i += PowerPC::HW_PAGE_SIZE)
      AddMapping(logical_address + i, physical_address + i, index);
  }

  static void RemoveMapping(u32 logical_address, u32 physical_address, u32 index)
  {
    auto [pte1, pte2] = CreateMapping(logical_address, physical_address);
    pte1.V = 0;  // Mapping is invalid
    SetPTE(pte1, pte2, logical_address, index);
  }

  static void RemoveHostSizedMapping(u32 logical_address, u32 physical_address, u32 index)
  {
    DisableDR disable_dr;
    for (u32 i = 0; i < s_minimum_mapping_size; i += PowerPC::HW_PAGE_SIZE)
      RemoveMapping(logical_address + i, physical_address + i, index);
  }
};

TEST_F(PageTableHostMappingTest, Basic)
{
  s_current_temporary_memory = 0x00100000;

  // Create a basic mapping
  for (u32 i = 0; i < s_minimum_mapping_size; i += PowerPC::HW_PAGE_SIZE)
  {
    ExpectNotMapped(0x10100000 + i);
    AddMapping(0x10100000 + i, 0x00100000 + i, 0);
  }
  for (u32 i = 0; i < s_minimum_mapping_size; i += PowerPC::HW_PAGE_SIZE)
    ExpectMapped(0x10100000 + i, 0x00100000 + i);

  // Create another mapping pointing to the same physical address
  for (u32 i = 0; i < s_minimum_mapping_size; i += PowerPC::HW_PAGE_SIZE)
  {
    ExpectNotMapped(0x10100000 + s_minimum_mapping_size + i);
    AddMapping(0x10100000 + s_minimum_mapping_size + i, 0x00100000 + i, 0);
  }
  for (u32 i = 0; i < s_minimum_mapping_size; i += PowerPC::HW_PAGE_SIZE)
  {
    ExpectMapped(0x10100000 + i, 0x00100000 + i);
    ExpectMapped(0x10100000 + s_minimum_mapping_size + i, 0x00100000 + i);
  }

  // Remove the first page
  RemoveMapping(0x10100000, 0x00100000, 0);
  ExpectNotMapped(0x10100000);
  for (u32 i = 0; i < s_minimum_mapping_size; i += PowerPC::HW_PAGE_SIZE)
    ExpectMapped(0x10100000 + s_minimum_mapping_size + i, 0x00100000 + i);

  s_current_temporary_memory = TEMPORARY_MEMORY;
}

TEST_F(PageTableHostMappingTest, LargeHostPageMismatchedAddresses)
{
  {
    DisableDR disable_dr;
    AddMapping(0x10110000, 0x00111000, 0);
    for (u32 i = PowerPC::HW_PAGE_SIZE; i < s_minimum_mapping_size; i += PowerPC::HW_PAGE_SIZE)
      AddMapping(0x10110000 + i, 0x00110000 + i, 0);
  }

  ExpectMappedOnlyIf4KHostPages(0x10110000, 0x00111000);
}

TEST_F(PageTableHostMappingTest, LargeHostPageMisalignedAddresses)
{
  {
    DisableDR disable_dr;
    for (u32 i = 0; i < s_minimum_mapping_size * 2; i += PowerPC::HW_PAGE_SIZE)
      AddMapping(0x10120000 + i, 0x00121000 + i, 0);
  }

  ExpectMappedOnlyIf4KHostPages(0x10120000, 0x00121000);
  ExpectMappedOnlyIf4KHostPages(0x10120000 + s_minimum_mapping_size,
                                0x00121000 + s_minimum_mapping_size);
}

TEST_F(PageTableHostMappingTest, ChangeSR)
{
  {
    DisableDR disable_dr;
    for (u32 i = 0; i < s_minimum_mapping_size; i += PowerPC::HW_PAGE_SIZE)
    {
      auto [pte1, pte2] = CreateMapping(0x20130000 + i, 0x00130000 + i);
      pte1.VSID = 0xabc;
      SetPTE(pte1, pte2, 0x20130000 + i, 0);
    }
  }
  ExpectNotMapped(0x20130000);

  SetSR(2, 0xabc);
  ExpectMapped(0x20130000, 0x00130000);
  ExpectNotMapped(0x30130000);

  SetSR(3, 0xabc);
  ExpectMapped(0x20130000, 0x00130000);
  ExpectMapped(0x30130000, 0x00130000);
  ExpectNotMapped(0x00130000);
  ExpectNotMapped(0x10130000);
}

// DBAT takes priority over page table mappings.
TEST_F(PageTableHostMappingTest, DBATPriority)
{
  SetSR(5, 5);

  AddHostSizedMapping(0x50140000, 0x00150000, 0);
  ExpectMapped(0x50140000, 0x00150000);

  SetBAT(SPR_DBAT0U, 0x50000000, 0x00000000, 0x01000000);
  ExpectMapped(0x50140000, 0x00140000);
}

// Host-side page table mappings are for data only, so IBAT has no effect on them.
TEST_F(PageTableHostMappingTest, IBATPriority)
{
  SetSR(6, 6);

  AddHostSizedMapping(0x60160000, 0x00170000, 0);
  ExpectMapped(0x60160000, 0x00170000);

  SetBAT(SPR_IBAT0U, 0x60000000, 0x00000000, 0x01000000);
  ExpectMapped(0x60160000, 0x00170000);
}

TEST_F(PageTableHostMappingTest, Priority)
{
  // Secondary PTEs for 0x10180000

  AddHostSizedMapping(0x10180000, 0x00180000, 10);
  ExpectMapped(0x10180000, 0x00180000);

  AddHostSizedMapping(0x10180000, 0x00190000, 12);
  ExpectMapped(0x10180000, 0x00180000);

  AddHostSizedMapping(0x10180000, 0x001a0000, 8);
  ExpectMapped(0x10180000, 0x001a0000);

  RemoveHostSizedMapping(0x10180000, 0x00180000, 10);
  ExpectMapped(0x10180000, 0x001a0000);

  RemoveHostSizedMapping(0x10180000, 0x001a0000, 8);
  ExpectMapped(0x10180000, 0x00190000);

  // Primary PTEs for 0x10180000

  AddHostSizedMapping(0x10180000, 0x00180000, 2);
  ExpectMapped(0x10180000, 0x00180000);

  AddHostSizedMapping(0x10180000, 0x001a0000, 4);
  ExpectMapped(0x10180000, 0x00180000);

  AddHostSizedMapping(0x10180000, 0x001b0000, 0);
  ExpectMapped(0x10180000, 0x001b0000);

  RemoveHostSizedMapping(0x10180000, 0x00180000, 2);
  ExpectMapped(0x10180000, 0x001b0000);

  RemoveHostSizedMapping(0x10180000, 0x001b0000, 0);
  ExpectMapped(0x10180000, 0x001a0000);

  // Return to secondary PTE for 0x10180000

  RemoveHostSizedMapping(0x10180000, 0x001a0000, 4);
  ExpectMapped(0x10180000, 0x00190000);

  // Secondary PTEs for 0x11180000

  AddHostSizedMapping(0x11180000, 0x01180000, 11);
  ExpectMapped(0x11180000, 0x01180000);

  AddHostSizedMapping(0x11180000, 0x01190000, 13);
  ExpectMapped(0x11180000, 0x01180000);

  AddHostSizedMapping(0x11180000, 0x011a0000, 9);
  ExpectMapped(0x11180000, 0x011a0000);

  RemoveHostSizedMapping(0x11180000, 0x01180000, 11);
  ExpectMapped(0x11180000, 0x011a0000);

  RemoveHostSizedMapping(0x11180000, 0x011a0000, 9);
  ExpectMapped(0x11180000, 0x01190000);

  // Primary PTEs for 0x11180000

  AddHostSizedMapping(0x11180000, 0x01180000, 3);
  ExpectMapped(0x11180000, 0x01180000);

  AddHostSizedMapping(0x11180000, 0x011a0000, 5);
  ExpectMapped(0x11180000, 0x01180000);

  AddHostSizedMapping(0x11180000, 0x011b0000, 1);
  ExpectMapped(0x11180000, 0x011b0000);

  RemoveHostSizedMapping(0x11180000, 0x01180000, 3);
  ExpectMapped(0x11180000, 0x011b0000);

  RemoveHostSizedMapping(0x11180000, 0x011b0000, 1);
  ExpectMapped(0x11180000, 0x011a0000);

  // Return to secondary PTE for 0x11180000

  RemoveHostSizedMapping(0x11180000, 0x011a0000, 5);
  ExpectMapped(0x11180000, 0x01190000);

  // Check that 0x10180000 is still working properly

  ExpectMapped(0x10180000, 0x00190000);

  AddHostSizedMapping(0x10180000, 0x00180000, 0);
  ExpectMapped(0x10180000, 0x00180000);

  // Check that 0x11180000 is still working properly

  ExpectMapped(0x11180000, 0x01190000);

  AddHostSizedMapping(0x11180000, 0x01180000, 1);
  ExpectMapped(0x11180000, 0x01180000);
}

TEST_F(PageTableHostMappingTest, ChangeAddress)
{
  // Initial mapping
  AddHostSizedMapping(0x101c0000, 0x001c0000, 0);
  ExpectMapped(0x101c0000, 0x001c0000);

  // Change physical address
  AddHostSizedMapping(0x101c0000, 0x001d0000, 0);
  ExpectMapped(0x101c0000, 0x001d0000);

  // Change logical address
  AddHostSizedMapping(0x111c0000, 0x001d0000, 0);
  ExpectMapped(0x111c0000, 0x001d0000);
  ExpectNotMapped(0x101c0000);

  // Change both logical address and physical address
  AddHostSizedMapping(0x101c0000, 0x011d0000, 0);
  ExpectMapped(0x101c0000, 0x011d0000);
  ExpectNotMapped(0x111c0000);
}

TEST_F(PageTableHostMappingTest, InvalidPhysicalAddress)
{
  AddHostSizedMapping(0x101d0000, 0x0ff00000, 0);
  ExpectNotMapped(0x101d0000);
}

TEST_F(PageTableHostMappingTest, WIMG)
{
  for (u32 i = 0; i < 16; ++i)
  {
    {
      DisableDR disable_dr;
      for (u32 j = 0; j < s_minimum_mapping_size; j += PowerPC::HW_PAGE_SIZE)
      {
        auto [pte1, pte2] = CreateMapping(0x101e0000 + j, 0x001e0000 + j);
        pte2.WIMG = i;
        SetPTE(pte1, pte2, 0x101e0000 + j, 0);
      }
    }

    if ((i & 0b1100) != 0)
      ExpectNotMapped(0x101e0000);
    else
      ExpectMapped(0x101e0000, 0x001e0000);
  }
}

TEST_F(PageTableHostMappingTest, RC)
{
  auto& mmu = Core::System::GetInstance().GetMMU();

  const auto set_up_mapping = [] {
    DisableDR disable_dr;
    for (u32 i = 0; i < s_minimum_mapping_size; i += PowerPC::HW_PAGE_SIZE)
    {
      auto [pte1, pte2] = CreateMapping(0x101f0000 + i, 0x001f0000 + i);
      pte2.R = 0;
      pte2.C = 0;
      SetPTE(pte1, pte2, 0x101f0000 + i, 0);
    }
  };

  const auto expect_rc = [](u32 r, u32 c) {
    auto [pte1, pte2] = GetPTE(0x101f0000, 0);
    EXPECT_TRUE(pte1.V);
    EXPECT_EQ(pte2.R, r);
    EXPECT_EQ(pte2.C, c);
  };

  // Start with R=0, C=0
  set_up_mapping();
  ExpectNotMapped(0x101f0000);
  expect_rc(0, 0);

  // Automatically set R=1, C=0
  for (u32 i = 0; i < s_minimum_mapping_size; i += PowerPC::HW_PAGE_SIZE)
    mmu.Read<u32>(0x101f0000 + i);
  ExpectReadOnlyMapped(0x101f0000, 0x001f0000);
  expect_rc(1, 0);

  // Automatically set R=1, C=1
  for (u32 i = 0; i < s_minimum_mapping_size; i += PowerPC::HW_PAGE_SIZE)
    mmu.Write<u32>(0x12345678, 0x101f0000 + i);
  ExpectMapped(0x101f0000, 0x001f0000);
  expect_rc(1, 1);

  // Start over with R=0, C=0
  set_up_mapping();
  ExpectNotMapped(0x101f0000);
  expect_rc(0, 0);

  // Automatically set R=1, C=1
  for (u32 i = 0; i < s_minimum_mapping_size; i += PowerPC::HW_PAGE_SIZE)
    mmu.Write<u32>(0x12345678, 0x101f0000 + i);
  ExpectMapped(0x101f0000, 0x001f0000);
  expect_rc(1, 1);
}

TEST_F(PageTableHostMappingTest, ResizePageTable)
{
  AddHostSizedMapping(0x10200000, 0x00200000, 0);
  AddHostSizedMapping(0x10600000, 0x00210000, 1);
  ExpectMapped(0x10200000, 0x00200000);
  ExpectMapped(0x10600000, 0x00210000);

  SetSDR(ALIGNED_PAGE_TABLE_BASE, ALIGNED_PAGE_TABLE_MASK_LARGE);
  ExpectMapped(0x10200000, 0x00200000);
  ExpectNotMapped(0x10600000);

  AddHostSizedMapping(0x10600000, 0x00220000, 1);
  ExpectMapped(0x10200000, 0x00200000);
  ExpectMapped(0x10600000, 0x00220000);

  AddHostSizedMapping(0x10610000, 0x00200000, 1);
  ExpectMapped(0x10610000, 0x00200000);

  SetSDR(ALIGNED_PAGE_TABLE_BASE, ALIGNED_PAGE_TABLE_MASK_SMALL);
  ExpectMapped(0x10200000, 0x00200000);
  ExpectMapped(0x10600000, 0x00210000);
  ExpectNotMapped(0x10610000);
}

// The PEM says that all bits that are one in the page table mask must be zero in the page table
// address. What it doesn't tell you is that if this isn't obeyed, the Gekko will do a logical OR of
// the page table base and the page table offset, producing behavior that might not be intuitive.
TEST_F(PageTableHostMappingTest, MisalignedPageTable)
{
  SetSDR(MISALIGNED_PAGE_TABLE_BASE + 0x10000, PowerPC::PAGE_TABLE_MIN_SIZE - 1);

  AddHostSizedMapping(0x10a30000, 0x00230000, 4);
  ExpectMapped(0x10a30000, 0x00230000);

  SetSDR(MISALIGNED_PAGE_TABLE_BASE, MISALIGNED_PAGE_TABLE_MASK);

  ExpectNotMapped(0x10a30000);

  AddHostSizedMapping(0x10230000, 0x00240000, 0);
  AddHostSizedMapping(0x10630000, 0x00250000, 1);
  AddHostSizedMapping(0x10a30000, 0x00260000, 2);
  AddHostSizedMapping(0x10e30000, 0x00270000, 3);

  ExpectMapped(0x10230000, 0x00240000);
  ExpectMapped(0x10630000, 0x00250000);
  ExpectMapped(0x10a30000, 0x00260000);
  ExpectMapped(0x10e30000, 0x00270000);

  // Exercise the code for falling back to a secondary PTE after removing a primary PTE.
  AddHostSizedMapping(0x10a30000, 0x00270000, 10);
  AddHostSizedMapping(0x10e30000, 0x00260000, 11);
  RemoveHostSizedMapping(0x10a30000, 0x00260000, 2);
  RemoveHostSizedMapping(0x10e30000, 0x00250000, 3);

  ExpectMapped(0x10230000, 0x00240000);
  ExpectMapped(0x10630000, 0x00250000);
  ExpectMapped(0x10a30000, 0x00270000);
  ExpectMapped(0x10e30000, 0x00260000);

  SetSDR(MISALIGNED_PAGE_TABLE_BASE_ALIGNED, MISALIGNED_PAGE_TABLE_MASK);

  ExpectNotMapped(0x10230000);
  ExpectMapped(0x10630000, 0x00250000);
  ExpectMapped(0x10a30000, 0x00230000);
  ExpectNotMapped(0x10e30000);

  SetSDR(ALIGNED_PAGE_TABLE_BASE, ALIGNED_PAGE_TABLE_MASK_SMALL);
}

// Putting a zero in the middle of the page table mask's ones results in similar behavior
// to the scenario described above.
TEST_F(PageTableHostMappingTest, HoleInMask)
{
  SetSDR(HOLE_MASK_PAGE_TABLE_BASE + 0x10000, PowerPC::PAGE_TABLE_MIN_SIZE - 1);

  AddHostSizedMapping(0x10680000, 0x00280000, 4);
  ExpectMapped(0x10680000, 0x00280000);

  SetSDR(HOLE_MASK_PAGE_TABLE_BASE, HOLE_MASK_PAGE_TABLE_MASK);

  ExpectNotMapped(0x10680000);

  AddHostSizedMapping(0x10280000, 0x00290000, 0);
  AddHostSizedMapping(0x10680000, 0x002a0000, 1);
  AddHostSizedMapping(0x10a80000, 0x002b0000, 2);
  AddHostSizedMapping(0x10e80000, 0x002c0000, 3);

  ExpectMapped(0x10280000, 0x00290000);
  ExpectMapped(0x10680000, 0x002a0000);
  ExpectMapped(0x10a80000, 0x002b0000);
  ExpectMapped(0x10e80000, 0x002c0000);

  // Exercise the code for falling back to a secondary PTE after removing a primary PTE.
  AddHostSizedMapping(0x10a80000, 0x002c0000, 10);
  AddHostSizedMapping(0x10e80000, 0x002b0000, 11);
  RemoveHostSizedMapping(0x10a80000, 0x002b0000, 2);
  RemoveHostSizedMapping(0x10e80000, 0x002c0000, 3);

  ExpectMapped(0x10280000, 0x00290000);
  ExpectMapped(0x10680000, 0x002a0000);
  ExpectMapped(0x10a80000, 0x002c0000);
  ExpectMapped(0x10e80000, 0x002b0000);

  SetSDR(HOLE_MASK_PAGE_TABLE_BASE, HOLE_MASK_PAGE_TABLE_MASK_WITHOUT_HOLE);

  ExpectMapped(0x10280000, 0x00290000);
  ExpectMapped(0x10680000, 0x00280000);
  ExpectNotMapped(0x10a80000);
  ExpectMapped(0x10e80000, 0x002b0000);

  SetSDR(ALIGNED_PAGE_TABLE_BASE, ALIGNED_PAGE_TABLE_MASK_SMALL);
}

// If we combine the two scenarios above, both making the base misaligned and putting a hole in the
// mask, we get the same result as if we just make the base misaligned.
TEST_F(PageTableHostMappingTest, HoleInMaskMisalignedPageTable)
{
  SetSDR(MISALIGNED_PAGE_TABLE_BASE + 0x10000, PowerPC::PAGE_TABLE_MIN_SIZE - 1);

  AddHostSizedMapping(0x10ad0000, 0x002d0000, 4);
  ExpectMapped(0x10ad0000, 0x002d0000);

  SetSDR(MISALIGNED_PAGE_TABLE_BASE, MISALIGNED_PAGE_TABLE_MASK);

  ExpectNotMapped(0x10ad0000);

  AddHostSizedMapping(0x102d0000, 0x002e0000, 0);
  AddHostSizedMapping(0x106d0000, 0x002f0000, 1);
  AddHostSizedMapping(0x10ad0000, 0x00300000, 2);
  AddHostSizedMapping(0x10ed0000, 0x00310000, 3);

  ExpectMapped(0x102d0000, 0x002e0000);
  ExpectMapped(0x106d0000, 0x002f0000);
  ExpectMapped(0x10ad0000, 0x00300000);
  ExpectMapped(0x10ed0000, 0x00310000);

  // Exercise the code for falling back to a secondary PTE after removing a primary PTE.
  AddHostSizedMapping(0x10ad0000, 0x00310000, 10);
  AddHostSizedMapping(0x10ed0000, 0x00300000, 11);
  RemoveHostSizedMapping(0x10ad0000, 0x00300000, 2);
  RemoveHostSizedMapping(0x10ed0000, 0x00310000, 3);

  ExpectMapped(0x102d0000, 0x002e0000);
  ExpectMapped(0x106d0000, 0x002f0000);
  ExpectMapped(0x10ad0000, 0x00310000);
  ExpectMapped(0x10ed0000, 0x00300000);

  SetSDR(MISALIGNED_PAGE_TABLE_BASE_ALIGNED, MISALIGNED_PAGE_TABLE_MASK);

  ExpectNotMapped(0x102d0000);
  ExpectMapped(0x106d0000, 0x002f0000);
  ExpectMapped(0x10ad0000, 0x002d0000);
  ExpectNotMapped(0x10ed0000);

  SetSDR(ALIGNED_PAGE_TABLE_BASE, ALIGNED_PAGE_TABLE_MASK_SMALL);
}

TEST_F(PageTableHostMappingTest, MemChecks)
{
  AddHostSizedMapping(0x10320000, 0x00330000, 0);
  AddHostSizedMapping(0x10330000, 0x00320000, 0);
  ExpectMapped(0x10320000, 0x00330000);
  ExpectMapped(0x10330000, 0x00320000);

  auto& memchecks = Core::System::GetInstance().GetPowerPC().GetMemChecks();
  TMemCheck memcheck;
  memcheck.start_address = 0x10320000;
  memcheck.end_address = 0x10320001;
  memchecks.Add(std::move(memcheck));

  ExpectNotMapped(0x10320000);
  ExpectMapped(0x10330000, 0x00320000);

  memchecks.Remove(0x10320000);

  ExpectMapped(0x10320000, 0x00330000);
  ExpectMapped(0x10330000, 0x00320000);
}
