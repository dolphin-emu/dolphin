// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/Fifo.h"

#include <atomic>
#include <cstring>

#include "Common/Assert.h"
#include "Common/Atomic.h"
#include "Common/BlockingLoop.h"
#include "Common/ChunkFile.h"
#include "Common/Event.h"
#include "Common/FPURoundMode.h"
#include "Common/MemoryUtil.h"
#include "Common/MsgHandler.h"

#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"
#include "Core/Host.h"

#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoBackendBase.h"

namespace Fifo
{
static constexpr u32 FIFO_SIZE = 2 * 1024 * 1024;
static constexpr int GPU_TIME_SLOT_SIZE = 1000;

static Common::BlockingLoop s_gpu_mainloop;

static Common::Flag s_emu_running_state;

// Most of this array is unlikely to be faulted in...
static u8 s_fifo_aux_data[FIFO_SIZE];
static u8* s_fifo_aux_write_ptr;
static u8* s_fifo_aux_read_ptr;

// This could be in SConfig, but it depends on multiple settings
// and can change at runtime.
static bool s_use_deterministic_gpu_thread;

static CoreTiming::EventType* s_event_sync_gpu;

// STATE_TO_SAVE
static u8* s_video_buffer;
static u8* s_video_buffer_read_ptr;
static std::atomic<u8*> s_video_buffer_write_ptr;
static std::atomic<u8*> s_video_buffer_seen_ptr;
static u8* s_video_buffer_pp_read_ptr;
// The read_ptr is always owned by the GPU thread.  In normal mode, so is the
// write_ptr, despite it being atomic.  In deterministic GPU thread mode,
// things get a bit more complicated:
// - The seen_ptr is written by the GPU thread, and points to what it's already
// processed as much of as possible - in the case of a partial command which
// caused it to stop, not the same as the read ptr.  It's written by the GPU,
// under the lock, and updating the cond.
// - The write_ptr is written by the CPU thread after it copies data from the
// FIFO.  Maybe someday it will be under the lock.  For now, because RunGpuLoop
// polls, it's just atomic.
// - The pp_read_ptr is the CPU preprocessing version of the read_ptr.

static std::atomic<int> s_sync_ticks;
static bool s_syncing_suspended;
static Common::Event s_sync_wakeup_event;

void DoState(PointerWrap& p)
{
  p.DoArray(s_video_buffer, FIFO_SIZE);
  u8* write_ptr = s_video_buffer_write_ptr;
  p.DoPointer(write_ptr, s_video_buffer);
  s_video_buffer_write_ptr = write_ptr;
  p.DoPointer(s_video_buffer_read_ptr, s_video_buffer);
  if (p.mode == PointerWrap::MODE_READ && s_use_deterministic_gpu_thread)
  {
    // We're good and paused, right?
    s_video_buffer_seen_ptr = s_video_buffer_pp_read_ptr = s_video_buffer_read_ptr;
  }

  p.Do(s_sync_ticks);
  p.Do(s_syncing_suspended);
}

void PauseAndLock(bool doLock, bool unpauseOnUnlock)
{
  if (doLock)
  {
    SyncGPU(SyncGPUReason::Other);
    EmulatorState(false);

    const SConfig& param = SConfig::GetInstance();

    if (!param.bCPUThread || s_use_deterministic_gpu_thread)
      return;

    s_gpu_mainloop.WaitYield(std::chrono::milliseconds(100), Host_YieldToUI);
  }
  else
  {
    if (unpauseOnUnlock)
      EmulatorState(true);
  }
}

void Init()
{
  // Padded so that SIMD overreads in the vertex loader are safe
  s_video_buffer = static_cast<u8*>(Common::AllocateMemoryPages(FIFO_SIZE + 4));
  ResetVideoBuffer();
  if (SConfig::GetInstance().bCPUThread)
    s_gpu_mainloop.Prepare();
  s_sync_ticks.store(0);
}

void Shutdown()
{
  if (s_gpu_mainloop.IsRunning())
    PanicAlert("Fifo shutting down while active");

  Common::FreeMemoryPages(s_video_buffer, FIFO_SIZE + 4);
  s_video_buffer = nullptr;
  s_video_buffer_write_ptr = nullptr;
  s_video_buffer_pp_read_ptr = nullptr;
  s_video_buffer_read_ptr = nullptr;
  s_video_buffer_seen_ptr = nullptr;
  s_fifo_aux_write_ptr = nullptr;
  s_fifo_aux_read_ptr = nullptr;
}

// May be executed from any thread, even the graphics thread.
// Created to allow for self shutdown.
void ExitGpuLoop()
{
  // This should break the wait loop in CPU thread
  CommandProcessor::fifo.bFF_GPReadEnable = false;
  FlushGpu();

  // Terminate GPU thread loop
  s_emu_running_state.Set();
  s_gpu_mainloop.Stop(s_gpu_mainloop.kNonBlock);
}

void EmulatorState(bool running)
{
  s_emu_running_state.Set(running);
  if (running)
    s_gpu_mainloop.Wakeup();
  else
    s_gpu_mainloop.AllowSleep();
}

void SyncGPU(SyncGPUReason reason, bool may_move_read_ptr)
{
  if (s_use_deterministic_gpu_thread)
  {
    s_gpu_mainloop.Wait();
    if (!s_gpu_mainloop.IsRunning())
      return;

    // Opportunistically reset FIFOs so we don't wrap around.
    if (may_move_read_ptr && s_fifo_aux_write_ptr != s_fifo_aux_read_ptr)
      PanicAlert("aux fifo not synced (%p, %p)", s_fifo_aux_write_ptr, s_fifo_aux_read_ptr);

    memmove(s_fifo_aux_data, s_fifo_aux_read_ptr, s_fifo_aux_write_ptr - s_fifo_aux_read_ptr);
    s_fifo_aux_write_ptr -= (s_fifo_aux_read_ptr - s_fifo_aux_data);
    s_fifo_aux_read_ptr = s_fifo_aux_data;

    if (may_move_read_ptr)
    {
      u8* write_ptr = s_video_buffer_write_ptr;

      // what's left over in the buffer
      size_t size = write_ptr - s_video_buffer_pp_read_ptr;

      memmove(s_video_buffer, s_video_buffer_pp_read_ptr, size);
      // This change always decreases the pointers.  We write seen_ptr
      // after write_ptr here, and read it before in RunGpuLoop, so
      // 'write_ptr > seen_ptr' there cannot become spuriously true.
      s_video_buffer_write_ptr = write_ptr = s_video_buffer + size;
      s_video_buffer_pp_read_ptr = s_video_buffer;
      s_video_buffer_read_ptr = s_video_buffer;
      s_video_buffer_seen_ptr = write_ptr;
    }
  }
}

void PushFifoAuxBuffer(const void* ptr, size_t size)
{
  if (size > (size_t)(s_fifo_aux_data + FIFO_SIZE - s_fifo_aux_write_ptr))
  {
    SyncGPU(SyncGPUReason::AuxSpace, /* may_move_read_ptr */ false);
    if (!s_gpu_mainloop.IsRunning())
    {
      // GPU is shutting down
      return;
    }
    if (size > (size_t)(s_fifo_aux_data + FIFO_SIZE - s_fifo_aux_write_ptr))
    {
      // That will sync us up to the last 32 bytes, so this short region
      // of FIFO would have to point to a 2MB display list or something.
      PanicAlert("absurdly large aux buffer");
      return;
    }
  }
  memcpy(s_fifo_aux_write_ptr, ptr, size);
  s_fifo_aux_write_ptr += size;
}

void* PopFifoAuxBuffer(size_t size)
{
  void* ret = s_fifo_aux_read_ptr;
  s_fifo_aux_read_ptr += size;
  return ret;
}

// Description: RunGpuLoop() sends data through this function.
static u32 ReadDataFromFifo(u32 readPtr)
{
  u32 len = 32;
  u8* write_ptr = s_video_buffer_write_ptr;
  if (len > (s_video_buffer + FIFO_SIZE - write_ptr))
  {
    size_t existing_len = write_ptr - s_video_buffer_read_ptr;
    if (len > (FIFO_SIZE - existing_len))
    {
      PanicAlert("FIFO out of bounds (existing %zu + new %u > %u)", existing_len, len, FIFO_SIZE);
      return 0;
    }
    memmove(s_video_buffer, s_video_buffer_read_ptr, existing_len);
    write_ptr = s_video_buffer + existing_len;
    s_video_buffer_read_ptr = s_video_buffer;
  }
  // Copy new video instructions to s_video_buffer for future use in rendering the new picture
  Memory::CopyFromEmu(write_ptr, readPtr, len);
  s_video_buffer_write_ptr = write_ptr + len;
  return len;
}

// The deterministic_gpu_thread version.
static u32 ReadDataFromFifoOnCPU(u32 readPtr, u32* needSize)
{
  u32 len = 32;
  u8* write_ptr = s_video_buffer_write_ptr;
  if (len > (s_video_buffer + FIFO_SIZE - write_ptr))
  {
    // We can't wrap around while the GPU is working on the data.
    // This should be very rare due to the reset in SyncGPU.
    SyncGPU(SyncGPUReason::Wraparound);
    if (!s_gpu_mainloop.IsRunning())
    {
      // GPU is shutting down, so the next asserts may fail
      return 0;
    }

    if (s_video_buffer_pp_read_ptr != s_video_buffer_read_ptr)
    {
      PanicAlert("desynced read pointers");
      return 0;
    }
    write_ptr = s_video_buffer_write_ptr;
    size_t existing_len = write_ptr - s_video_buffer_pp_read_ptr;
    if (len > (FIFO_SIZE - existing_len))
    {
      PanicAlert("FIFO out of bounds (existing %zu + new %u > %u)", existing_len, len, FIFO_SIZE);
      return 0;
    }
  }
  Memory::CopyFromEmu(write_ptr, readPtr, len);
  write_ptr += len;
  if (write_ptr - s_video_buffer_pp_read_ptr >= *needSize)
  {
    *needSize = 0;
    s_video_buffer_pp_read_ptr = OpcodeDecoder::Run<true>(
        DataReader(s_video_buffer_pp_read_ptr, write_ptr), nullptr, false, needSize);
  }
  // This would have to be locked if the GPU thread didn't spin.
  s_video_buffer_write_ptr = write_ptr;
  return len;
}

void ResetVideoBuffer()
{
  s_video_buffer_read_ptr = s_video_buffer;
  s_video_buffer_write_ptr = s_video_buffer;
  s_video_buffer_seen_ptr = s_video_buffer;
  s_video_buffer_pp_read_ptr = s_video_buffer;
  s_fifo_aux_write_ptr = s_fifo_aux_data;
  s_fifo_aux_read_ptr = s_fifo_aux_data;
}

// Description: Main FIFO update loop
// Purpose: Keep the Core HW updated about the CPU-GPU distance
void RunGpuLoop()
{
  AsyncRequests::GetInstance()->SetEnable(true);
  AsyncRequests::GetInstance()->SetPassthrough(false);

  s_gpu_mainloop.Run(
      [] {
        const SConfig& param = SConfig::GetInstance();

        // Do nothing while paused
        if (!s_emu_running_state.IsSet())
          return;

        if (s_use_deterministic_gpu_thread)
        {
          AsyncRequests::GetInstance()->PullEvents();

          // All the fifo/CP stuff is on the CPU.  We just need to run the opcode decoder.
          u8* seen_ptr = s_video_buffer_seen_ptr;
          u8* write_ptr = s_video_buffer_write_ptr;
          // See comment in SyncGPU
          if (write_ptr > seen_ptr)
          {
            s_video_buffer_read_ptr =
                OpcodeDecoder::Run(DataReader(s_video_buffer_read_ptr, write_ptr), nullptr, false);
            s_video_buffer_seen_ptr = write_ptr;
          }
        }
        else
        {
          CommandProcessor::SCPFifoStruct& fifo = CommandProcessor::fifo;
          AsyncRequests::GetInstance()->PullEvents();
          CommandProcessor::SetCPStatusFromGPU();

          u32 needSize = 0;
          // check if we are able to run this buffer
          while (!CommandProcessor::IsInterruptWaiting() && fifo.bFF_GPReadEnable &&
                 fifo.CPReadWriteDistance && !AtBreakpoint())
          {
            if (param.bSyncGPU && s_sync_ticks.load() < param.iSyncGpuMinDistance)
              break;

            u32 readPtr = fifo.CPReadPointer;
            u32 readSize = ReadDataFromFifo(readPtr);
            u8* write_ptr = s_video_buffer_write_ptr;

            readPtr += readSize;
            if (readPtr == fifo.CPEnd + 32)
              readPtr = fifo.CPBase;

            fifo.CPReadPointer = readPtr;
            Common::AtomicAdd(fifo.CPReadWriteDistance, -(s32)readSize);

            if (write_ptr - s_video_buffer_read_ptr >= needSize)
            {
              u32 cyclesExecuted = 0;
              needSize = 0;
              s_video_buffer_read_ptr =
                  OpcodeDecoder::Run(DataReader(s_video_buffer_read_ptr, write_ptr),
                                     &cyclesExecuted, false, &needSize);

              if ((write_ptr - s_video_buffer_read_ptr) == 0)
                Common::AtomicStore(fifo.SafeCPReadPointer, readPtr);

              if (param.bSyncGPU)
              {
                cyclesExecuted = (u32)(cyclesExecuted / param.fSyncGpuOverclock);
                int old = s_sync_ticks.fetch_sub(cyclesExecuted);
                if (old >= param.iSyncGpuMaxDistance &&
                    old - (int)cyclesExecuted < param.iSyncGpuMaxDistance)
                  s_sync_wakeup_event.Set();
              }
            }

            CommandProcessor::SetCPStatusFromGPU();
            // This call is pretty important in DualCore mode and must be called in the FIFO Loop.
            // If we don't, s_swapRequested or s_efbAccessRequested won't be set to false
            // leading the CPU thread to wait in Video_BeginField or Video_AccessEFB thus slowing
            // things down.
            AsyncRequests::GetInstance()->PullEvents();
          }

          // fast skip remaining GPU time if fifo is empty
          if (s_sync_ticks.load() > 0)
          {
            int old = s_sync_ticks.exchange(0);
            if (old >= param.iSyncGpuMaxDistance)
              s_sync_wakeup_event.Set();
          }
        }
      },
      100);

  AsyncRequests::GetInstance()->SetEnable(false);
  AsyncRequests::GetInstance()->SetPassthrough(true);
}

void FlushGpu()
{
  const SConfig& param = SConfig::GetInstance();

  if (!param.bCPUThread || s_use_deterministic_gpu_thread)
    return;

  s_gpu_mainloop.Wait();
}

void GpuMaySleep()
{
  s_gpu_mainloop.AllowSleep();
}

bool AtBreakpoint()
{
  CommandProcessor::SCPFifoStruct& fifo = CommandProcessor::fifo;
  return fifo.bFF_BPEnable && (fifo.CPReadPointer == fifo.CPBreakpoint);
}

void RunGpu()
{
  const SConfig& param = SConfig::GetInstance();

  // wake up GPU thread
  if (param.bCPUThread && !s_use_deterministic_gpu_thread)
  {
    s_gpu_mainloop.Wakeup();
  }

  // if the sync GPU callback is suspended, wake it up.
  if (!SConfig::GetInstance().bCPUThread || s_use_deterministic_gpu_thread ||
      SConfig::GetInstance().bSyncGPU)
  {
    if (s_syncing_suspended)
    {
      s_syncing_suspended = false;
      CoreTiming::ScheduleEvent(GPU_TIME_SLOT_SIZE, s_event_sync_gpu, GPU_TIME_SLOT_SIZE);
    }
  }
}

static int RunGpuOnCpu(int ticks)
{
  CommandProcessor::SCPFifoStruct& fifo = CommandProcessor::fifo;
  bool reset_simd_state = false;
  u32 need_size = 0;
  int available_ticks = int(ticks * SConfig::GetInstance().fSyncGpuOverclock) + s_sync_ticks.load();
  while (fifo.bFF_GPReadEnable && fifo.CPReadWriteDistance && !AtBreakpoint() &&
         available_ticks >= 0)
  {
    u32 read_size;
    if (s_use_deterministic_gpu_thread)
    {
      read_size = ReadDataFromFifoOnCPU(fifo.CPReadPointer, &need_size);
      s_gpu_mainloop.Wakeup();
    }
    else
    {
      if (!reset_simd_state)
      {
        FPURoundMode::SaveSIMDState();
        FPURoundMode::LoadDefaultSIMDState();
        reset_simd_state = true;
      }
      read_size = ReadDataFromFifo(fifo.CPReadPointer);
      u8* write_ptr = s_video_buffer_write_ptr;
      if (write_ptr - s_video_buffer_read_ptr >= need_size)
      {
        u32 cycles = 0;
        need_size = 0;
        s_video_buffer_read_ptr = OpcodeDecoder::Run(DataReader(s_video_buffer_read_ptr, write_ptr),
                                                     &cycles, false, &need_size);
        available_ticks -= cycles;
      }
    }

    fifo.CPReadPointer += read_size;
    if (fifo.CPReadPointer == fifo.CPEnd + 32)
      fifo.CPReadPointer = fifo.CPBase;
    fifo.CPReadWriteDistance -= read_size;
  }

  CommandProcessor::SetCPStatusFromGPU();

  if (reset_simd_state)
  {
    FPURoundMode::LoadSIMDState();
  }

  // Discard all available ticks as there is nothing to do any more.
  s_sync_ticks.store(std::min(available_ticks, 0));

  // If the GPU is idle, drop the handler.
  if (available_ticks >= 0)
    return -1;

  // Always wait at least for GPU_TIME_SLOT_SIZE cycles.
  return -available_ticks + GPU_TIME_SLOT_SIZE;
}

void UpdateWantDeterminism(bool want)
{
  // We are paused (or not running at all yet), so
  // it should be safe to change this.
  const SConfig& param = SConfig::GetInstance();
  bool gpu_thread = false;
  switch (param.m_GPUDeterminismMode)
  {
  case GPUDeterminismMode::Auto:
    gpu_thread = want;
    break;
  case GPUDeterminismMode::Disabled:
    gpu_thread = false;
    break;
  case GPUDeterminismMode::FakeCompletion:
    gpu_thread = true;
    break;
  }

  gpu_thread = gpu_thread && param.bCPUThread;

  if (s_use_deterministic_gpu_thread != gpu_thread)
  {
    s_use_deterministic_gpu_thread = gpu_thread;
    if (gpu_thread)
    {
      // These haven't been updated in non-deterministic mode.
      s_video_buffer_seen_ptr = s_video_buffer_pp_read_ptr = s_video_buffer_read_ptr;
      CopyPreprocessCPStateFromMain();
      VertexLoaderManager::MarkAllDirty();
    }
  }
}

bool UseDeterministicGPUThread()
{
  return s_use_deterministic_gpu_thread;
}

/* This function checks the emulated CPU - GPU distance and may wake up the GPU,
 * or block the CPU if required. It should be called by the CPU thread regularly.
 * @ticks The gone emulated CPU time.
 * @return A good time to call WaitForGpuThread() next.
 */
static int WaitForGpuThread(int ticks)
{
  const SConfig& param = SConfig::GetInstance();

  int old = s_sync_ticks.fetch_add(ticks);
  int now = old + ticks;

  // GPU is idle, so stop polling.
  if (old >= 0 && s_gpu_mainloop.IsDone())
    return -1;

  // Wakeup GPU
  if (old < param.iSyncGpuMinDistance && now >= param.iSyncGpuMinDistance)
    RunGpu();

  // If the GPU is still sleeping, wait for a longer time
  if (now < param.iSyncGpuMinDistance)
    return GPU_TIME_SLOT_SIZE + param.iSyncGpuMinDistance - now;

  // Wait for GPU
  if (now >= param.iSyncGpuMaxDistance)
    s_sync_wakeup_event.Wait();

  return GPU_TIME_SLOT_SIZE;
}

static void SyncGPUCallback(u64 ticks, s64 cyclesLate)
{
  ticks += cyclesLate;
  int next = -1;

  if (!SConfig::GetInstance().bCPUThread || s_use_deterministic_gpu_thread)
  {
    next = RunGpuOnCpu((int)ticks);
  }
  else if (SConfig::GetInstance().bSyncGPU)
  {
    next = WaitForGpuThread((int)ticks);
  }

  s_syncing_suspended = next < 0;
  if (!s_syncing_suspended)
    CoreTiming::ScheduleEvent(next, s_event_sync_gpu, next);
}

// Initialize GPU - CPU thread syncing, this gives us a deterministic way to start the GPU thread.
void Prepare()
{
  s_event_sync_gpu = CoreTiming::RegisterEvent("SyncGPUCallback", SyncGPUCallback);
  s_syncing_suspended = true;
}
}  // namespace Fifo
