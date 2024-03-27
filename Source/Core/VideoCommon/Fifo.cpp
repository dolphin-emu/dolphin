// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Fifo.h"

#include <atomic>
#include <cstring>

#include "Common/Assert.h"
#include "Common/BlockingLoop.h"
#include "Common/ChunkFile.h"
#include "Common/Event.h"
#include "Common/FPURoundMode.h"
#include "Common/MemoryUtil.h"
#include "Common/MsgHandler.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/Memmap.h"
#include "Core/Host.h"
#include "Core/System.h"

#include "AbstractGfx.h"
#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoBackendBase.h"

namespace Fifo
{
static constexpr int GPU_TIME_SLOT_SIZE = 1000;

FifoManager::FifoManager(Core::System& system) : m_system{system}
{
}

FifoManager::~FifoManager() = default;

void FifoManager::RefreshConfig()
{
  m_config_sync_gpu = Config::Get(Config::MAIN_SYNC_GPU);
  m_config_sync_gpu_max_distance = Config::Get(Config::MAIN_SYNC_GPU_MAX_DISTANCE);
  m_config_sync_gpu_min_distance = Config::Get(Config::MAIN_SYNC_GPU_MIN_DISTANCE);
  m_config_sync_gpu_overclock = Config::Get(Config::MAIN_SYNC_GPU_OVERCLOCK);
}

void FifoManager::DoState(PointerWrap& p)
{
  p.DoArray(m_video_buffer, FIFO_SIZE);
  u8* write_ptr = m_video_buffer_write_ptr;
  p.DoPointer(write_ptr, m_video_buffer);
  m_video_buffer_write_ptr = write_ptr;
  p.DoPointer(m_video_buffer_read_ptr, m_video_buffer);
  if (p.IsReadMode() && m_use_deterministic_gpu_thread)
  {
    // We're good and paused, right?
    m_video_buffer_seen_ptr = m_video_buffer_pp_read_ptr = m_video_buffer_read_ptr;
  }

  p.Do(m_sync_ticks);
  p.Do(m_syncing_suspended);
}

void FifoManager::PauseAndLock(bool do_lock, bool unpause_on_unlock)
{
  if (do_lock)
  {
    SyncGPU(SyncGPUReason::Other);
    EmulatorState(false);

    if (!m_system.IsDualCoreMode() || m_use_deterministic_gpu_thread)
      return;

    m_gpu_mainloop.WaitYield(std::chrono::milliseconds(100), Host_YieldToUI);
  }
  else
  {
    if (unpause_on_unlock)
      EmulatorState(true);
  }
}

void FifoManager::Init()
{
  if (!m_config_callback_id)
    m_config_callback_id = Config::AddConfigChangedCallback([this] { RefreshConfig(); });
  RefreshConfig();

  // Padded so that SIMD overreads in the vertex loader are safe
  m_video_buffer = static_cast<u8*>(Common::AllocateMemoryPages(FIFO_SIZE + 4));
  ResetVideoBuffer();
  if (m_system.IsDualCoreMode())
    m_gpu_mainloop.Prepare();
  m_sync_ticks.store(0);
}

void FifoManager::Shutdown()
{
  if (m_gpu_mainloop.IsRunning())
    PanicAlertFmt("FIFO shutting down while active");

  Common::FreeMemoryPages(m_video_buffer, FIFO_SIZE + 4);
  m_video_buffer = nullptr;
  m_video_buffer_write_ptr = nullptr;
  m_video_buffer_pp_read_ptr = nullptr;
  m_video_buffer_read_ptr = nullptr;
  m_video_buffer_seen_ptr = nullptr;
  m_fifo_aux_write_ptr = nullptr;
  m_fifo_aux_read_ptr = nullptr;

  if (m_config_callback_id)
  {
    Config::RemoveConfigChangedCallback(*m_config_callback_id);
    m_config_callback_id = std::nullopt;
  }
}

// May be executed from any thread, even the graphics thread.
// Created to allow for self shutdown.
void FifoManager::ExitGpuLoop()
{
  auto& command_processor = m_system.GetCommandProcessor();
  auto& fifo = command_processor.GetFifo();

  // This should break the wait loop in CPU thread
  fifo.bFF_GPReadEnable.store(0, std::memory_order_relaxed);
  FlushGpu();

  // Terminate GPU thread loop
  m_emu_running_state.Set();
  m_gpu_mainloop.Stop(Common::BlockingLoop::StopMode::NonBlock);
}

void FifoManager::EmulatorState(bool running)
{
  m_emu_running_state.Set(running);
  if (running)
    m_gpu_mainloop.Wakeup();
  else
    m_gpu_mainloop.AllowSleep();
}

void FifoManager::SyncGPU(SyncGPUReason reason, bool may_move_read_ptr)
{
  if (m_use_deterministic_gpu_thread)
  {
    m_gpu_mainloop.Wait();
    if (!m_gpu_mainloop.IsRunning())
      return;

    // Opportunistically reset FIFOs so we don't wrap around.
    if (may_move_read_ptr && m_fifo_aux_write_ptr != m_fifo_aux_read_ptr)
    {
      PanicAlertFmt("Aux FIFO not synced ({}, {})", fmt::ptr(m_fifo_aux_write_ptr),
                    fmt::ptr(m_fifo_aux_read_ptr));
    }

    memmove(m_fifo_aux_data, m_fifo_aux_read_ptr, m_fifo_aux_write_ptr - m_fifo_aux_read_ptr);
    m_fifo_aux_write_ptr -= (m_fifo_aux_read_ptr - m_fifo_aux_data);
    m_fifo_aux_read_ptr = m_fifo_aux_data;

    if (may_move_read_ptr)
    {
      u8* write_ptr = m_video_buffer_write_ptr;

      // what's left over in the buffer
      size_t size = write_ptr - m_video_buffer_pp_read_ptr;

      memmove(m_video_buffer, m_video_buffer_pp_read_ptr, size);
      // This change always decreases the pointers.  We write seen_ptr
      // after write_ptr here, and read it before in RunGpuLoop, so
      // 'write_ptr > seen_ptr' there cannot become spuriously true.
      m_video_buffer_write_ptr = write_ptr = m_video_buffer + size;
      m_video_buffer_pp_read_ptr = m_video_buffer;
      m_video_buffer_read_ptr = m_video_buffer;
      m_video_buffer_seen_ptr = write_ptr;
    }
  }
}

void FifoManager::PushFifoAuxBuffer(const void* ptr, size_t size)
{
  if (size > (size_t)(m_fifo_aux_data + FIFO_SIZE - m_fifo_aux_write_ptr))
  {
    SyncGPU(SyncGPUReason::AuxSpace, /* may_move_read_ptr */ false);
    if (!m_gpu_mainloop.IsRunning())
    {
      // GPU is shutting down
      return;
    }
    if (size > (size_t)(m_fifo_aux_data + FIFO_SIZE - m_fifo_aux_write_ptr))
    {
      // That will sync us up to the last 32 bytes, so this short region
      // of FIFO would have to point to a 2MB display list or something.
      PanicAlertFmt("Absurdly large aux buffer");
      return;
    }
  }
  memcpy(m_fifo_aux_write_ptr, ptr, size);
  m_fifo_aux_write_ptr += size;
}

void* FifoManager::PopFifoAuxBuffer(size_t size)
{
  void* ret = m_fifo_aux_read_ptr;
  m_fifo_aux_read_ptr += size;
  return ret;
}

// Description: RunGpuLoop() sends data through this function.
void FifoManager::ReadDataFromFifo(u32 read_ptr)
{
  if (GPFifo::GATHER_PIPE_SIZE >
      static_cast<size_t>(m_video_buffer + FIFO_SIZE - m_video_buffer_write_ptr))
  {
    const size_t existing_len = m_video_buffer_write_ptr - m_video_buffer_read_ptr;
    if (GPFifo::GATHER_PIPE_SIZE > static_cast<size_t>(FIFO_SIZE - existing_len))
    {
      PanicAlertFmt("FIFO out of bounds (existing {} + new {} > {})", existing_len,
                    GPFifo::GATHER_PIPE_SIZE, FIFO_SIZE);
      return;
    }
    memmove(m_video_buffer, m_video_buffer_read_ptr, existing_len);
    m_video_buffer_write_ptr = m_video_buffer + existing_len;
    m_video_buffer_read_ptr = m_video_buffer;
  }
  // Copy new video instructions to m_video_buffer for future use in rendering the new picture
  auto& memory = m_system.GetMemory();
  memory.CopyFromEmu(m_video_buffer_write_ptr, read_ptr, GPFifo::GATHER_PIPE_SIZE);
  m_video_buffer_write_ptr += GPFifo::GATHER_PIPE_SIZE;
}

// The deterministic_gpu_thread version.
void FifoManager::ReadDataFromFifoOnCPU(u32 read_ptr)
{
  u8* write_ptr = m_video_buffer_write_ptr;
  if (GPFifo::GATHER_PIPE_SIZE > static_cast<size_t>(m_video_buffer + FIFO_SIZE - write_ptr))
  {
    // We can't wrap around while the GPU is working on the data.
    // This should be very rare due to the reset in SyncGPU.
    SyncGPU(SyncGPUReason::Wraparound);
    if (!m_gpu_mainloop.IsRunning())
    {
      // GPU is shutting down, so the next asserts may fail
      return;
    }

    if (m_video_buffer_pp_read_ptr != m_video_buffer_read_ptr)
    {
      PanicAlertFmt("Desynced read pointers");
      return;
    }
    write_ptr = m_video_buffer_write_ptr;
    const size_t existing_len = write_ptr - m_video_buffer_pp_read_ptr;
    if (GPFifo::GATHER_PIPE_SIZE > static_cast<size_t>(FIFO_SIZE - existing_len))
    {
      PanicAlertFmt("FIFO out of bounds (existing {} + new {} > {})", existing_len,
                    GPFifo::GATHER_PIPE_SIZE, FIFO_SIZE);
      return;
    }
  }
  auto& memory = m_system.GetMemory();
  memory.CopyFromEmu(m_video_buffer_write_ptr, read_ptr, GPFifo::GATHER_PIPE_SIZE);
  m_video_buffer_pp_read_ptr = OpcodeDecoder::RunFifo<true>(
      DataReader(m_video_buffer_pp_read_ptr, write_ptr + GPFifo::GATHER_PIPE_SIZE), nullptr);
  // This would have to be locked if the GPU thread didn't spin.
  m_video_buffer_write_ptr = write_ptr + GPFifo::GATHER_PIPE_SIZE;
}

void FifoManager::ResetVideoBuffer()
{
  m_video_buffer_read_ptr = m_video_buffer;
  m_video_buffer_write_ptr = m_video_buffer;
  m_video_buffer_seen_ptr = m_video_buffer;
  m_video_buffer_pp_read_ptr = m_video_buffer;
  m_fifo_aux_write_ptr = m_fifo_aux_data;
  m_fifo_aux_read_ptr = m_fifo_aux_data;
}

// Description: Main FIFO update loop
// Purpose: Keep the Core HW updated about the CPU-GPU distance
void FifoManager::RunGpuLoop()
{
  AsyncRequests::GetInstance()->SetEnable(true);
  AsyncRequests::GetInstance()->SetPassthrough(false);

  m_gpu_mainloop.Run(
      [this] {
        // Run events from the CPU thread.
        AsyncRequests::GetInstance()->PullEvents();

        // Do nothing while paused
        if (!m_emu_running_state.IsSet())
          return;

        if (m_use_deterministic_gpu_thread)
        {
          // All the fifo/CP stuff is on the CPU.  We just need to run the opcode decoder.
          u8* seen_ptr = m_video_buffer_seen_ptr;
          u8* write_ptr = m_video_buffer_write_ptr;
          // See comment in SyncGPU
          if (write_ptr > seen_ptr)
          {
            m_video_buffer_read_ptr =
                OpcodeDecoder::RunFifo(DataReader(m_video_buffer_read_ptr, write_ptr), nullptr);
            m_video_buffer_seen_ptr = write_ptr;
          }
        }
        else
        {
          auto& command_processor = m_system.GetCommandProcessor();
          auto& fifo = command_processor.GetFifo();
          command_processor.SetCPStatusFromGPU();

          // check if we are able to run this buffer
          while (!command_processor.IsInterruptWaiting() &&
                 fifo.bFF_GPReadEnable.load(std::memory_order_relaxed) &&
                 fifo.CPReadWriteDistance.load(std::memory_order_relaxed) &&
                 !AtBreakpoint(m_system))
          {
            if (m_config_sync_gpu && m_sync_ticks.load() < m_config_sync_gpu_min_distance)
              break;

            u32 cyclesExecuted = 0;
            u32 readPtr = fifo.CPReadPointer.load(std::memory_order_relaxed);
            ReadDataFromFifo(readPtr);

            if (readPtr == fifo.CPEnd.load(std::memory_order_relaxed))
              readPtr = fifo.CPBase.load(std::memory_order_relaxed);
            else
              readPtr += GPFifo::GATHER_PIPE_SIZE;

            const s32 distance =
                static_cast<s32>(fifo.CPReadWriteDistance.load(std::memory_order_relaxed)) -
                GPFifo::GATHER_PIPE_SIZE;
            ASSERT_MSG(COMMANDPROCESSOR, distance >= 0,
                       "Negative fifo.CPReadWriteDistance = {} in FIFO Loop !\nThat can produce "
                       "instability in the game. Please report it.",
                       distance);

            u8* write_ptr = m_video_buffer_write_ptr;
            m_video_buffer_read_ptr = OpcodeDecoder::RunFifo(
                DataReader(m_video_buffer_read_ptr, write_ptr), &cyclesExecuted);

            fifo.CPReadPointer.store(readPtr, std::memory_order_relaxed);
            fifo.CPReadWriteDistance.fetch_sub(GPFifo::GATHER_PIPE_SIZE, std::memory_order_seq_cst);
            if ((write_ptr - m_video_buffer_read_ptr) == 0)
            {
              fifo.SafeCPReadPointer.store(fifo.CPReadPointer.load(std::memory_order_relaxed),
                                           std::memory_order_relaxed);
            }

            command_processor.SetCPStatusFromGPU();

            if (m_config_sync_gpu)
            {
              cyclesExecuted = (int)(cyclesExecuted / m_config_sync_gpu_overclock);
              int old = m_sync_ticks.fetch_sub(cyclesExecuted);
              if (old >= m_config_sync_gpu_max_distance &&
                  old - (int)cyclesExecuted < m_config_sync_gpu_max_distance)
              {
                m_sync_wakeup_event.Set();
              }
            }

            // This call is pretty important in DualCore mode and must be called in the FIFO Loop.
            // If we don't, s_swapRequested or s_efbAccessRequested won't be set to false
            // leading the CPU thread to wait in Video_OutputXFB or Video_AccessEFB thus slowing
            // things down.
            AsyncRequests::GetInstance()->PullEvents();
          }

          // fast skip remaining GPU time if fifo is empty
          if (m_sync_ticks.load() > 0)
          {
            int old = m_sync_ticks.exchange(0);
            if (old >= m_config_sync_gpu_max_distance)
              m_sync_wakeup_event.Set();
          }

          // The fifo is empty and it's unlikely we will get any more work in the near future.
          // Make sure VertexManager finishes drawing any primitives it has stored in it's buffer.
          g_vertex_manager->Flush();
          g_framebuffer_manager->RefreshPeekCache();

          // Flush to worker thread on multithreaded backends (Vulkan)
          g_gfx->Flush(FlushType::FlushToWorker);
        }
      },
      100);

  AsyncRequests::GetInstance()->SetEnable(false);
  AsyncRequests::GetInstance()->SetPassthrough(true);
}

void FifoManager::FlushGpu()
{
  if (!m_system.IsDualCoreMode() || m_use_deterministic_gpu_thread)
    return;

  m_gpu_mainloop.Wait();
}

void FifoManager::GpuMaySleep()
{
  m_gpu_mainloop.AllowSleep();
}

bool AtBreakpoint(Core::System& system)
{
  auto& command_processor = system.GetCommandProcessor();
  const auto& fifo = command_processor.GetFifo();
  return fifo.bFF_BPEnable.load(std::memory_order_relaxed) &&
         (fifo.CPReadPointer.load(std::memory_order_relaxed) ==
          fifo.CPBreakpoint.load(std::memory_order_relaxed));
}

void FifoManager::RunGpu()
{
  const bool is_dual_core = m_system.IsDualCoreMode();

  // wake up GPU thread
  if (is_dual_core && !m_use_deterministic_gpu_thread)
  {
    m_gpu_mainloop.Wakeup();
  }

  // if the sync GPU callback is suspended, wake it up.
  if (!is_dual_core || m_use_deterministic_gpu_thread || m_config_sync_gpu)
  {
    if (m_syncing_suspended)
    {
      m_syncing_suspended = false;
      m_system.GetCoreTiming().ScheduleEvent(GPU_TIME_SLOT_SIZE, m_event_sync_gpu,
                                             GPU_TIME_SLOT_SIZE);
    }
  }
}

int FifoManager::RunGpuOnCpu(int ticks)
{
  auto& command_processor = m_system.GetCommandProcessor();
  auto& fifo = command_processor.GetFifo();
  bool reset_simd_state = false;
  int available_ticks = int(ticks * m_config_sync_gpu_overclock) + m_sync_ticks.load();
  while (fifo.bFF_GPReadEnable.load(std::memory_order_relaxed) &&
         fifo.CPReadWriteDistance.load(std::memory_order_relaxed) && !AtBreakpoint(m_system) &&
         available_ticks >= 0)
  {
    if (m_use_deterministic_gpu_thread)
    {
      ReadDataFromFifoOnCPU(fifo.CPReadPointer.load(std::memory_order_relaxed));
      m_gpu_mainloop.Wakeup();
    }
    else
    {
      if (!reset_simd_state)
      {
        Common::FPU::SaveSIMDState();
        Common::FPU::LoadDefaultSIMDState();
        reset_simd_state = true;
      }
      ReadDataFromFifo(fifo.CPReadPointer.load(std::memory_order_relaxed));
      u32 cycles = 0;
      m_video_buffer_read_ptr = OpcodeDecoder::RunFifo(
          DataReader(m_video_buffer_read_ptr, m_video_buffer_write_ptr), &cycles);
      available_ticks -= cycles;
    }

    if (fifo.CPReadPointer.load(std::memory_order_relaxed) ==
        fifo.CPEnd.load(std::memory_order_relaxed))
    {
      fifo.CPReadPointer.store(fifo.CPBase.load(std::memory_order_relaxed),
                               std::memory_order_relaxed);
    }
    else
    {
      fifo.CPReadPointer.fetch_add(GPFifo::GATHER_PIPE_SIZE, std::memory_order_relaxed);
    }

    fifo.CPReadWriteDistance.fetch_sub(GPFifo::GATHER_PIPE_SIZE, std::memory_order_relaxed);
  }

  command_processor.SetCPStatusFromGPU();

  if (reset_simd_state)
  {
    Common::FPU::LoadSIMDState();
  }

  // Discard all available ticks as there is nothing to do any more.
  m_sync_ticks.store(std::min(available_ticks, 0));

  g_gfx->Flush(FlushType::FlushToWorker);

  // If the GPU is idle, drop the handler.
  if (available_ticks >= 0)
    return -1;

  // Always wait at least for GPU_TIME_SLOT_SIZE cycles.
  return -available_ticks + GPU_TIME_SLOT_SIZE;
}

void FifoManager::UpdateWantDeterminism(bool want)
{
  // We are paused (or not running at all yet), so
  // it should be safe to change this.
  bool gpu_thread = false;
  switch (Config::GetGPUDeterminismMode())
  {
  case Config::GPUDeterminismMode::Auto:
    gpu_thread = want;
    break;
  case Config::GPUDeterminismMode::Disabled:
    gpu_thread = false;
    break;
  case Config::GPUDeterminismMode::FakeCompletion:
    gpu_thread = true;
    break;
  }

  gpu_thread = gpu_thread && m_system.IsDualCoreMode();

  if (m_use_deterministic_gpu_thread != gpu_thread)
  {
    m_use_deterministic_gpu_thread = gpu_thread;
    if (gpu_thread)
    {
      // These haven't been updated in non-deterministic mode.
      m_video_buffer_seen_ptr = m_video_buffer_pp_read_ptr = m_video_buffer_read_ptr;
      CopyPreprocessCPStateFromMain();
      VertexLoaderManager::MarkAllDirty();
    }
  }
}

/* This function checks the emulated CPU - GPU distance and may wake up the GPU,
 * or block the CPU if required. It should be called by the CPU thread regularly.
 * @ticks The gone emulated CPU time.
 * @return A good time to call WaitForGpuThread() next.
 */
int FifoManager::WaitForGpuThread(int ticks)
{
  int old = m_sync_ticks.fetch_add(ticks);
  int now = old + ticks;

  // GPU is idle, so stop polling.
  if (old >= 0 && m_gpu_mainloop.IsDone())
    return -1;

  // Wakeup GPU
  if (old < m_config_sync_gpu_min_distance && now >= m_config_sync_gpu_min_distance)
    RunGpu();

  // If the GPU is still sleeping, wait for a longer time
  if (now < m_config_sync_gpu_min_distance)
    return GPU_TIME_SLOT_SIZE + m_config_sync_gpu_min_distance - now;

  // Wait for GPU
  if (now >= m_config_sync_gpu_max_distance)
    m_sync_wakeup_event.Wait();

  return GPU_TIME_SLOT_SIZE;
}

void FifoManager::SyncGPUCallback(Core::System& system, u64 ticks, s64 cyclesLate)
{
  ticks += cyclesLate;
  int next = -1;

  auto& fifo = system.GetFifo();
  if (!system.IsDualCoreMode() || fifo.m_use_deterministic_gpu_thread)
  {
    next = fifo.RunGpuOnCpu(int(ticks));
  }
  else if (fifo.m_config_sync_gpu)
  {
    next = fifo.WaitForGpuThread(int(ticks));
  }

  fifo.m_syncing_suspended = next < 0;
  if (!fifo.m_syncing_suspended)
    system.GetCoreTiming().ScheduleEvent(next, fifo.m_event_sync_gpu, next);
}

void FifoManager::SyncGPUForRegisterAccess()
{
  SyncGPU(SyncGPUReason::Other);

  if (!m_system.IsDualCoreMode() || m_use_deterministic_gpu_thread)
    RunGpuOnCpu(GPU_TIME_SLOT_SIZE);
  else if (m_config_sync_gpu)
    WaitForGpuThread(GPU_TIME_SLOT_SIZE);
}

// Initialize GPU - CPU thread syncing, this gives us a deterministic way to start the GPU thread.
void FifoManager::Prepare()
{
  m_event_sync_gpu = m_system.GetCoreTiming().RegisterEvent("SyncGPUCallback", SyncGPUCallback);
  m_syncing_suspended = true;
}
}  // namespace Fifo
