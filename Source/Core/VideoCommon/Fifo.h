// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <cstddef>
#include <optional>

#include "Common/BlockingLoop.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Flag.h"

class PointerWrap;

namespace Core
{
class System;
}
namespace CoreTiming
{
struct EventType;
}

namespace Fifo
{
// Used for diagnostics.
enum class SyncGPUReason
{
  Other,
  Wraparound,
  EFBPoke,
  PerfQuery,
  BBox,
  Swap,
  AuxSpace,
};

class FifoManager final
{
public:
  FifoManager();
  FifoManager(const FifoManager& other) = delete;
  FifoManager(FifoManager&& other) = delete;
  FifoManager& operator=(const FifoManager& other) = delete;
  FifoManager& operator=(FifoManager&& other) = delete;
  ~FifoManager();

  void Init(Core::System& system);
  void Shutdown();
  void Prepare(Core::System& system);  // Must be called from the CPU thread.
  void DoState(PointerWrap& f);
  void PauseAndLock(Core::System& system, bool doLock, bool unpauseOnUnlock);
  void UpdateWantDeterminism(Core::System& system, bool want);
  bool UseDeterministicGPUThread() const { return m_use_deterministic_gpu_thread; }

  // In deterministic GPU thread mode this waits for the GPU to be done with pending work.
  void SyncGPU(SyncGPUReason reason, bool may_move_read_ptr = true);

  // In single core mode, this runs the GPU for a single slice.
  // In dual core mode, this synchronizes with the GPU thread.
  void SyncGPUForRegisterAccess(Core::System& system);

  void PushFifoAuxBuffer(const void* ptr, size_t size);
  void* PopFifoAuxBuffer(size_t size);

  void FlushGpu(Core::System& system);
  void RunGpu(Core::System& system);
  void GpuMaySleep();
  void RunGpuLoop(Core::System& system);
  void ExitGpuLoop(Core::System& system);
  void EmulatorState(bool running);
  void ResetVideoBuffer();

private:
  void RefreshConfig();
  void ReadDataFromFifo(Core::System& system, u32 readPtr);
  void ReadDataFromFifoOnCPU(Core::System& system, u32 readPtr);
  int RunGpuOnCpu(Core::System& system, int ticks);
  int WaitForGpuThread(Core::System& system, int ticks);
  static void SyncGPUCallback(Core::System& system, u64 ticks, s64 cyclesLate);

  static constexpr u32 FIFO_SIZE = 2 * 1024 * 1024;

  Common::BlockingLoop m_gpu_mainloop;

  Common::Flag m_emu_running_state;

  // Most of this array is unlikely to be faulted in...
  u8 m_fifo_aux_data[FIFO_SIZE]{};
  u8* m_fifo_aux_write_ptr = nullptr;
  u8* m_fifo_aux_read_ptr = nullptr;

  // This could be in SConfig, but it depends on multiple settings
  // and can change at runtime.
  bool m_use_deterministic_gpu_thread = false;

  CoreTiming::EventType* m_event_sync_gpu = nullptr;

  // STATE_TO_SAVE
  u8* m_video_buffer = nullptr;
  u8* m_video_buffer_read_ptr = nullptr;
  std::atomic<u8*> m_video_buffer_write_ptr = nullptr;
  std::atomic<u8*> m_video_buffer_seen_ptr = nullptr;
  u8* m_video_buffer_pp_read_ptr = nullptr;
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

  std::atomic<int> m_sync_ticks = 0;
  bool m_syncing_suspended = false;
  Common::Event m_sync_wakeup_event;

  std::optional<size_t> m_config_callback_id = std::nullopt;
  bool m_config_sync_gpu = false;
  int m_config_sync_gpu_max_distance = 0;
  int m_config_sync_gpu_min_distance = 0;
  float m_config_sync_gpu_overclock = 0.0f;
};

bool AtBreakpoint(Core::System& system);
}  // namespace Fifo
