// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>

#include "Common/CommonTypes.h"
#include "Common/Flag.h"

class PointerWrap;
namespace MMIO
{
class Mapping;
}
namespace Core
{
class System;
}
namespace CoreTiming
{
struct EventType;
}

namespace CommandProcessor
{
struct SCPFifoStruct
{
  // fifo registers
  std::atomic<u32> CPBase = 0;
  std::atomic<u32> CPEnd = 0;
  u32 CPHiWatermark = 0;
  u32 CPLoWatermark = 0;
  std::atomic<u32> CPReadWriteDistance = 0;
  std::atomic<u32> CPWritePointer = 0;
  std::atomic<u32> CPReadPointer = 0;
  std::atomic<u32> CPBreakpoint = 0;
  std::atomic<u32> SafeCPReadPointer = 0;

  std::atomic<u32> bFF_GPLinkEnable = 0;
  std::atomic<u32> bFF_GPReadEnable = 0;
  std::atomic<u32> bFF_BPEnable = 0;
  std::atomic<u32> bFF_BPInt = 0;
  std::atomic<u32> bFF_Breakpoint = 0;

  std::atomic<u32> bFF_LoWatermarkInt = 0;
  std::atomic<u32> bFF_HiWatermarkInt = 0;

  std::atomic<u32> bFF_LoWatermark = 0;
  std::atomic<u32> bFF_HiWatermark = 0;

  void Init();
  void DoState(PointerWrap& p);
};

// internal hardware addresses
enum
{
  STATUS_REGISTER = 0x00,
  CTRL_REGISTER = 0x02,
  CLEAR_REGISTER = 0x04,
  PERF_SELECT = 0x06,
  FIFO_TOKEN_REGISTER = 0x0E,
  FIFO_BOUNDING_BOX_LEFT = 0x10,
  FIFO_BOUNDING_BOX_RIGHT = 0x12,
  FIFO_BOUNDING_BOX_TOP = 0x14,
  FIFO_BOUNDING_BOX_BOTTOM = 0x16,
  FIFO_BASE_LO = 0x20,
  FIFO_BASE_HI = 0x22,
  FIFO_END_LO = 0x24,
  FIFO_END_HI = 0x26,
  FIFO_HI_WATERMARK_LO = 0x28,
  FIFO_HI_WATERMARK_HI = 0x2a,
  FIFO_LO_WATERMARK_LO = 0x2c,
  FIFO_LO_WATERMARK_HI = 0x2e,
  FIFO_RW_DISTANCE_LO = 0x30,
  FIFO_RW_DISTANCE_HI = 0x32,
  FIFO_WRITE_POINTER_LO = 0x34,
  FIFO_WRITE_POINTER_HI = 0x36,
  FIFO_READ_POINTER_LO = 0x38,
  FIFO_READ_POINTER_HI = 0x3A,
  FIFO_BP_LO = 0x3C,
  FIFO_BP_HI = 0x3E,
  XF_RASBUSY_L = 0x40,
  XF_RASBUSY_H = 0x42,
  XF_CLKS_L = 0x44,
  XF_CLKS_H = 0x46,
  XF_WAIT_IN_L = 0x48,
  XF_WAIT_IN_H = 0x4a,
  XF_WAIT_OUT_L = 0x4c,
  XF_WAIT_OUT_H = 0x4e,
  VCACHE_METRIC_CHECK_L = 0x50,
  VCACHE_METRIC_CHECK_H = 0x52,
  VCACHE_METRIC_MISS_L = 0x54,
  VCACHE_METRIC_MISS_H = 0x56,
  VCACHE_METRIC_STALL_L = 0x58,
  VCACHE_METRIC_STALL_H = 0x5A,
  CLKS_PER_VTX_IN_L = 0x60,
  CLKS_PER_VTX_IN_H = 0x62,
  CLKS_PER_VTX_OUT = 0x64,
};

enum
{
  INT_CAUSE_CP = 0x800
};

// Fifo Status Register
union UCPStatusReg
{
  struct
  {
    u16 OverflowHiWatermark : 1;
    u16 UnderflowLoWatermark : 1;
    u16 ReadIdle : 1;
    u16 CommandIdle : 1;
    u16 Breakpoint : 1;
    u16 : 11;
  };
  u16 Hex;
  UCPStatusReg() { Hex = 0; }
  UCPStatusReg(u16 _hex) { Hex = _hex; }
};

// Fifo Control Register
union UCPCtrlReg
{
  struct
  {
    u16 GPReadEnable : 1;
    u16 BPEnable : 1;
    u16 FifoOverflowIntEnable : 1;
    u16 FifoUnderflowIntEnable : 1;
    u16 GPLinkEnable : 1;
    u16 BPInt : 1;
    u16 : 10;
  };
  u16 Hex;
  UCPCtrlReg() { Hex = 0; }
  UCPCtrlReg(u16 _hex) { Hex = _hex; }
};

// Fifo Clear Register
union UCPClearReg
{
  struct
  {
    u16 ClearFifoOverflow : 1;
    u16 ClearFifoUnderflow : 1;
    u16 ClearMetrices : 1;
    u16 : 13;
  };
  u16 Hex;
  UCPClearReg() { Hex = 0; }
  UCPClearReg(u16 _hex) { Hex = _hex; }
};

constexpr u32 GetPhysicalAddressMask(bool is_wii)
{
  // Physical addresses in CP seem to ignore some of the upper bits (depending on platform)
  // This can be observed in CP MMIO registers by setting to 0xffffffff and then reading back.
  return is_wii ? 0x1fffffff : 0x03ffffff;
}

class CommandProcessorManager
{
public:
  explicit CommandProcessorManager(Core::System& system);

  void Init();
  void DoState(PointerWrap& p);

  void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

  void SetCPStatusFromGPU();
  void SetCPStatusFromCPU();
  void GatherPipeBursted();
  void UpdateInterrupts(u64 userdata);
  void UpdateInterruptsFromVideoBackend(u64 userdata);

  bool IsInterruptWaiting() const;

  void SetCpClearRegister();
  void SetCpControlRegister();
  void SetCpStatusRegister();

  void HandleUnknownOpcode(u8 cmd_byte, const u8* buffer, bool preprocess);

  // This one is shared between gfx thread and emulator thread.
  // It is only used by the Fifo and by the CommandProcessor.
  SCPFifoStruct& GetFifo() { return m_fifo; }

private:
  SCPFifoStruct m_fifo;

  CoreTiming::EventType* m_event_type_update_interrupts = nullptr;

  // STATE_TO_SAVE
  UCPStatusReg m_cp_status_reg;
  UCPCtrlReg m_cp_ctrl_reg;
  UCPClearReg m_cp_clear_reg;

  u16 m_bbox_left = 0;
  u16 m_bbox_top = 0;
  u16 m_bbox_right = 0;
  u16 m_bbox_bottom = 0;
  u16 m_token_reg = 0;

  Common::Flag m_interrupt_set;
  Common::Flag m_interrupt_waiting;

  bool m_is_fifo_error_seen = false;

  Core::System& m_system;
};

}  // namespace CommandProcessor
