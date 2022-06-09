// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/BitFieldView.h"
#include "Common/CommonTypes.h"

class PointerWrap;
namespace MMIO
{
class Mapping;
}

namespace VideoInterface
{
// VI Internal Hardware Addresses
enum
{
  VI_VERTICAL_TIMING = 0x00,
  VI_CONTROL_REGISTER = 0x02,
  VI_HORIZONTAL_TIMING_0_HI = 0x04,
  VI_HORIZONTAL_TIMING_0_LO = 0x06,
  VI_HORIZONTAL_TIMING_1_HI = 0x08,
  VI_HORIZONTAL_TIMING_1_LO = 0x0a,
  VI_VBLANK_TIMING_ODD_HI = 0x0c,
  VI_VBLANK_TIMING_ODD_LO = 0x0e,
  VI_VBLANK_TIMING_EVEN_HI = 0x10,
  VI_VBLANK_TIMING_EVEN_LO = 0x12,
  VI_BURST_BLANKING_ODD_HI = 0x14,
  VI_BURST_BLANKING_ODD_LO = 0x16,
  VI_BURST_BLANKING_EVEN_HI = 0x18,
  VI_BURST_BLANKING_EVEN_LO = 0x1a,
  VI_FB_LEFT_TOP_HI = 0x1c,  // FB_LEFT_TOP is first half of XFB info
  VI_FB_LEFT_TOP_LO = 0x1e,
  VI_FB_RIGHT_TOP_HI = 0x20,  // FB_RIGHT_TOP is only used in 3D mode
  VI_FB_RIGHT_TOP_LO = 0x22,
  VI_FB_LEFT_BOTTOM_HI = 0x24,  // FB_LEFT_BOTTOM is second half of XFB info
  VI_FB_LEFT_BOTTOM_LO = 0x26,
  VI_FB_RIGHT_BOTTOM_HI = 0x28,  // FB_RIGHT_BOTTOM is only used in 3D mode
  VI_FB_RIGHT_BOTTOM_LO = 0x2a,
  VI_VERTICAL_BEAM_POSITION = 0x2c,
  VI_HORIZONTAL_BEAM_POSITION = 0x2e,
  VI_PRERETRACE_HI = 0x30,
  VI_PRERETRACE_LO = 0x32,
  VI_POSTRETRACE_HI = 0x34,
  VI_POSTRETRACE_LO = 0x36,
  VI_DISPLAY_INTERRUPT_2_HI = 0x38,
  VI_DISPLAY_INTERRUPT_2_LO = 0x3a,
  VI_DISPLAY_INTERRUPT_3_HI = 0x3c,
  VI_DISPLAY_INTERRUPT_3_LO = 0x3e,
  VI_DISPLAY_LATCH_0_HI = 0x40,
  VI_DISPLAY_LATCH_0_LO = 0x42,
  VI_DISPLAY_LATCH_1_HI = 0x44,
  VI_DISPLAY_LATCH_1_LO = 0x46,
  VI_HSCALEW = 0x48,
  VI_HSCALER = 0x4a,
  VI_FILTER_COEF_0_HI = 0x4c,
  VI_FILTER_COEF_0_LO = 0x4e,
  VI_FILTER_COEF_1_HI = 0x50,
  VI_FILTER_COEF_1_LO = 0x52,
  VI_FILTER_COEF_2_HI = 0x54,
  VI_FILTER_COEF_2_LO = 0x56,
  VI_FILTER_COEF_3_HI = 0x58,
  VI_FILTER_COEF_3_LO = 0x5a,
  VI_FILTER_COEF_4_HI = 0x5c,
  VI_FILTER_COEF_4_LO = 0x5e,
  VI_FILTER_COEF_5_HI = 0x60,
  VI_FILTER_COEF_5_LO = 0x62,
  VI_FILTER_COEF_6_HI = 0x64,
  VI_FILTER_COEF_6_LO = 0x66,
  VI_UNK_AA_REG_HI = 0x68,
  VI_UNK_AA_REG_LO = 0x6a,
  VI_CLOCK = 0x6c,
  VI_DTV_STATUS = 0x6e,
  VI_FBWIDTH = 0x70,
  VI_BORDER_BLANK_END = 0x72,    // Only used in debug video mode
  VI_BORDER_BLANK_START = 0x74,  // Only used in debug video mode
  // VI_INTERLACE                      = 0x850, // ??? MYSTERY OLD CODE
};

struct VIVerticalTimingRegister
{
  u16 Hex = 0;
  BFVIEW_M(Hex, u16, 0, 4, EQU);   // Equalization pulse in half lines
  BFVIEW_M(Hex, u16, 4, 10, ACV);  // Active video in lines per field (seems always zero)

