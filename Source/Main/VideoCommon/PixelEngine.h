// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
class PointerWrap;
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
union UPEAlphaReadReg
{
  u16 Hex;
  struct
  {
    u16 ReadMode : 2;
    u16 : 14;
  };
};

void Init();
void DoState(PointerWrap& p);

void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

// gfx backend support
void SetToken(const u16 token, const bool interrupt);
void SetFinish();
UPEAlphaReadReg GetAlphaReadMode();

}  // end of namespace PixelEngine
