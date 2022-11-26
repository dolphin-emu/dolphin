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
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoBackendBase.h"

namespace PixelEngine
{
// Note: These enums are (assumed to be) identical to the one in BPMemory, but the base type is set
// to u16 instead of u32 for BitField
enum class CompareMode : u16
{
  Never = 0,
  Less = 1,
  Equal = 2,
  LEqual = 3,
  Greater = 4,
  NEqual = 5,
  GEqual = 6,
  Always = 7
};

union UPEZConfReg
{
  u16 hex;
  BitField<0, 1, bool, u16> z_comparator_enable;
  BitField<1, 3, CompareMode, u16> function;
  BitField<4, 1, bool, u16> z_update_enable;
};

enum class SrcBlendFactor : u16
{
  Zero = 0,
  One = 1,
  DstClr = 2,
  InvDstClr = 3,
  SrcAlpha = 4,
  InvSrcAlpha = 5,
  DstAlpha = 6,
  InvDstAlpha = 7
};

enum class DstBlendFactor : u16
{
  Zero = 0,
  One = 1,
  SrcClr = 2,
  InvSrcClr = 3,
  SrcAlpha = 4,
  InvSrcAlpha = 5,
  DstAlpha = 6,
  InvDstAlpha = 7
};

enum class LogicOp : u16
{
  Clear = 0,
  And = 1,
  AndReverse = 2,
  Copy = 3,
  AndInverted = 4,
  NoOp = 5,
  Xor = 6,
  Or = 7,
  Nor = 8,
  Equiv = 9,
  Invert = 10,
  OrReverse = 11,
  CopyInverted = 12,
  OrInverted = 13,
  Nand = 14,
  Set = 15
};

union UPEAlphaConfReg
{
  u16 hex;
  BitField<0, 1, bool, u16> blend;  // Set for GX_BM_BLEND or GX_BM_SUBTRACT
  BitField<1, 1, bool, u16> logic;  // Set for GX_BM_LOGIC
  BitField<2, 1, bool, u16> dither;
  BitField<3, 1, bool, u16> color_update_enable;
  BitField<4, 1, bool, u16> alpha_update_enable;
  BitField<5, 3, DstBlendFactor, u16> dst_factor;
  BitField<8, 3, SrcBlendFactor, u16> src_factor;
  BitField<11, 1, bool, u16> subtract;  // Set for GX_BM_SUBTRACT
  BitField<12, 4, LogicOp, u16> logic_op;
};

union UPEDstAlphaConfReg
{
  u16 hex;
  BitField<0, 8, u8, u16> alpha;
  BitField<8, 1, bool, u16> enable;
};

union UPEAlphaModeConfReg
{
  u16 hex;
  BitField<0, 8, u8, u16> threshold;
  // Yagcd and libogc use 8 bits for this, but the enum only needs 3
  BitField<8, 3, CompareMode, u16> compare_mode;
};

union UPEAlphaReadReg
{
  u16 hex;
  BitField<0, 2, AlphaReadMode, u16> read_mode;
};

// fifo Control Register
union UPECtrlReg
{
  u16 hex;
  BitField<0, 1, bool, u16> pe_token_enable;
  BitField<1, 1, bool, u16> pe_finish_enable;
  BitField<2, 1, bool, u16> pe_token;   // Write only
  BitField<3, 1, bool, u16> pe_finish;  // Write only
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
  p.Do(m_Control);

  p.Do(s_token);
  p.Do(s_token_pending);
  p.Do(s_token_interrupt_pending);
  p.Do(s_finish_interrupt_pending);
  p.Do(s_event_raised);

