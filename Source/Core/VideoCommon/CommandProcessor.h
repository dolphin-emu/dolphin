// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>

#include "Common/BitFieldView.h"
#include "Common/CommonTypes.h"

class PointerWrap;
namespace MMIO
{
class Mapping;
}

namespace CommandProcessor
{
struct SCPFifoStruct
{
  // fifo registers
  std::atomic<u32> CPBase;
  std::atomic<u32> CPEnd;
  u32 CPHiWatermark = 0;
  u32 CPLoWatermark = 0;
  std::atomic<u32> CPReadWriteDistance;
  std::atomic<u32> CPWritePointer;
  std::atomic<u32> CPReadPointer;
  std::atomic<u32> CPBreakpoint;
  std::atomic<u32> SafeCPReadPointer;

  std::atomic<bool> bFF_GPLinkEnable;
  std::atomic<bool> bFF_GPReadEnable;
  std::atomic<bool> bFF_BPEnable;
  std::atomic<bool> bFF_BPInt;
  std::atomic<bool> bFF_Breakpoint;

  std::atomic<bool> bFF_LoWatermarkInt;
  std::atomic<bool> bFF_HiWatermarkInt;

  std::atomic<bool> bFF_LoWatermark;
  std::atomic<bool> bFF_HiWatermark;

  void Init();
  void DoState(PointerWrap& p);
};

// This one is shared between gfx thread and emulator thread.
// It is only used by the Fifo and by the CommandProcessor.
extern SCPFifoStruct fifo;

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
struct CPStatusReg
{
  BFVIEW(bool, 1, 0, OverflowHiWatermark)
  BFVIEW(bool, 1, 1, UnderflowLoWatermark)
  BFVIEW(bool, 1, 2, ReadIdle)
  BFVIEW(bool, 1, 3, CommandIdle)
  BFVIEW(bool, 1, 4, Breakpoint)

  u16 Hex;
  CPStatusReg() { Hex = 0; }
  CPStatusReg(u16 _hex) { Hex = _hex; }
};

// Fifo Control Register
struct CPCtrlReg
{
  BFVIEW(bool, 1, 0, GPReadEnable)
  BFVIEW(bool, 1, 1, BPEnable)
  BFVIEW(bool, 1, 2, FifoOverflowIntEnable)
  BFVIEW(bool, 1, 3, FifoUnderflowIntEnable)
  BFVIEW(bool, 1, 4, GPLinkEnable)
  BFVIEW(bool, 1, 5, BPInt)

  u16 Hex;
  CPCtrlReg() { Hex = 0; }
  CPCtrlReg(u16 _hex) { Hex = _hex; }
};

// Fifo Clear Register
struct CPClearReg
{
  BFVIEW(bool, 1, 0, ClearFifoOverflow)
  BFVIEW(bool, 1, 1, ClearFifoUnderflow)
  BFVIEW(bool, 1, 2, ClearMetrices)

  u16 Hex;
  CPClearReg() { Hex = 0; }
  CPClearReg(u16 _hex) { Hex = _hex; }
};

// Init
void Init();
void DoState(PointerWrap& p);

void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

void SetCPStatusFromGPU();
void SetCPStatusFromCPU();
void GatherPipeBursted();
void UpdateInterrupts(u64 userdata);
void UpdateInterruptsFromVideoBackend(u64 userdata);

bool IsInterruptWaiting();

void SetCpClearRegister();
void SetCpControlRegister();
void SetCpStatusRegister();

void HandleUnknownOpcode(u8 cmd_byte, const u8* buffer, bool preprocess);

u32 GetPhysicalAddressMask();

}  // namespace CommandProcessor
