// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PixelEngine.h"

#include <mutex>

#include "Common/BitField.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/System.h"

#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/PerfQueryBase.h"
#include "VideoCommon/VideoBackendBase.h"

namespace PixelEngine
{
enum
{
  INT_CAUSE_PE_TOKEN = 0x200,   // GP Token
  INT_CAUSE_PE_FINISH = 0x400,  // GP Finished
};

void PixelEngineManager::DoState(PointerWrap& p)
{
  p.Do(m_z_conf);
  p.Do(m_alpha_conf);
  p.Do(m_dst_alpha_conf);
  p.Do(m_alpha_mode_conf);
  p.Do(m_alpha_read);
  p.Do(m_control);

  p.Do(m_token);
  p.Do(m_token_pending);
  p.Do(m_token_interrupt_pending);
  p.Do(m_finish_interrupt_pending);
  p.Do(m_event_raised);

  p.Do(m_signal_token_interrupt);
  p.Do(m_signal_finish_interrupt);
}

void PixelEngineManager::Init(Core::System& system)
{
  m_control.hex = 0;
  m_z_conf.hex = 0;
  m_alpha_conf.hex = 0;
  m_dst_alpha_conf.hex = 0;
  m_alpha_mode_conf.hex = 0;
  m_alpha_read.hex = 0;

  m_token = 0;
  m_token_pending = 0;
  m_token_interrupt_pending = false;
  m_finish_interrupt_pending = false;
  m_event_raised = false;

  m_signal_token_interrupt = false;
  m_signal_finish_interrupt = false;

  m_event_type_set_token_finish =
      system.GetCoreTiming().RegisterEvent("SetTokenFinish", SetTokenFinish_OnMainThread_Static);
}

void PixelEngineManager::RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  // Directly mapped registers.
  struct
  {
    u32 addr;
    u16* ptr;
  } directly_mapped_vars[] = {
      {PE_ZCONF, &m_z_conf.hex},
      {PE_ALPHACONF, &m_alpha_conf.hex},
      {PE_DSTALPHACONF, &m_dst_alpha_conf.hex},
      {PE_ALPHAMODE, &m_alpha_mode_conf.hex},
      {PE_ALPHAREAD, &m_alpha_read.hex},
  };
  for (auto& mapped_var : directly_mapped_vars)
  {
    mmio->Register(base | mapped_var.addr, MMIO::DirectRead<u16>(mapped_var.ptr),
                   MMIO::DirectWrite<u16>(mapped_var.ptr));
  }

  // Performance queries registers: read only, need to call the video backend
  // to get the results.
  struct
  {
    u32 addr;
    PerfQueryType pqtype;
  } pq_regs[] = {
      {PE_PERF_ZCOMP_INPUT_ZCOMPLOC_L, PQ_ZCOMP_INPUT_ZCOMPLOC},
      {PE_PERF_ZCOMP_OUTPUT_ZCOMPLOC_L, PQ_ZCOMP_OUTPUT_ZCOMPLOC},
      {PE_PERF_ZCOMP_INPUT_L, PQ_ZCOMP_INPUT},
      {PE_PERF_ZCOMP_OUTPUT_L, PQ_ZCOMP_OUTPUT},
      {PE_PERF_BLEND_INPUT_L, PQ_BLEND_INPUT},
      {PE_PERF_EFB_COPY_CLOCKS_L, PQ_EFB_COPY_CLOCKS},
  };
  for (auto& pq_reg : pq_regs)
  {
    mmio->Register(base | pq_reg.addr, MMIO::ComplexRead<u16>([pq_reg](Core::System&, u32) {
                     return g_video_backend->Video_GetQueryResult(pq_reg.pqtype) & 0xFFFF;
                   }),
                   MMIO::InvalidWrite<u16>());
    mmio->Register(base | (pq_reg.addr + 2), MMIO::ComplexRead<u16>([pq_reg](Core::System&, u32) {
                     return g_video_backend->Video_GetQueryResult(pq_reg.pqtype) >> 16;
                   }),
                   MMIO::InvalidWrite<u16>());
  }