  p.Do(s_signal_token_interrupt);
  p.Do(s_signal_finish_interrupt);
}

static void UpdateInterrupts();
static void SetTokenFinish_OnMainThread(Core::System& system, u64 userdata, s64 cyclesLate);

void Init()
{
  m_Control.hex = 0;
  m_ZConf.hex = 0;
  m_AlphaConf.hex = 0;
  m_DstAlphaConf.hex = 0;
  m_AlphaModeConf.hex = 0;
  m_AlphaRead.hex = 0;

  s_token = 0;
  s_token_pending = 0;
  s_token_interrupt_pending = false;
  s_finish_interrupt_pending = false;
  s_event_raised = false;

  s_signal_token_interrupt = false;
  s_signal_finish_interrupt = false;

  et_SetTokenFinishOnMainThread = Core::System::GetInstance().GetCoreTiming().RegisterEvent(
      "SetTokenFinish", SetTokenFinish_OnMainThread);
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  // Directly mapped registers.
  struct
  {
    u32 addr;
    u16* ptr;
  } directly_mapped_vars[] = {
      {PE_ZCONF, &m_ZConf.hex},
      {PE_ALPHACONF, &m_AlphaConf.hex},
      {PE_DSTALPHACONF, &m_DstAlphaConf.hex},
      {PE_ALPHAMODE, &m_AlphaModeConf.hex},
      {PE_ALPHAREAD, &m_AlphaRead.hex},
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
  mmio->Register(base | PE_CTRL_REGISTER, MMIO::DirectRead<u16>(&m_Control.hex),
                 MMIO::ComplexWrite<u16>([](Core::System&, u32, u16 val) {
                   UPECtrlReg tmpCtrl{.hex = val};

                   if (tmpCtrl.pe_token)
                     s_signal_token_interrupt = false;

                   if (tmpCtrl.pe_finish)
                     s_signal_finish_interrupt = false;

                   m_Control.pe_token_enable = tmpCtrl.pe_token_enable.Value();
                   m_Control.pe_finish_enable = tmpCtrl.pe_finish_enable.Value();
                   m_Control.pe_token = false;   // this flag is write only
                   m_Control.pe_finish = false;  // this flag is write only

                   DEBUG_LOG_FMT(PIXELENGINE, "(w16) CTRL_REGISTER: {:#06x}", val);
                   UpdateInterrupts();
                 }));

  // Token register, readonly.
  mmio->Register(base | PE_TOKEN_REG, MMIO::DirectRead<u16>(&s_token), MMIO::InvalidWrite<u16>());

  // BBOX registers, readonly and need to update a flag.
  for (int i = 0; i < 4; ++i)
  {
    mmio->Register(base | (PE_BBOX_LEFT + 2 * i), MMIO::ComplexRead<u16>([i](Core::System&, u32) {
                     g_renderer->BBoxDisable();
                     return g_video_backend->Video_GetBoundingBox(i);
                   }),
                   MMIO::InvalidWrite<u16>());
  }
}

static void UpdateInterrupts()
{
  // check if there is a token-interrupt
  ProcessorInterface::SetInterrupt(INT_CAUSE_PE_TOKEN,
                                   s_signal_token_interrupt && m_Control.pe_token_enable);

  // check if there is a finish-interrupt
  ProcessorInterface::SetInterrupt(INT_CAUSE_PE_FINISH,
                                   s_signal_finish_interrupt && m_Control.pe_finish_enable);
}

static void SetTokenFinish_OnMainThread(Core::System& system, u64 userdata, s64 cyclesLate)
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
static void RaiseEvent(int cycles_into_future)
{
  if (s_event_raised)
    return;

  s_event_raised = true;

  CoreTiming::FromThread from = CoreTiming::FromThread::NON_CPU;
  s64 cycles = 0;  // we don't care about timings for dual core mode.
  if (!Core::System::GetInstance().IsDualCoreMode() || Fifo::UseDeterministicGPUThread())
  {
    from = CoreTiming::FromThread::CPU;

    // Hack: Dolphin's single-core gpu timings are way too fast. Enforce a minimum delay to give
    //       games time to setup any interrupt state
    cycles = std::max(500, cycles_into_future);
  }
  Core::System::GetInstance().GetCoreTiming().ScheduleEvent(cycles, et_SetTokenFinishOnMainThread,
                                                            0, from);
}

// SetToken
// THIS IS EXECUTED FROM VIDEO THREAD
void SetToken(const u16 token, const bool interrupt, int cycles_into_future)
{
  DEBUG_LOG_FMT(PIXELENGINE, "VIDEO Backend raises INT_CAUSE_PE_TOKEN (btw, token: {:04x})", token);

  std::lock_guard<std::mutex> lk(s_token_finish_mutex);

  s_token_pending = token;
  s_token_interrupt_pending |= interrupt;

  RaiseEvent(cycles_into_future);
}

// SetFinish
// THIS IS EXECUTED FROM VIDEO THREAD (BPStructs.cpp) when a new frame has been drawn
void SetFinish(int cycles_into_future)
{
  DEBUG_LOG_FMT(PIXELENGINE, "VIDEO Set Finish");

  std::lock_guard<std::mutex> lk(s_token_finish_mutex);

  s_finish_interrupt_pending |= true;

  RaiseEvent(cycles_into_future);
}

AlphaReadMode GetAlphaReadMode()
{
  return m_AlphaRead.read_mode;
}

}  // namespace PixelEngine
