// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>

#include "Common/BitField.h"
#include "Common/CommonTypes.h"

class PointerWrap;

namespace Core
{
class System;
}
namespace CoreTiming
{
struct EventType;
}
namespace MMIO
{
class Mapping;
}

namespace PixelEngine
{
// internal hardware addresses
enum
{
  PE_ZCONF = 0x00,          // Z Config
  PE_ALPHACONF = 0x02,      // Alpha Config
  PE_DSTALPHACONF = 0x04,   // Destination Alpha Config
  PE_ALPHAMODE = 0x06,      // Alpha Mode Config
  PE_ALPHAREAD = 0x08,      // Alpha Read
  PE_CTRL_REGISTER = 0x0a,  // Control
  PE_TOKEN_REG = 0x0e,      // Token
  PE_BBOX_LEFT = 0x10,      // Bounding Box Left Pixel
  PE_BBOX_RIGHT = 0x12,     // Bounding Box Right Pixel
  PE_BBOX_TOP = 0x14,       // Bounding Box Top Pixel
  PE_BBOX_BOTTOM = 0x16,    // Bounding Box Bottom Pixel

  // NOTE: Order not verified
  // These indicate the number of quads that are being used as input/output for each particular
  // stage
  PE_PERF_ZCOMP_INPUT_ZCOMPLOC_L = 0x18,
  PE_PERF_ZCOMP_INPUT_ZCOMPLOC_H = 0x1a,
  PE_PERF_ZCOMP_OUTPUT_ZCOMPLOC_L = 0x1c,
  PE_PERF_ZCOMP_OUTPUT_ZCOMPLOC_H = 0x1e,
  PE_PERF_ZCOMP_INPUT_L = 0x20,
  PE_PERF_ZCOMP_INPUT_H = 0x22,
  PE_PERF_ZCOMP_OUTPUT_L = 0x24,
  PE_PERF_ZCOMP_OUTPUT_H = 0x26,
  PE_PERF_BLEND_INPUT_L = 0x28,
  PE_PERF_BLEND_INPUT_H = 0x2a,
  PE_PERF_EFB_COPY_CLOCKS_L = 0x2c,
  PE_PERF_EFB_COPY_CLOCKS_H = 0x2e,
};

// ReadMode specifies the returned alpha channel for EFB peeks
enum class AlphaReadMode : u16
{
  Read00 = 0,    // Always read 0x00
  ReadFF = 1,    // Always read 0xFF
  ReadNone = 2,  // Always read the real alpha value
};

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
  u16 hex = 0;
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
  u16 hex = 0;
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
  u16 hex = 0;
  BitField<0, 8, u8, u16> alpha;
  BitField<8, 1, bool, u16> enable;
};

union UPEAlphaModeConfReg
{
  u16 hex = 0;
  BitField<0, 8, u8, u16> threshold;
  // Yagcd and libogc use 8 bits for this, but the enum only needs 3
  BitField<8, 3, CompareMode, u16> compare_mode;
};

union UPEAlphaReadReg
{
  u16 hex = 0;
  BitField<0, 2, AlphaReadMode, u16> read_mode;
};

// fifo Control Register
union UPECtrlReg
{
  u16 hex = 0;
  BitField<0, 1, bool, u16> pe_token_enable;
  BitField<1, 1, bool, u16> pe_finish_enable;
  BitField<2, 1, bool, u16> pe_token;   // Write only
  BitField<3, 1, bool, u16> pe_finish;  // Write only
};

class PixelEngineManager
{
public:
  void Init(Core::System& system);
  void DoState(PointerWrap& p);

  void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

  // gfx backend support
  void SetToken(Core::System& system, const u16 token, const bool interrupt, int cycle_delay);
  void SetFinish(Core::System& system, int cycle_delay);
  AlphaReadMode GetAlphaReadMode() const { return m_alpha_read.read_mode; }

private:
  void RaiseEvent(Core::System& system, int cycles_into_future);
  void UpdateInterrupts();
  void SetTokenFinish_OnMainThread(Core::System& system, u64 userdata, s64 cycles_late);

  static void SetTokenFinish_OnMainThread_Static(Core::System& system, u64 userdata,
                                                 s64 cycles_late);

  // STATE_TO_SAVE
  UPEZConfReg m_z_conf;
  UPEAlphaConfReg m_alpha_conf;
  UPEDstAlphaConfReg m_dst_alpha_conf;
  UPEAlphaModeConfReg m_alpha_mode_conf;
  UPEAlphaReadReg m_alpha_read;
  UPECtrlReg m_control;

  std::mutex m_token_finish_mutex;
  u16 m_token = 0;
  u16 m_token_pending = 0;
  bool m_token_interrupt_pending = false;
  bool m_finish_interrupt_pending = false;
  bool m_event_raised = false;

  bool m_signal_token_interrupt = false;
  bool m_signal_finish_interrupt = false;

  CoreTiming::EventType* m_event_type_set_token_finish = nullptr;
};

}  // namespace PixelEngine