  VIVerticalTimingRegister() = default;
  explicit VIVerticalTimingRegister(u16 hex) : Hex{hex} {}
};

struct VIDisplayControlRegister
{
  u16 Hex = 0;

  BFVIEW_M(Hex, bool, 0, 1, ENB);  // Enables video timing generation and data request
  BFVIEW_M(Hex, bool, 1, 1, RST);  // Clears all data requests and puts VI into its idle state
  BFVIEW_M(Hex, bool, 2, 1, NIN);  // 0: Interlaced, 1: Non-Interlaced: top field drawn at
                                   // field rate and bottom field is not displayed
  BFVIEW_M(Hex, bool, 3, 1, DLR);  // Selects 3D Display Mode
  BFVIEW_M(Hex, u16, 4, 2, LE0);   // Display Latch
                                   // 0: Off, 1: On for 1 field, 2: On for 2 fields,
                                   // 3: Always on
  BFVIEW_M(Hex, u16, 6, 2, LE1);
  BFVIEW_M(Hex, u16, 8, 2, FMT);  // 0: NTSC, 1: PAL, 2: MPAL, 3: Debug

  VIDisplayControlRegister() = default;
  explicit VIDisplayControlRegister(u16 hex) : Hex{hex} {}
};

union UVIHorizontalTiming0
{
  u32 Hex;
  struct
  {
    u16 Lo, Hi;
  };

  BFVIEW_M(Hex, u32, 0, 10, HLW);  // Halfline Width (W*16 = Width (720))
  BFVIEW_M(Hex, u32, 16, 7, HCE);  // Horizontal Sync Start to Color Burst End
  BFVIEW_M(Hex, u32, 24, 7, HCS);  // Horizontal Sync Start to Color Burst Start
};

union UVIHorizontalTiming1
{
  u32 Hex;
  struct
  {
    u16 Lo, Hi;
  };

  BFVIEW_M(Hex, u32, 0, 7, HSY);       // Horizontal Sync Width
  BFVIEW_M(Hex, u32, 7, 10, HBE640);   // Horizontal Sync Start to horizontal blank end
  BFVIEW_M(Hex, u32, 17, 10, HBS640);  // Half line to horizontal blanking start
};

// Exists for both odd and even fields
union UVIVBlankTimingRegister
{
  u32 Hex;
  struct
  {
    u16 Lo, Hi;
  };

  BFVIEW_M(Hex, u32, 0, 10, PRB);   // Pre-blanking in half lines
  BFVIEW_M(Hex, u32, 16, 10, PSB);  // Post blanking in half lines
};

// Exists for both odd and even fields
union UVIBurstBlankingRegister
{
  u32 Hex;
  struct
  {
    u16 Lo, Hi;
  };

  BFVIEW_M(Hex, u32, 0, 5, BS0);    // Field x start to burst blanking start in halflines
  BFVIEW_M(Hex, u32, 5, 11, BE0);   // Field x start to burst blanking end in halflines
  BFVIEW_M(Hex, u32, 16, 5, BS2);   // Field x+2 start to burst blanking start in halflines
  BFVIEW_M(Hex, u32, 21, 11, BE2);  // Field x+2 start to burst blanking end in halflines
};

union UVIFBInfoRegister
{
  u32 Hex;
  struct
  {
    u16 Lo, Hi;
  };

  // TODO: mask out lower 9bits/align to 9bits???
  BFVIEW_M(Hex, u32, 0, 24, FBB);  // Base address of the framebuffer in external mem
  // POFF only seems to exist in the top reg. XOFF, unknown.
  BFVIEW_M(Hex, u32, 24, 4, XOFF);     // Horizontal Offset of the left-most pixel within the
                                       // first word of the fetched picture
  BFVIEW_M(Hex, bool, 28, 1, POFF);    // Page offest: 1: fb address is (address>>5)
  BFVIEW_M(Hex, u32, 29, 3, CLRPOFF);  // ? setting bit 31 clears POFF
};

// VI Interrupt Register
union UVIInterruptRegister
{
  u32 Hex;
  struct
  {
    u16 Lo, Hi;
  };

