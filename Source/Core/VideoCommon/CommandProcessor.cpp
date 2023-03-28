// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/CommandProcessor.h"

#include <atomic>
#include <cstring>
#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"
#include "VideoCommon/Fifo.h"

namespace CommandProcessor
{
static bool IsOnThread(Core::System& system)
{
  return system.IsDualCoreMode();
}

static void UpdateInterrupts_Wrapper(Core::System& system, u64 userdata, s64 cyclesLate)
{
  system.GetCommandProcessor().UpdateInterrupts(system, userdata);
}

void SCPFifoStruct::Init()
{
  CPBase = 0;
  CPEnd = 0;
  CPHiWatermark = 0;
  CPLoWatermark = 0;
  CPReadWriteDistance = 0;
  CPWritePointer = 0;
  CPReadPointer = 0;
  CPBreakpoint = 0;
  SafeCPReadPointer = 0;

  bFF_GPLinkEnable = 0;
  bFF_GPReadEnable = 0;
  bFF_BPEnable = 0;
  bFF_BPInt = 0;

  bFF_Breakpoint.store(0, std::memory_order_relaxed);
  bFF_HiWatermark.store(0, std::memory_order_relaxed);
  bFF_HiWatermarkInt.store(0, std::memory_order_relaxed);
  bFF_LoWatermark.store(0, std::memory_order_relaxed);
  bFF_LoWatermarkInt.store(0, std::memory_order_relaxed);
}

void SCPFifoStruct::DoState(PointerWrap& p)
{
  p.Do(CPBase);
  p.Do(CPEnd);
  p.Do(CPHiWatermark);
  p.Do(CPLoWatermark);
  p.Do(CPReadWriteDistance);
  p.Do(CPWritePointer);
  p.Do(CPReadPointer);
  p.Do(CPBreakpoint);
  p.Do(SafeCPReadPointer);

  p.Do(bFF_GPLinkEnable);
  p.Do(bFF_GPReadEnable);
  p.Do(bFF_BPEnable);
  p.Do(bFF_BPInt);
  p.Do(bFF_Breakpoint);

  p.Do(bFF_LoWatermarkInt);
  p.Do(bFF_HiWatermarkInt);

  p.Do(bFF_LoWatermark);
  p.Do(bFF_HiWatermark);
}

void CommandProcessorManager::DoState(PointerWrap& p)
{
  p.Do(m_cp_status_reg);
  p.Do(m_cp_ctrl_reg);
  p.Do(m_cp_clear_reg);
  p.Do(m_bbox_left);
  p.Do(m_bbox_top);
  p.Do(m_bbox_right);
  p.Do(m_bbox_bottom);
  p.Do(m_token_reg);
  m_fifo.DoState(p);

  p.Do(m_interrupt_set);
  p.Do(m_interrupt_waiting);
}

static inline void WriteHigh(std::atomic<u32>& reg, u16 highbits)
{
  reg.store((reg.load(std::memory_order_relaxed) & 0x0000FFFF) | (static_cast<u32>(highbits) << 16),
            std::memory_order_relaxed);
}

void CommandProcessorManager::Init(Core::System& system)
{
  m_cp_status_reg.Hex = 0;
  m_cp_status_reg.CommandIdle = 1;
  m_cp_status_reg.ReadIdle = 1;

  m_cp_ctrl_reg.Hex = 0;

  m_cp_clear_reg.Hex = 0;

  m_bbox_left = 0;
  m_bbox_top = 0;
  m_bbox_right = 640;
  m_bbox_bottom = 480;

  m_token_reg = 0;

  m_fifo.Init();

  m_is_fifo_error_seen = false;

  m_interrupt_set.Clear();
  m_interrupt_waiting.Clear();

  m_event_type_update_interrupts =
      system.GetCoreTiming().RegisterEvent("CPInterrupt", UpdateInterrupts_Wrapper);
}

u32 GetPhysicalAddressMask()
{
  // Physical addresses in CP seem to ignore some of the upper bits (depending on platform)
  // This can be observed in CP MMIO registers by setting to 0xffffffff and then reading back.
  return SConfig::GetInstance().bWii ? 0x1fffffff : 0x03ffffff;
}

void CommandProcessorManager::RegisterMMIO(Core::System& system, MMIO::Mapping* mmio, u32 base)
{
  constexpr u16 WMASK_NONE = 0x0000;
  constexpr u16 WMASK_ALL = 0xffff;
  constexpr u16 WMASK_LO_ALIGN_32BIT = 0xffe0;
  const u16 WMASK_HI_RESTRICT = GetPhysicalAddressMask() >> 16;

  auto& fifo = m_fifo;

  struct
  {
    u32 addr;
    u16* ptr;
    bool readonly;
    // FIFO mmio regs in the range [cc000020-cc00003e] have certain bits that always read as 0
    // For _LO registers in this range, only bits 0xffe0 can be set
    // For _HI registers in this range, only bits 0x03ff can be set on GCN and 0x1fff on Wii
    u16 wmask;
  } directly_mapped_vars[] = {
      {FIFO_TOKEN_REGISTER, &m_token_reg, false, WMASK_ALL},

      // Bounding box registers are read only.
      {FIFO_BOUNDING_BOX_LEFT, &m_bbox_left, true, WMASK_NONE},
      {FIFO_BOUNDING_BOX_RIGHT, &m_bbox_right, true, WMASK_NONE},
      {FIFO_BOUNDING_BOX_TOP, &m_bbox_top, true, WMASK_NONE},
      {FIFO_BOUNDING_BOX_BOTTOM, &m_bbox_bottom, true, WMASK_NONE},
      {FIFO_BASE_LO, MMIO::Utils::LowPart(&fifo.CPBase), false, WMASK_LO_ALIGN_32BIT},
      {FIFO_BASE_HI, MMIO::Utils::HighPart(&fifo.CPBase), false, WMASK_HI_RESTRICT},
      {FIFO_END_LO, MMIO::Utils::LowPart(&fifo.CPEnd), false, WMASK_LO_ALIGN_32BIT},
      {FIFO_END_HI, MMIO::Utils::HighPart(&fifo.CPEnd), false, WMASK_HI_RESTRICT},
      {FIFO_HI_WATERMARK_LO, MMIO::Utils::LowPart(&fifo.CPHiWatermark), false,
       WMASK_LO_ALIGN_32BIT},
      {FIFO_HI_WATERMARK_HI, MMIO::Utils::HighPart(&fifo.CPHiWatermark), false, WMASK_HI_RESTRICT},
      {FIFO_LO_WATERMARK_LO, MMIO::Utils::LowPart(&fifo.CPLoWatermark), false,
       WMASK_LO_ALIGN_32BIT},
      {FIFO_LO_WATERMARK_HI, MMIO::Utils::HighPart(&fifo.CPLoWatermark), false, WMASK_HI_RESTRICT},
      // FIFO_RW_DISTANCE has some complex read code different for
      // single/dual core.
      {FIFO_WRITE_POINTER_LO, MMIO::Utils::LowPart(&fifo.CPWritePointer), false,
       WMASK_LO_ALIGN_32BIT},
      {FIFO_WRITE_POINTER_HI, MMIO::Utils::HighPart(&fifo.CPWritePointer), false,
       WMASK_HI_RESTRICT},
      // FIFO_READ_POINTER has different code for single/dual core.
      {FIFO_BP_LO, MMIO::Utils::LowPart(&fifo.CPBreakpoint), false, WMASK_LO_ALIGN_32BIT},
      {FIFO_BP_HI, MMIO::Utils::HighPart(&fifo.CPBreakpoint), false, WMASK_HI_RESTRICT},
  };

  for (auto& mapped_var : directly_mapped_vars)
  {
    mmio->Register(base | mapped_var.addr, MMIO::DirectRead<u16>(mapped_var.ptr),
                   mapped_var.readonly ? MMIO::InvalidWrite<u16>() :
                                         MMIO::DirectWrite<u16>(mapped_var.ptr, mapped_var.wmask));
  }

  // Timing and metrics MMIOs are stubbed with fixed values.
  struct
  {
    u32 addr;
    u16 value;
  } metrics_mmios[] = {
      {XF_RASBUSY_L, 0},
      {XF_RASBUSY_H, 0},
      {XF_CLKS_L, 0},
      {XF_CLKS_H, 0},
      {XF_WAIT_IN_L, 0},
      {XF_WAIT_IN_H, 0},
      {XF_WAIT_OUT_L, 0},
      {XF_WAIT_OUT_H, 0},
      {VCACHE_METRIC_CHECK_L, 0},
      {VCACHE_METRIC_CHECK_H, 0},
      {VCACHE_METRIC_MISS_L, 0},
      {VCACHE_METRIC_MISS_H, 0},
      {VCACHE_METRIC_STALL_L, 0},
      {VCACHE_METRIC_STALL_H, 0},
      {CLKS_PER_VTX_OUT, 4},
  };
  for (auto& metrics_mmio : metrics_mmios)
  {
    mmio->Register(base | metrics_mmio.addr, MMIO::Constant<u16>(metrics_mmio.value),
                   MMIO::InvalidWrite<u16>());
  }

  mmio->Register(base | STATUS_REGISTER, MMIO::ComplexRead<u16>([](Core::System& system_, u32) {
                   auto& cp = system_.GetCommandProcessor();
                   system_.GetFifo().SyncGPUForRegisterAccess(system_);
                   cp.SetCpStatusRegister(system_);
                   return cp.m_cp_status_reg.Hex;
                 }),
                 MMIO::InvalidWrite<u16>());

  mmio->Register(base | CTRL_REGISTER, MMIO::DirectRead<u16>(&m_cp_ctrl_reg.Hex),
                 MMIO::ComplexWrite<u16>([](Core::System& system_, u32, u16 val) {
                   auto& cp = system_.GetCommandProcessor();
                   UCPCtrlReg tmp(val);
                   cp.m_cp_ctrl_reg.Hex = tmp.Hex;
                   cp.SetCpControlRegister(system_);
                   system_.GetFifo().RunGpu(system_);
                 }));

  mmio->Register(base | CLEAR_REGISTER, MMIO::DirectRead<u16>(&m_cp_clear_reg.Hex),
                 MMIO::ComplexWrite<u16>([](Core::System& system_, u32, u16 val) {
                   auto& cp = system_.GetCommandProcessor();
                   UCPClearReg tmp(val);
                   cp.m_cp_clear_reg.Hex = tmp.Hex;
                   cp.SetCpClearRegister();
                   system_.GetFifo().RunGpu(system_);
                 }));

  mmio->Register(base | PERF_SELECT, MMIO::InvalidRead<u16>(), MMIO::Nop<u16>());

  // Some MMIOs have different handlers for single core vs. dual core mode.
  const bool is_on_thread = IsOnThread(system);
  MMIO::ReadHandlingMethod<u16>* fifo_rw_distance_lo_r;
  if (is_on_thread)
  {
    fifo_rw_distance_lo_r = MMIO::ComplexRead<u16>([](Core::System& system_, u32) {
      const auto& fifo_ = system_.GetCommandProcessor().GetFifo();
      if (fifo_.CPWritePointer.load(std::memory_order_relaxed) >=
          fifo_.SafeCPReadPointer.load(std::memory_order_relaxed))
      {
        return static_cast<u16>(fifo_.CPWritePointer.load(std::memory_order_relaxed) -
                                fifo_.SafeCPReadPointer.load(std::memory_order_relaxed));
      }
      else
      {
        return static_cast<u16>(fifo_.CPEnd.load(std::memory_order_relaxed) -
                                fifo_.SafeCPReadPointer.load(std::memory_order_relaxed) +
                                fifo_.CPWritePointer.load(std::memory_order_relaxed) -
                                fifo_.CPBase.load(std::memory_order_relaxed) + 32);
      }
    });
  }
  else
  {
    fifo_rw_distance_lo_r = MMIO::DirectRead<u16>(MMIO::Utils::LowPart(&fifo.CPReadWriteDistance));
  }
  mmio->Register(base | FIFO_RW_DISTANCE_LO, fifo_rw_distance_lo_r,
                 MMIO::DirectWrite<u16>(MMIO::Utils::LowPart(&fifo.CPReadWriteDistance),
                                        WMASK_LO_ALIGN_32BIT));

  MMIO::ReadHandlingMethod<u16>* fifo_rw_distance_hi_r;
  if (is_on_thread)
  {
    fifo_rw_distance_hi_r = MMIO::ComplexRead<u16>([](Core::System& system_, u32) {
      const auto& fifo_ = system_.GetCommandProcessor().GetFifo();
      system_.GetFifo().SyncGPUForRegisterAccess(system_);
      if (fifo_.CPWritePointer.load(std::memory_order_relaxed) >=
          fifo_.SafeCPReadPointer.load(std::memory_order_relaxed))
      {
        return (fifo_.CPWritePointer.load(std::memory_order_relaxed) -
                fifo_.SafeCPReadPointer.load(std::memory_order_relaxed)) >>
               16;
      }
      else
      {
        return (fifo_.CPEnd.load(std::memory_order_relaxed) -
                fifo_.SafeCPReadPointer.load(std::memory_order_relaxed) +
                fifo_.CPWritePointer.load(std::memory_order_relaxed) -
                fifo_.CPBase.load(std::memory_order_relaxed) + 32) >>
               16;
      }
    });
  }
  else
  {
    fifo_rw_distance_hi_r = MMIO::ComplexRead<u16>([](Core::System& system_, u32) {
      const auto& fifo_ = system_.GetCommandProcessor().GetFifo();
      system_.GetFifo().SyncGPUForRegisterAccess(system_);
      return fifo_.CPReadWriteDistance.load(std::memory_order_relaxed) >> 16;
    });
  }
  mmio->Register(base | FIFO_RW_DISTANCE_HI, fifo_rw_distance_hi_r,
                 MMIO::ComplexWrite<u16>([WMASK_HI_RESTRICT](Core::System& system_, u32, u16 val) {
                   auto& fifo_ = system_.GetCommandProcessor().GetFifo();
                   system_.GetFifo().SyncGPUForRegisterAccess(system_);
                   WriteHigh(fifo_.CPReadWriteDistance, val & WMASK_HI_RESTRICT);
                   system_.GetFifo().RunGpu(system_);
                 }));

  mmio->Register(
      base | FIFO_READ_POINTER_LO,
      is_on_thread ? MMIO::DirectRead<u16>(MMIO::Utils::LowPart(&fifo.SafeCPReadPointer)) :
                     MMIO::DirectRead<u16>(MMIO::Utils::LowPart(&fifo.CPReadPointer)),
      MMIO::DirectWrite<u16>(MMIO::Utils::LowPart(&fifo.CPReadPointer), WMASK_LO_ALIGN_32BIT));

  MMIO::ReadHandlingMethod<u16>* fifo_read_hi_r;
  MMIO::WriteHandlingMethod<u16>* fifo_read_hi_w;
  if (is_on_thread)
  {
    fifo_read_hi_r = MMIO::ComplexRead<u16>([](Core::System& system_, u32) {
      auto& fifo_ = system_.GetCommandProcessor().GetFifo();
      system_.GetFifo().SyncGPUForRegisterAccess(system_);
      return fifo_.SafeCPReadPointer.load(std::memory_order_relaxed) >> 16;
    });
    fifo_read_hi_w =
        MMIO::ComplexWrite<u16>([WMASK_HI_RESTRICT](Core::System& system_, u32, u16 val) {
          auto& fifo_ = system_.GetCommandProcessor().GetFifo();
          system_.GetFifo().SyncGPUForRegisterAccess(system_);
          WriteHigh(fifo_.CPReadPointer, val & WMASK_HI_RESTRICT);
          fifo_.SafeCPReadPointer.store(fifo_.CPReadPointer.load(std::memory_order_relaxed),
                                        std::memory_order_relaxed);
        });
  }
  else
  {
    fifo_read_hi_r = MMIO::ComplexRead<u16>([](Core::System& system_, u32) {
      const auto& fifo_ = system_.GetCommandProcessor().GetFifo();
      system_.GetFifo().SyncGPUForRegisterAccess(system_);
      return fifo_.CPReadPointer.load(std::memory_order_relaxed) >> 16;
    });
    fifo_read_hi_w =
        MMIO::ComplexWrite<u16>([WMASK_HI_RESTRICT](Core::System& system_, u32, u16 val) {
          auto& fifo_ = system_.GetCommandProcessor().GetFifo();
          system_.GetFifo().SyncGPUForRegisterAccess(system_);
          WriteHigh(fifo_.CPReadPointer, val & WMASK_HI_RESTRICT);
        });
  }
  mmio->Register(base | FIFO_READ_POINTER_HI, fifo_read_hi_r, fifo_read_hi_w);
}

void CommandProcessorManager::GatherPipeBursted(Core::System& system)
{
  auto& fifo = m_fifo;

  SetCPStatusFromCPU(system);

  auto& processor_interface = system.GetProcessorInterface();

  // if we aren't linked, we don't care about gather pipe data
  if (!m_cp_ctrl_reg.GPLinkEnable)
  {
    if (IsOnThread(system) && !system.GetFifo().UseDeterministicGPUThread())
    {
      // In multibuffer mode is not allowed write in the same FIFO attached to the GPU.
      // Fix Pokemon XD in DC mode.
      if ((processor_interface.m_fifo_cpu_end == fifo.CPEnd.load(std::memory_order_relaxed)) &&
          (processor_interface.m_fifo_cpu_base == fifo.CPBase.load(std::memory_order_relaxed)) &&
          fifo.CPReadWriteDistance.load(std::memory_order_relaxed) > 0)
      {
        system.GetFifo().FlushGpu(system);
      }
    }
    system.GetFifo().RunGpu(system);
    return;
  }

  // update the fifo pointer
  if (fifo.CPWritePointer.load(std::memory_order_relaxed) ==
      fifo.CPEnd.load(std::memory_order_relaxed))
  {
    fifo.CPWritePointer.store(fifo.CPBase, std::memory_order_relaxed);
  }
  else
  {
    fifo.CPWritePointer.fetch_add(GPFifo::GATHER_PIPE_SIZE, std::memory_order_relaxed);
  }

  if (m_cp_ctrl_reg.GPReadEnable && m_cp_ctrl_reg.GPLinkEnable)
  {
    processor_interface.m_fifo_cpu_write_pointer =
        fifo.CPWritePointer.load(std::memory_order_relaxed);
    processor_interface.m_fifo_cpu_base = fifo.CPBase.load(std::memory_order_relaxed);
    processor_interface.m_fifo_cpu_end = fifo.CPEnd.load(std::memory_order_relaxed);
  }

  // If the game is running close to overflowing, make the exception checking more frequent.
  if (fifo.bFF_HiWatermark.load(std::memory_order_relaxed) != 0)
    system.GetCoreTiming().ForceExceptionCheck(0);

  fifo.CPReadWriteDistance.fetch_add(GPFifo::GATHER_PIPE_SIZE, std::memory_order_seq_cst);

  system.GetFifo().RunGpu(system);

  ASSERT_MSG(COMMANDPROCESSOR,
             fifo.CPReadWriteDistance.load(std::memory_order_relaxed) <=
                 fifo.CPEnd.load(std::memory_order_relaxed) -
                     fifo.CPBase.load(std::memory_order_relaxed),
             "FIFO is overflowed by GatherPipe !\nCPU thread is too fast!");

  // check if we are in sync
  ASSERT_MSG(COMMANDPROCESSOR,
             fifo.CPWritePointer.load(std::memory_order_relaxed) ==
                 processor_interface.m_fifo_cpu_write_pointer,
             "FIFOs linked but out of sync");
  ASSERT_MSG(COMMANDPROCESSOR,
             fifo.CPBase.load(std::memory_order_relaxed) == processor_interface.m_fifo_cpu_base,
             "FIFOs linked but out of sync");
  ASSERT_MSG(COMMANDPROCESSOR,
             fifo.CPEnd.load(std::memory_order_relaxed) == processor_interface.m_fifo_cpu_end,
             "FIFOs linked but out of sync");
}

void CommandProcessorManager::UpdateInterrupts(Core::System& system, u64 userdata)
{
  if (userdata)
  {
    m_interrupt_set.Set();
    DEBUG_LOG_FMT(COMMANDPROCESSOR, "Interrupt set");
    system.GetProcessorInterface().SetInterrupt(INT_CAUSE_CP, true);
  }
  else
  {
    m_interrupt_set.Clear();
    DEBUG_LOG_FMT(COMMANDPROCESSOR, "Interrupt cleared");
    system.GetProcessorInterface().SetInterrupt(INT_CAUSE_CP, false);
  }
  system.GetCoreTiming().ForceExceptionCheck(0);
  m_interrupt_waiting.Clear();
  system.GetFifo().RunGpu(system);
}

void CommandProcessorManager::UpdateInterruptsFromVideoBackend(Core::System& system, u64 userdata)
{
  if (!system.GetFifo().UseDeterministicGPUThread())
  {
    system.GetCoreTiming().ScheduleEvent(0, m_event_type_update_interrupts, userdata,
                                         CoreTiming::FromThread::NON_CPU);
  }
}

bool CommandProcessorManager::IsInterruptWaiting() const
{
  return m_interrupt_waiting.IsSet();
}

void CommandProcessorManager::SetCPStatusFromGPU(Core::System& system)
{
  auto& fifo = m_fifo;

  // breakpoint
  const bool breakpoint = fifo.bFF_Breakpoint.load(std::memory_order_relaxed);
  if (fifo.bFF_BPEnable.load(std::memory_order_relaxed) != 0)
  {
    if (fifo.CPBreakpoint.load(std::memory_order_relaxed) ==
        fifo.CPReadPointer.load(std::memory_order_relaxed))
    {
      if (!breakpoint)
      {
        DEBUG_LOG_FMT(COMMANDPROCESSOR, "Hit breakpoint at {}",
                      fifo.CPReadPointer.load(std::memory_order_relaxed));
        fifo.bFF_Breakpoint.store(1, std::memory_order_relaxed);
      }
    }
    else
    {
      if (breakpoint)
      {
        DEBUG_LOG_FMT(COMMANDPROCESSOR, "Cleared breakpoint at {}",
                      fifo.CPReadPointer.load(std::memory_order_relaxed));
        fifo.bFF_Breakpoint.store(0, std::memory_order_relaxed);
      }
    }
  }
  else
  {
    if (breakpoint)
    {
      DEBUG_LOG_FMT(COMMANDPROCESSOR, "Cleared breakpoint at {}",
                    fifo.CPReadPointer.load(std::memory_order_relaxed));
      fifo.bFF_Breakpoint = false;
    }
  }

  // overflow & underflow check
  fifo.bFF_HiWatermark.store(
      (fifo.CPReadWriteDistance.load(std::memory_order_relaxed) > fifo.CPHiWatermark),
      std::memory_order_relaxed);
  fifo.bFF_LoWatermark.store(
      (fifo.CPReadWriteDistance.load(std::memory_order_relaxed) < fifo.CPLoWatermark),
      std::memory_order_relaxed);

  bool bpInt = fifo.bFF_Breakpoint.load(std::memory_order_relaxed) &&
               fifo.bFF_BPInt.load(std::memory_order_relaxed);
  bool ovfInt = fifo.bFF_HiWatermark.load(std::memory_order_relaxed) &&
                fifo.bFF_HiWatermarkInt.load(std::memory_order_relaxed);
  bool undfInt = fifo.bFF_LoWatermark.load(std::memory_order_relaxed) &&
                 fifo.bFF_LoWatermarkInt.load(std::memory_order_relaxed);

  bool interrupt = (bpInt || ovfInt || undfInt) && m_cp_ctrl_reg.GPReadEnable;

  if (interrupt != m_interrupt_set.IsSet() && !m_interrupt_waiting.IsSet())
  {
    u64 userdata = interrupt ? 1 : 0;
    if (IsOnThread(system))
    {
      if (!interrupt || bpInt || undfInt || ovfInt)
      {
        // Schedule the interrupt asynchronously
        m_interrupt_waiting.Set();
        UpdateInterruptsFromVideoBackend(system, userdata);
      }
    }
    else
    {
      UpdateInterrupts(system, userdata);
    }
  }
}

void CommandProcessorManager::SetCPStatusFromCPU(Core::System& system)
{
  auto& fifo = m_fifo;

  // overflow & underflow check
  fifo.bFF_HiWatermark.store(
      (fifo.CPReadWriteDistance.load(std::memory_order_relaxed) > fifo.CPHiWatermark),
      std::memory_order_relaxed);
  fifo.bFF_LoWatermark.store(
      (fifo.CPReadWriteDistance.load(std::memory_order_relaxed) < fifo.CPLoWatermark),
      std::memory_order_relaxed);

  bool bpInt = fifo.bFF_Breakpoint.load(std::memory_order_relaxed) &&
               fifo.bFF_BPInt.load(std::memory_order_relaxed);
  bool ovfInt = fifo.bFF_HiWatermark.load(std::memory_order_relaxed) &&
                fifo.bFF_HiWatermarkInt.load(std::memory_order_relaxed);
  bool undfInt = fifo.bFF_LoWatermark.load(std::memory_order_relaxed) &&
                 fifo.bFF_LoWatermarkInt.load(std::memory_order_relaxed);

  bool interrupt = (bpInt || ovfInt || undfInt) && m_cp_ctrl_reg.GPReadEnable;

  if (interrupt != m_interrupt_set.IsSet() && !m_interrupt_waiting.IsSet())
  {
    u64 userdata = interrupt ? 1 : 0;
    if (IsOnThread(system))
    {
      if (!interrupt || bpInt || undfInt || ovfInt)
      {
        m_interrupt_set.Set(interrupt);
        DEBUG_LOG_FMT(COMMANDPROCESSOR, "Interrupt set");
        system.GetProcessorInterface().SetInterrupt(INT_CAUSE_CP, interrupt);
      }
    }
    else
    {
      UpdateInterrupts(system, userdata);
    }
  }
}

void CommandProcessorManager::SetCpStatusRegister(Core::System& system)
{
  const auto& fifo = m_fifo;

  // Here always there is one fifo attached to the GPU
  m_cp_status_reg.Breakpoint = fifo.bFF_Breakpoint.load(std::memory_order_relaxed);
  m_cp_status_reg.ReadIdle = !fifo.CPReadWriteDistance.load(std::memory_order_relaxed) ||
                             (fifo.CPReadPointer.load(std::memory_order_relaxed) ==
                              fifo.CPWritePointer.load(std::memory_order_relaxed));
  m_cp_status_reg.CommandIdle = !fifo.CPReadWriteDistance.load(std::memory_order_relaxed) ||
                                Fifo::AtBreakpoint(system) ||
                                !fifo.bFF_GPReadEnable.load(std::memory_order_relaxed);
  m_cp_status_reg.UnderflowLoWatermark = fifo.bFF_LoWatermark.load(std::memory_order_relaxed);
  m_cp_status_reg.OverflowHiWatermark = fifo.bFF_HiWatermark.load(std::memory_order_relaxed);

  DEBUG_LOG_FMT(COMMANDPROCESSOR, "\t Read from STATUS_REGISTER : {:04x}", m_cp_status_reg.Hex);
  DEBUG_LOG_FMT(COMMANDPROCESSOR,
                "(r) status: iBP {} | fReadIdle {} | fCmdIdle {} | iOvF {} | iUndF {}",
                m_cp_status_reg.Breakpoint ? "ON" : "OFF", m_cp_status_reg.ReadIdle ? "ON" : "OFF",
                m_cp_status_reg.CommandIdle ? "ON" : "OFF",
                m_cp_status_reg.OverflowHiWatermark ? "ON" : "OFF",
                m_cp_status_reg.UnderflowLoWatermark ? "ON" : "OFF");
}

void CommandProcessorManager::SetCpControlRegister(Core::System& system)
{
  auto& fifo = m_fifo;

  fifo.bFF_BPInt.store(m_cp_ctrl_reg.BPInt, std::memory_order_relaxed);
  fifo.bFF_BPEnable.store(m_cp_ctrl_reg.BPEnable, std::memory_order_relaxed);
  fifo.bFF_HiWatermarkInt.store(m_cp_ctrl_reg.FifoOverflowIntEnable, std::memory_order_relaxed);
  fifo.bFF_LoWatermarkInt.store(m_cp_ctrl_reg.FifoUnderflowIntEnable, std::memory_order_relaxed);
  fifo.bFF_GPLinkEnable.store(m_cp_ctrl_reg.GPLinkEnable, std::memory_order_relaxed);

  if (fifo.bFF_GPReadEnable.load(std::memory_order_relaxed) && !m_cp_ctrl_reg.GPReadEnable)
  {
    fifo.bFF_GPReadEnable.store(m_cp_ctrl_reg.GPReadEnable, std::memory_order_relaxed);
    system.GetFifo().FlushGpu(system);
  }
  else
  {
    fifo.bFF_GPReadEnable = m_cp_ctrl_reg.GPReadEnable;
  }

  DEBUG_LOG_FMT(COMMANDPROCESSOR, "\t GPREAD {} | BP {} | Int {} | OvF {} | UndF {} | LINK {}",
                fifo.bFF_GPReadEnable.load(std::memory_order_relaxed) ? "ON" : "OFF",
                fifo.bFF_BPEnable.load(std::memory_order_relaxed) ? "ON" : "OFF",
                fifo.bFF_BPInt.load(std::memory_order_relaxed) ? "ON" : "OFF",
                m_cp_ctrl_reg.FifoOverflowIntEnable ? "ON" : "OFF",
                m_cp_ctrl_reg.FifoUnderflowIntEnable ? "ON" : "OFF",
                m_cp_ctrl_reg.GPLinkEnable ? "ON" : "OFF");
}

// NOTE: We intentionally don't emulate this function at the moment.
// We don't emulate proper GP timing anyway at the moment, so it would just slow down emulation.
void CommandProcessorManager::SetCpClearRegister()
{
}

void CommandProcessorManager::HandleUnknownOpcode(Core::System& system, u8 cmd_byte,
                                                  const u8* buffer, bool preprocess)
{
  const auto& fifo = m_fifo;

  // Datel software uses 0x01 during startup, and Mario Party 5's Wiggler capsule accidentally uses
  // 0x01-0x03 due to sending 4 more vertices than intended (see https://dolp.in/i8104).
  // Prince of Persia: Rival Swords sends 0x3f if the home menu is opened during the intro cutscene
  // due to a game bug resulting in an incorrect vertex desc that results in the float value 1.0,
  // encoded as 0x3f800000, being parsed as an opcode (see https://dolp.in/i9203).
  //
  // Hardware testing indicates that these opcodes do nothing, so to avoid annoying the user with
  // spurious popups, we don't create a panic alert in those cases.  Other unknown opcodes
  // (such as 0x18) seem to result in actual hangs on real hardware, so the alert still is important
  // to keep around for unexpected cases.
  const bool suppress_panic_alert = (cmd_byte <= 0x7) || (cmd_byte == 0x3f);

  const auto log_level =
      suppress_panic_alert ? Common::Log::LogLevel::LWARNING : Common::Log::LogLevel::LERROR;

  // We always generate this log message, though we only generate the panic alerts once.
  //
  // PC and LR are generally inaccurate in dual-core and are still misleading in single-core
  // due to the gather pipe queueing data.  Changing GATHER_PIPE_SIZE to 1 and
  // GATHER_PIPE_EXTRA_SIZE to 16 * 32 in GPFifo.h, and using the cached interpreter CPU emulation
  // engine, can result in more accurate information (though it is still a bit delayed).
  // PC and LR are meaningless when using the fifoplayer, and will generally not be helpful if the
  // unknown opcode is inside of a display list.  Also note that the changes in GPFifo.h are not
  // accurate and may introduce timing issues.
  const auto& ppc_state = system.GetPPCState();
  GENERIC_LOG_FMT(
      Common::Log::LogType::VIDEO, log_level,
      "FIFO: Unknown Opcode {:#04x} @ {}, preprocessing = {}, CPBase: {:#010x}, CPEnd: "
      "{:#010x}, CPHiWatermark: {:#010x}, CPLoWatermark: {:#010x}, CPReadWriteDistance: "
      "{:#010x}, CPWritePointer: {:#010x}, CPReadPointer: {:#010x}, CPBreakpoint: "
      "{:#010x}, bFF_GPReadEnable: {}, bFF_BPEnable: {}, bFF_BPInt: {}, bFF_Breakpoint: "
      "{}, bFF_GPLinkEnable: {}, bFF_HiWatermarkInt: {}, bFF_LoWatermarkInt: {}, "
      "approximate PC: {:08x}, approximate LR: {:08x}",
      cmd_byte, fmt::ptr(buffer), preprocess ? "yes" : "no",
      fifo.CPBase.load(std::memory_order_relaxed), fifo.CPEnd.load(std::memory_order_relaxed),
      fifo.CPHiWatermark, fifo.CPLoWatermark,
      fifo.CPReadWriteDistance.load(std::memory_order_relaxed),
      fifo.CPWritePointer.load(std::memory_order_relaxed),
      fifo.CPReadPointer.load(std::memory_order_relaxed),
      fifo.CPBreakpoint.load(std::memory_order_relaxed),
      fifo.bFF_GPReadEnable.load(std::memory_order_relaxed) ? "true" : "false",
      fifo.bFF_BPEnable.load(std::memory_order_relaxed) ? "true" : "false",
      fifo.bFF_BPInt.load(std::memory_order_relaxed) ? "true" : "false",
      fifo.bFF_Breakpoint.load(std::memory_order_relaxed) ? "true" : "false",
      fifo.bFF_GPLinkEnable.load(std::memory_order_relaxed) ? "true" : "false",
      fifo.bFF_HiWatermarkInt.load(std::memory_order_relaxed) ? "true" : "false",
      fifo.bFF_LoWatermarkInt.load(std::memory_order_relaxed) ? "true" : "false", ppc_state.pc,
      LR(ppc_state));

  if (!m_is_fifo_error_seen && !suppress_panic_alert)
  {
    m_is_fifo_error_seen = true;

    // TODO(Omega): Maybe dump FIFO to file on this error
    PanicAlertFmtT("GFX FIFO: Unknown Opcode ({0:#04x} @ {1}, preprocess={2}).\n"
                   "This means one of the following:\n"
                   "* The emulated GPU got desynced, disabling dual core can help\n"
                   "* Command stream corrupted by some spurious memory bug\n"
                   "* This really is an unknown opcode (unlikely)\n"
                   "* Some other sort of bug\n\n"
                   "Further errors will be sent to the Video Backend log and\n"
                   "Dolphin will now likely crash or hang.",
                   cmd_byte, fmt::ptr(buffer), preprocess);
  }
}

}  // namespace CommandProcessor
