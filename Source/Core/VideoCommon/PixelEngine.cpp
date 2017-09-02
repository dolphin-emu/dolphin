// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// http://www.nvidia.com/object/General_FAQ.html#t6 !!!!!

#include "VideoCommon/PixelEngine.h"

#include <mutex>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/ProcessorInterface.h"
#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/PerfQueryBase.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/VideoBackendBase.h"

namespace PixelEngine
{
union UPEZConfReg
{
  u16 Hex;
  struct
  {
    u16 ZCompEnable : 1;  // Z Comparator Enable
    u16 Function : 3;
    u16 ZUpdEnable : 1;
    u16 : 11;
  };
};

union UPEAlphaConfReg
{
  u16 Hex;
  struct
  {
    u16 BMMath : 1;   // GX_BM_BLEND || GX_BM_SUBSTRACT
    u16 BMLogic : 1;  // GX_BM_LOGIC
    u16 Dither : 1;
    u16 ColorUpdEnable : 1;
    u16 AlphaUpdEnable : 1;
    u16 DstFactor : 3;
    u16 SrcFactor : 3;
    u16 Substract : 1;  // Additive mode by default
    u16 BlendOperator : 4;
  };
};

union UPEDstAlphaConfReg
{
  u16 Hex;
  struct
  {
    u16 DstAlpha : 8;
    u16 Enable : 1;
    u16 : 7;
  };
};

union UPEAlphaModeConfReg
{
  u16 Hex;
  struct
  {
    u16 Threshold : 8;
    u16 CompareMode : 8;
  };
};

// fifo Control Register
union UPECtrlReg
{
  struct
  {
    u16 PETokenEnable : 1;
    u16 PEFinishEnable : 1;
    u16 PEToken : 1;   // write only
    u16 PEFinish : 1;  // write only
    u16 : 12;
  };
  u16 Hex;
  UPECtrlReg() { Hex = 0; }
  UPECtrlReg(u16 _hex) { Hex = _hex; }
};

// STATE_TO_SAVE
static UPEZConfReg m_ZConf;
static UPEAlphaConfReg m_AlphaConf;
static UPEDstAlphaConfReg m_DstAlphaConf;
static UPEAlphaModeConfReg m_AlphaModeConf;
static UPEAlphaReadReg m_AlphaRead;
static UPECtrlReg m_Control;

static std::mutex s_token_finish_mutex;
static u16 s_token;
static u16 s_token_pending;
static bool s_token_interrupt_pending;
static bool s_finish_interrupt_pending;
static bool s_event_raised;

static bool s_signal_token_interrupt;
static bool s_signal_finish_interrupt;

static CoreTiming::EventType* et_SetTokenFinishOnMainThread;

enum
{
  INT_CAUSE_PE_TOKEN = 0x200,   // GP Token
  INT_CAUSE_PE_FINISH = 0x400,  // GP Finished
};

void DoState(PointerWrap& p)
{
  p.Do(m_ZConf);
  p.Do(m_AlphaConf);
  p.Do(m_DstAlphaConf);
  p.Do(m_AlphaModeConf);
  p.Do(m_AlphaRead);
  p.DoPOD(m_Control);

  p.Do(s_token);
  p.Do(s_token_pending);
  p.Do(s_token_interrupt_pending);
  p.Do(s_finish_interrupt_pending);
  p.Do(s_event_raised);

  p.Do(s_signal_token_interrupt);
  p.Do(s_signal_finish_interrupt);
}

static void UpdateInterrupts();
static void SetTokenFinish_OnMainThread(u64 userdata, s64 cyclesLate);

void Init()
{
  m_Control.Hex = 0;
  m_ZConf.Hex = 0;
  m_AlphaConf.Hex = 0;
  m_DstAlphaConf.Hex = 0;
  m_AlphaModeConf.Hex = 0;
  m_AlphaRead.Hex = 0;

  s_token = 0;
  s_token_pending = 0;
  s_token_interrupt_pending = false;
  s_finish_interrupt_pending = false;
  s_event_raised = false;

  s_signal_token_interrupt = false;
  s_signal_finish_interrupt = false;

  et_SetTokenFinishOnMainThread =
      CoreTiming::RegisterEvent("SetTokenFinish", SetTokenFinish_OnMainThread);
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  // Directly mapped registers.
  struct
  {
    u32 addr;
    u16* ptr;
  } directly_mapped_vars[] = {
      {PE_ZCONF, &m_ZConf.Hex},
      {PE_ALPHACONF, &m_AlphaConf.Hex},
      {PE_DSTALPHACONF, &m_DstAlphaConf.Hex},
      {PE_ALPHAMODE, &m_AlphaModeConf.Hex},
      {PE_ALPHAREAD, &m_AlphaRead.Hex},
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
    mmio->Register(base | pq_reg.addr, MMIO::ComplexRead<u16>([pq_reg](u32) {
                     return g_video_backend->Video_GetQueryResult(pq_reg.pqtype) & 0xFFFF;
                   }),
                   MMIO::InvalidWrite<u16>());
    mmio->Register(base | (pq_reg.addr + 2), MMIO::ComplexRead<u16>([pq_reg](u32) {
                     return g_video_backend->Video_GetQueryResult(pq_reg.pqtype) >> 16;
                   }),
                   MMIO::InvalidWrite<u16>());
  }

  // Control register
  mmio->Register(base | PE_CTRL_REGISTER, MMIO::DirectRead<u16>(&m_Control.Hex),
                 MMIO::ComplexWrite<u16>([](u32, u16 val) {
                   UPECtrlReg tmpCtrl(val);

                   if (tmpCtrl.PEToken)
                     s_signal_token_interrupt = false;

                   if (tmpCtrl.PEFinish)
                     s_signal_finish_interrupt = false;

                   m_Control.PETokenEnable = tmpCtrl.PETokenEnable;
                   m_Control.PEFinishEnable = tmpCtrl.PEFinishEnable;
                   m_Control.PEToken = 0;   // this flag is write only
                   m_Control.PEFinish = 0;  // this flag is write only

                   DEBUG_LOG(PIXELENGINE, "(w16) CTRL_REGISTER: 0x%04x", val);
                   UpdateInterrupts();
                 }));

  // Token register, readonly.
  mmio->Register(base | PE_TOKEN_REG, MMIO::DirectRead<u16>(&s_token), MMIO::InvalidWrite<u16>());

  // BBOX registers, readonly and need to update a flag.
  for (int i = 0; i < 4; ++i)
  {
    mmio->Register(base | (PE_BBOX_LEFT + 2 * i), MMIO::ComplexRead<u16>([i](u32) {
                     BoundingBox::active = false;
                     PixelShaderManager::SetBoundingBoxActive(false);
                     return g_video_backend->Video_GetBoundingBox(i);
                   }),
                   MMIO::InvalidWrite<u16>());
  }
}

static void UpdateInterrupts()
{
  // check if there is a token-interrupt
  ProcessorInterface::SetInterrupt(INT_CAUSE_PE_TOKEN,
                                   s_signal_token_interrupt && m_Control.PETokenEnable);

  // check if there is a finish-interrupt
  ProcessorInterface::SetInterrupt(INT_CAUSE_PE_FINISH,
                                   s_signal_finish_interrupt && m_Control.PEFinishEnable);
}

static void SetTokenFinish_OnMainThread(u64 userdata, s64 cyclesLate)
{
  std::unique_lock<std::mutex> lk(s_token_finish_mutex);
  s_event_raised = false;

  s_token = s_token_pending;

  if (s_token_interrupt_pending)
  {
    s_token_interrupt_pending = false;
    s_signal_token_interrupt = true;
    UpdateInterrupts();
  }

  if (s_finish_interrupt_pending)
  {
    s_finish_interrupt_pending = false;
    s_signal_finish_interrupt = true;
    UpdateInterrupts();
    lk.unlock();
    Core::FrameUpdateOnCPUThread();
  }
}

// Raise the event handler above on the CPU thread.
// s_token_finish_mutex must be locked.
// THIS IS EXECUTED FROM VIDEO THREAD
static void RaiseEvent()
{
  if (s_event_raised)
    return;

  s_event_raised = true;

  CoreTiming::FromThread from = CoreTiming::FromThread::NON_CPU;
  if (!SConfig::GetInstance().bCPUThread || Fifo::UseDeterministicGPUThread())
    from = CoreTiming::FromThread::CPU;
  CoreTiming::ScheduleEvent(0, et_SetTokenFinishOnMainThread, 0, from);
}

// SetToken
// THIS IS EXECUTED FROM VIDEO THREAD
void SetToken(const u16 token, const bool interrupt)
{
  DEBUG_LOG(PIXELENGINE, "VIDEO Backend raises INT_CAUSE_PE_TOKEN (btw, token: %04x)", token);

  std::lock_guard<std::mutex> lk(s_token_finish_mutex);

  s_token_pending = token;
  s_token_interrupt_pending |= interrupt;

  RaiseEvent();
}

// SetFinish
// THIS IS EXECUTED FROM VIDEO THREAD (BPStructs.cpp) when a new frame has been drawn
void SetFinish()
{
  DEBUG_LOG(PIXELENGINE, "VIDEO Set Finish");

  std::lock_guard<std::mutex> lk(s_token_finish_mutex);

  s_finish_interrupt_pending |= true;

  RaiseEvent();
}

UPEAlphaReadReg GetAlphaReadMode()
{
  return m_AlphaRead;
}

}  // end of namespace PixelEngine
