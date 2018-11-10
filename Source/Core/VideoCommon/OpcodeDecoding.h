// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/DataReader.h"

class DataReader;

namespace OpcodeDecoder
{
enum
{
  GX_NOP = 0x00,
  GX_UNKNOWN_RESET = 0x01,

  GX_LOAD_BP_REG = 0x61,
  GX_LOAD_CP_REG = 0x08,
  GX_LOAD_XF_REG = 0x10,
  GX_LOAD_INDX_A = 0x20,
  GX_LOAD_INDX_B = 0x28,
  GX_LOAD_INDX_C = 0x30,
  GX_LOAD_INDX_D = 0x38,

  GX_CMD_CALL_DL = 0x40,
  GX_CMD_UNKNOWN_METRICS = 0x44,
  GX_CMD_INVL_VC = 0x48
};

enum
{
  GX_PRIMITIVE_MASK = 0x78,
  GX_PRIMITIVE_SHIFT = 3,
  GX_VAT_MASK = 0x07
};

// These values are the values extracted using GX_PRIMITIVE_MASK
// and GX_PRIMITIVE_SHIFT.
// GX_DRAW_QUADS_2 behaves the same way as GX_DRAW_QUADS.
enum
{
  GX_DRAW_QUADS = 0x0,           // 0x80
  GX_DRAW_QUADS_2 = 0x1,         // 0x88
  GX_DRAW_TRIANGLES = 0x2,       // 0x90
  GX_DRAW_TRIANGLE_STRIP = 0x3,  // 0x98
  GX_DRAW_TRIANGLE_FAN = 0x4,    // 0xA0
  GX_DRAW_LINES = 0x5,           // 0xA8
  GX_DRAW_LINE_STRIP = 0x6,      // 0xB0
  GX_DRAW_POINTS = 0x7           // 0xB8
};

void Init();

template <bool is_preprocess = false>
u8* Run(DataReader src, u32* cycles, bool in_display_list, u32* need_size = nullptr);

}  // namespace OpcodeDecoder