  // Control register
  mmio->Register(base | PE_CTRL_REGISTER, MMIO::DirectRead<u16>(&m_control.hex),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& pe = system.GetPixelEngine();

                   UPECtrlReg tmpCtrl{.hex = val};

                   if (tmpCtrl.pe_token)
                     pe.m_signal_token_interrupt = false;

                   if (tmpCtrl.pe_finish)
                     pe.m_signal_finish_interrupt = false;

                   pe.m_control.pe_token_enable = tmpCtrl.pe_token_enable.Value();
                   pe.m_control.pe_finish_enable = tmpCtrl.pe_finish_enable.Value();
                   pe.m_control.pe_token = false;   // this flag is write only
                   pe.m_control.pe_finish = false;  // this flag is write only

                   DEBUG_LOG_FMT(PIXELENGINE, "(w16) CTRL_REGISTER: {:#06x}", val);
                   pe.UpdateInterrupts();
                 }));

  // Token register, readonly.
  mmio->Register(base | PE_TOKEN_REG, MMIO::DirectRead<u16>(&m_token), MMIO::InvalidWrite<u16>());

  // BBOX registers, readonly and need to update a flag.
  for (int i = 0; i < 4; ++i)
  {
    mmio->Register(base | (PE_BBOX_LEFT + 2 * i),
                   MMIO::ComplexRead<u16>([i](Core::System& system, u32) {
                     g_bounding_box->Disable(system.GetPixelShaderManager());
                     return g_video_backend->Video_GetBoundingBox(i);
                   }),
                   MMIO::InvalidWrite<u16>());
  }
}

void PixelEngineManager::UpdateInterrupts()
{
  auto& system = Core::System::GetInstance();
  auto& processor_interface = system.GetProcessorInterface();

  // check if there is a token-interrupt
  processor_interface.SetInterrupt(INT_CAUSE_PE_TOKEN,
                                   m_signal_token_interrupt && m_control.pe_token_enable);

  // check if there is a finish-interrupt
  processor_interface.SetInterrupt(INT_CAUSE_PE_FINISH,
                                   m_signal_finish_interrupt && m_control.pe_finish_enable);
}

void PixelEngineManager::SetTokenFinish_OnMainThread_Static(Core::System& system, u64 userdata,
                                                            s64 cycles_late)
{
  system.GetPixelEngine().SetTokenFinish_OnMainThread(system, userdata, cycles_late);
}

void PixelEngineManager::SetTokenFinish_OnMainThread(Core::System& system, u64 userdata,
                                                     s64 cycles_late)
{
  std::unique_lock<std::mutex> lk(m_token_finish_mutex);
  m_event_raised = false;

  m_token = m_token_pending;

  if (m_token_interrupt_pending)
  {
    m_token_interrupt_pending = false;
    m_signal_token_interrupt = true;
    UpdateInterrupts();
  }

  if (m_finish_interrupt_pending)
  {
    m_finish_interrupt_pending = false;
    m_signal_finish_interrupt = true;
    UpdateInterrupts();
    lk.unlock();
    Core::FrameUpdateOnCPUThread();
  }
}

// Raise the event handler above on the CPU thread.
// m_token_finish_mutex must be locked.
// THIS IS EXECUTED FROM VIDEO THREAD
void PixelEngineManager::RaiseEvent(Core::System& system, int cycles_into_future)
{
  if (m_event_raised)
    return;

  m_event_raised = true;

  CoreTiming::FromThread from = CoreTiming::FromThread::NON_CPU;
  s64 cycles = 0;  // we don't care about timings for dual core mode.
  if (!system.IsDualCoreMode() || system.GetFifo().UseDeterministicGPUThread())
  {
    from = CoreTiming::FromThread::CPU;

    // Hack: Dolphin's single-core gpu timings are way too fast. Enforce a minimum delay to give
    //       games time to setup any interrupt state
    cycles = std::max(500, cycles_into_future);
  }
  system.GetCoreTiming().ScheduleEvent(cycles, m_event_type_set_token_finish, 0, from);
}

// SetToken
// THIS IS EXECUTED FROM VIDEO THREAD
void PixelEngineManager::SetToken(Core::System& system, const u16 token, const bool interrupt,
                                  int cycles_into_future)
{
  DEBUG_LOG_FMT(PIXELENGINE, "VIDEO Backend raises INT_CAUSE_PE_TOKEN (btw, token: {:04x})", token);

  std::lock_guard<std::mutex> lk(m_token_finish_mutex);

  m_token_pending = token;
  m_token_interrupt_pending |= interrupt;

  RaiseEvent(system, cycles_into_future);
}

// SetFinish
// THIS IS EXECUTED FROM VIDEO THREAD (BPStructs.cpp) when a new frame has been drawn
void PixelEngineManager::SetFinish(Core::System& system, int cycles_into_future)
{
  DEBUG_LOG_FMT(PIXELENGINE, "VIDEO Set Finish");

  std::lock_guard<std::mutex> lk(m_token_finish_mutex);

  m_finish_interrupt_pending |= true;

  RaiseEvent(system, cycles_into_future);
}

}  // namespace PixelEngine
