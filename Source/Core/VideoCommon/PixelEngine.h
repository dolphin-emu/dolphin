// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
class PointerWrap;
namespace MMIO { class Mapping; }

// internal hardware addresses
enum
{
	PE_ZCONF         = 0x00, // Z Config
	PE_ALPHACONF     = 0x02, // Alpha Config
	PE_DSTALPHACONF  = 0x04, // Destination Alpha Config
	PE_ALPHAMODE     = 0x06, // Alpha Mode Config
	PE_ALPHAREAD     = 0x08, // Alpha Read
	PE_CTRL_REGISTER = 0x0a, // Control
	PE_TOKEN_REG     = 0x0e, // Token
	PE_BBOX_LEFT     = 0x10, // Flip Left
	PE_BBOX_RIGHT    = 0x12, // Flip Right
	PE_BBOX_TOP      = 0x14, // Flip Top
	PE_BBOX_BOTTOM   = 0x16, // Flip Bottom

	// NOTE: Order not verified
	// These indicate the number of quads that are being used as input/output for each particular stage
	PE_PERF_ZCOMP_INPUT_ZCOMPLOC_L  = 0x18,
	PE_PERF_ZCOMP_INPUT_ZCOMPLOC_H  = 0x1a,
	PE_PERF_ZCOMP_OUTPUT_ZCOMPLOC_L = 0x1c,
	PE_PERF_ZCOMP_OUTPUT_ZCOMPLOC_H = 0x1e,
	PE_PERF_ZCOMP_INPUT_L           = 0x20,
	PE_PERF_ZCOMP_INPUT_H           = 0x22,
	PE_PERF_ZCOMP_OUTPUT_L          = 0x24,
	PE_PERF_ZCOMP_OUTPUT_H          = 0x26,
	PE_PERF_BLEND_INPUT_L           = 0x28,
	PE_PERF_BLEND_INPUT_H           = 0x2a,
	PE_PERF_EFB_COPY_CLOCKS_L       = 0x2c,
	PE_PERF_EFB_COPY_CLOCKS_H       = 0x2e,
};

namespace PixelEngine
{

// ReadMode specifies the returned alpha channel for EFB peeks
union UPEAlphaReadReg
{
	u16 Hex;
	struct
	{
		u16 ReadMode : 2;
		u16          : 14;
	};
};

void Init();
void DoState(PointerWrap &p);

void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

// gfx backend support
void SetToken(const u16 _token, const int _bSetTokenAcknowledge);
void SetFinish(void);
UPEAlphaReadReg GetAlphaReadMode();

// Bounding box functionality. Paper Mario (both) are a couple of the few games that use it.
extern u16 bbox[4];
extern bool bbox_active;

} // end of namespace PixelEngine