  BFVIEW_M(Hex, u32, 0, 11, HCT);       // Horizontal Position
  BFVIEW_M(Hex, u32, 16, 11, VCT);      // Vertical Position
  BFVIEW_M(Hex, bool, 28, 1, IR_MASK);  // Interrupt Mask Bit
  BFVIEW_M(Hex, bool, 31, 1, IR_INT);   // Interrupt Status (1=Active, 0=Clear)
};

union UVILatchRegister
{
  u32 Hex;
  struct
  {
    u16 Lo, Hi;
  };

  BFVIEW_M(Hex, u32, 0, 11, HCT);   // Horizontal Count
  BFVIEW_M(Hex, u32, 16, 11, VCT);  // Vertical Count
  BFVIEW_M(Hex, bool, 31, 1, TRG);  // Trigger Flag
};

struct PictureConfigurationRegister
{
  u16 Hex;

  BFVIEW_M(Hex, u16, 0, 8, STD);
  BFVIEW_M(Hex, u16, 8, 7, WPL);
};

struct VIHorizontalScaling
{
  u16 Hex = 0;

  BFVIEW_M(Hex, u16, 0, 9, STP);      // Horizontal stepping size (U1.8 Scaler Value)
                                      // (0x160 Works for 320)
  BFVIEW_M(Hex, bool, 12, 1, HS_EN);  // Enable Horizontal Scaling

  VIHorizontalScaling() = default;
  explicit VIHorizontalScaling(u16 hex) : Hex{hex} {}
};

// Used for tables 0-2
union UVIFilterCoefTable3
{
  u32 Hex;
  struct
  {
    u16 Lo, Hi;
  };

  BFVIEW_M(Hex, u32, 0, 10, Tap0);
  BFVIEW_M(Hex, u32, 10, 10, Tap1);
  BFVIEW_M(Hex, u32, 20, 10, Tap2);
};

// Used for tables 3-6
union UVIFilterCoefTable4
{
  u32 Hex;
  struct
  {
    u16 Lo, Hi;
  };

  BFVIEW_M(Hex, u8, 0, 8, Tap0);
  BFVIEW_M(Hex, u8, 8, 8, Tap1);
  BFVIEW_M(Hex, u8, 16, 8, Tap2);
  BFVIEW_M(Hex, u8, 24, 8, Tap3);
};

struct SVIFilterCoefTables
{
  UVIFilterCoefTable3 Tables02[3];
  UVIFilterCoefTable4 Tables36[4];
};

// Debug video mode only, probably never used in Dolphin...
union UVIBorderBlankRegister
{
  u32 Hex;
  struct
  {
    u16 Lo, Hi;
  };

  BFVIEW_M(Hex, u32, 0, 10, HBE656);    // Border Horizontal Blank End
  BFVIEW_M(Hex, u32, 21, 10, HBS656);   // Border Horizontal Blank start
  BFVIEW_M(Hex, bool, 31, 1, BRDR_EN);  // Border Enable
};

// ntsc-j and component cable bits
struct VIDTVStatus
{
  u16 Hex;

  BFVIEW_M(Hex, bool, 0, 1, component_plugged);
  BFVIEW_M(Hex, bool, 1, 1, ntsc_j);
};

struct VIHorizontalStepping
{
  u16 Hex;

  BFVIEW_M(Hex, u16, 0, 10, srcwidth);
};

// For BS2 HLE
void Preset(bool _bNTSC);

void Init();
void DoState(PointerWrap& p);

void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

// returns a pointer to the current visible xfb
u32 GetXFBAddressTop();
u32 GetXFBAddressBottom();

// Update and draw framebuffer
void Update(u64 ticks);

// UpdateInterrupts: check if we have to generate a new VI Interrupt
void UpdateInterrupts();

// Change values pertaining to video mode
void UpdateParameters();

double GetTargetRefreshRate();
u32 GetTargetRefreshRateNumerator();
u32 GetTargetRefreshRateDenominator();

u32 GetTicksPerSample();
u32 GetTicksPerHalfLine();
u32 GetTicksPerField();

// Get the aspect ratio of VI's active area.
// This function only deals with standard aspect ratios. For widescreen aspect ratios, multiply the
// result by 1.33333..
float GetAspectRatio();

// Create a fake VI mode for a fifolog
void FakeVIUpdate(u32 xfb_address, u32 fb_width, u32 fb_stride, u32 fb_height);

}  // namespace VideoInterface
