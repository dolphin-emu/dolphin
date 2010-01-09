// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
#ifndef _VIDEOINTERFACE_H
#define _VIDEOINTERFACE_H

#include "Common.h"
class PointerWrap;

namespace VideoInterface
{
// NTSC is 60 FPS, right?
// Wrong, it's about 59.94 FPS. The NTSC engineers had to slightly lower
// the field rate from 60 FPS when they added color to the standard.
// This was done to prevent analog interference between the video and
// audio signals. PAL has no similar reduction; it is exactly 50 FPS.
//#define NTSC_FIELD_RATE		(60.0f / 1.001f)
#define NTSC_FIELD_RATE		60
#define NTSC_LINE_COUNT		525
// These line numbers indicate the beginning of the "active video" in a frame.
// An NTSC frame has the lower field first followed by the upper field.
// TODO: Is this true for PAL-M? Is this true for EURGB60?
#define NTSC_LOWER_BEGIN		21
#define NTSC_UPPER_BEGIN		283

//#define PAL_FIELD_RATE		50.0f
#define PAL_FIELD_RATE		50
#define PAL_LINE_COUNT		625
// These line numbers indicate the beginning of the "active video" in a frame.
// A PAL frame has the upper field first followed by the lower field.
#define PAL_UPPER_BEGIN		23
#define PAL_LOWER_BEGIN		336

// VI Internal Hardware Addresses
enum
{
	VI_VERTICAL_TIMING					= 0x00,
	VI_CONTROL_REGISTER	                = 0x02,
	VI_HORIZONTAL_TIMING_0_HI			= 0x04,
	VI_HORIZONTAL_TIMING_0_LO			= 0x06,
	VI_HORIZONTAL_TIMING_1_HI			= 0x08,
	VI_HORIZONTAL_TIMING_1_LO			= 0x0a,
	VI_VBLANK_TIMING_ODD_HI				= 0x0c,
	VI_VBLANK_TIMING_ODD_LO				= 0x0e,
	VI_VBLANK_TIMING_EVEN_HI			= 0x10,
	VI_VBLANK_TIMING_EVEN_LO			= 0x12,
	VI_BURST_BLANKING_ODD_HI			= 0x14,
	VI_BURST_BLANKING_ODD_LO			= 0x16,
	VI_BURST_BLANKING_EVEN_HI			= 0x18,
	VI_BURST_BLANKING_EVEN_LO			= 0x1a,
	VI_FB_LEFT_TOP_HI					= 0x1c, // FB_LEFT_TOP is first half of XFB info
	VI_FB_LEFT_TOP_LO					= 0x1e,
	VI_FB_RIGHT_TOP_HI					= 0x20, // FB_RIGHT_TOP is only used in 3D mode
	VI_FB_RIGHT_TOP_LO					= 0x22,
	VI_FB_LEFT_BOTTOM_HI				= 0x24, // FB_LEFT_BOTTOM is second half of XFB info
	VI_FB_LEFT_BOTTOM_LO				= 0x26,
	VI_FB_RIGHT_BOTTOM_HI				= 0x28, // FB_RIGHT_BOTTOM is only used in 3D mode
	VI_FB_RIGHT_BOTTOM_LO				= 0x2a,
	VI_VERTICAL_BEAM_POSITION			= 0x2c,
	VI_HORIZONTAL_BEAM_POSITION			= 0x2e,
	VI_PRERETRACE_HI					= 0x30,
	VI_PRERETRACE_LO					= 0x32,
	VI_POSTRETRACE_HI					= 0x34,
	VI_POSTRETRACE_LO					= 0x36,
	VI_DISPLAY_INTERRUPT_2_HI			= 0x38,
	VI_DISPLAY_INTERRUPT_2_LO			= 0x3a,
	VI_DISPLAY_INTERRUPT_3_HI			= 0x3c,
	VI_DISPLAY_INTERRUPT_3_LO			= 0x3e,
	VI_DISPLAY_LATCH_0_HI				= 0x40,
	VI_DISPLAY_LATCH_0_LO				= 0x42,
	VI_DISPLAY_LATCH_1_HI				= 0x44,
	VI_DISPLAY_LATCH_1_LO				= 0x46,
	VI_HSCALEW                          = 0x48,
	VI_HSCALER                          = 0x4a,
	VI_FILTER_COEF_0_HI					= 0x4c,
	VI_FILTER_COEF_0_LO					= 0x4e,
	VI_FILTER_COEF_1_HI					= 0x50,
	VI_FILTER_COEF_1_LO					= 0x52,
	VI_FILTER_COEF_2_HI					= 0x54,
	VI_FILTER_COEF_2_LO					= 0x56,
	VI_FILTER_COEF_3_HI					= 0x58,
	VI_FILTER_COEF_3_LO					= 0x5a,
	VI_FILTER_COEF_4_HI					= 0x5c,
	VI_FILTER_COEF_4_LO					= 0x5e,
	VI_FILTER_COEF_5_HI					= 0x60,
	VI_FILTER_COEF_5_LO					= 0x62,
	VI_FILTER_COEF_6_HI					= 0x64,
	VI_FILTER_COEF_6_LO					= 0x66,
	VI_UNK_AA_REG_HI					= 0x68,
	VI_UNK_AA_REG_LO					= 0x6a,
	VI_CLOCK							= 0x6c,
	VI_DTV_STATUS						= 0x6e,
	VI_FBWIDTH							= 0x70,
	VI_BORDER_BLANK_END					= 0x72, // Only used in debug video mode
	VI_BORDER_BLANK_START				= 0x74, // Only used in debug video mode
	//VI_INTERLACE						= 0x850, // ??? MYSTERY OLD CODE
};

union UVIVerticalTimingRegister
{
	u16 Hex;
	struct
	{
		unsigned EQU	:	 4; // Equalization pulse in half lines
		unsigned ACV	:	10; // Active video in lines per field (seems always zero)
		unsigned		:	 2;
	};
	UVIVerticalTimingRegister(u16 _hex) { Hex = _hex;}
	UVIVerticalTimingRegister() { Hex = 0;}
};

union UVIDisplayControlRegister
{
	u16 Hex;
	struct
	{
		unsigned ENB	:	1; // Enables video timing generation and data request
		unsigned RST	:	1; // Clears all data requests and puts VI into its idle state
		unsigned NIN	:	1; // 0: Interlaced, 1: Non-Interlaced: top field drawn at field rate and bottom field is not displayed
		unsigned DLR	:	1; // Selects 3D Display Mode
		unsigned LE0	:	2; // Display Latch; 0: Off, 1: On for 1 field, 2: On for 2 fields, 3: Always on
		unsigned LE1	:	2;
		unsigned FMT	:	2; // 0: NTSC, 1: PAL, 2: MPAL, 3: Debug
		unsigned		:	6;
	};
	UVIDisplayControlRegister(u16 _hex) { Hex = _hex;}
	UVIDisplayControlRegister() { Hex = 0;}
};

union UVIHorizontalTiming0
{
	u32 Hex;
	struct { u16 Lo, Hi; };
	struct  
	{		
		unsigned HLW		:	9; // Halfline Width (W*16 = Width (720))
		unsigned			:	7;
		unsigned HCE		:	7; // Horizontal Sync Start to Color Burst End
		unsigned			:	1;
		unsigned HCS		:	7; // Horizontal Sync Start to Color Burst Start
		unsigned			:	1;
	};
};

union UVIHorizontalTiming1
{
	u32 Hex;
	struct { u16 Lo, Hi; };
	struct  
	{		
		unsigned HSY		:	 7; // Horizontal Sync Width
		unsigned HBE640		:	 9; // Horizontal Sync Start to horizontal blank end
		unsigned			:	 1;
		unsigned HBS640		:	 9; // Half line to horizontal blanking start
		unsigned			:	 6;
	};
};

// Exists for both odd and even fields
union UVIVBlankTimingRegister
{
	u32 Hex;
	struct { u16 Lo, Hi; };
	struct  
	{		
		unsigned PRB		:	10; // Pre-blanking in half lines
		unsigned			:	 6;
		unsigned PSB		:	10; // Post blanking in half lines
		unsigned			:	 6;
	};
};

// Exists for both odd and even fields
union UVIBurstBlankingRegister
{
	u32 Hex;
	struct { u16 Lo, Hi; };
	struct  
	{		
		unsigned BS0		:	 5; // Field x start to burst blanking start in halflines
		unsigned BE0		:	11; // Field x start to burst blanking end in halflines
		unsigned BS2		:	 5; // Field x+2 start to burst blanking start in halflines
		unsigned BE2		:	11; // Field x+2 start to burst blanking end in halflines
	};
};

union UVIFBInfoRegister
{
	u32 Hex;
	struct { u16 Lo, Hi; };
	struct  
	{
		// TODO: mask out lower 9bits/align to 9bits???
		unsigned FBB		:	24; // Base address of the framebuffer in external mem
		// POFF only seems to exist in the top reg. XOFF, unknown.
		unsigned XOFF		:	 4; // Horizontal Offset of the left-most pixel within the first word of the fetched picture
		unsigned POFF		:	 1; // Page offest: 1: fb address is (address>>5)
		unsigned CLRPOFF	:	 3; // ? setting bit 31 clears POFF
	};
};

// VI Interrupt Register
union UVIInterruptRegister
{
	u32 Hex;
	struct { u16 Lo, Hi; };
	struct  
	{		
		unsigned HCT		:	11; // Horizontal Position
		unsigned			:	 5;
		unsigned VCT		:	11; // Vertical Position
		unsigned			:	 1;
		unsigned IR_MASK	:	 1; // Interrupt Mask Bit
		unsigned			:	 2;
		unsigned IR_INT		:	 1; // Interrupt Status (1=Active, 0=Clear)
	};
};

union UVILatchRegister
{
	u32 Hex;
	struct { u16 Lo, Hi; };
	struct  
	{		
		unsigned HCT		:	11; // Horizontal Count
		unsigned			:	 5;
		unsigned VCT		:	11; // Vertical Count
		unsigned			:	 4;
		unsigned TRG		:	 1; // Trigger Flag
	};
};

union UVIHorizontalStepping
{
	u16 Hex;
	struct
	{
		unsigned FbSteps	:	 8;
		unsigned FieldSteps	:	 8;
	};
};

union UVIHorizontalScaling
{
	u16 Hex;
	struct
	{
		unsigned STP		:	9; // Horizontal stepping size (U1.8 Scaler Value) (0x160 Works for 320)
		unsigned			:	3;
		unsigned HS_EN		:	1; // Enable Horizontal Scaling
		unsigned			:	3;
	};
	UVIHorizontalScaling(u16 _hex) { Hex = _hex;}
	UVIHorizontalScaling() { Hex = 0;}
};

// Used for tables 0-2
union UVIFilterCoefTable3
{
	u32 Hex;
	struct { u16 Lo, Hi; };
	struct
	{
		unsigned Tap0		:	10;
		unsigned Tap1		:	10;
		unsigned Tap2		:	10;
		unsigned			:	 2;
	};
};

// Used for tables 3-6
union UVIFilterCoefTable4
{
	u32 Hex;
	struct { u16 Lo, Hi; };
	struct
	{
		unsigned Tap0		:	 8;
		unsigned Tap1		:	 8;
		unsigned Tap2		:	 8;
		unsigned Tap3		:	 8;
	};
};

struct SVIFilterCoefTables
{
	UVIFilterCoefTable3 Tables02[3];
	UVIFilterCoefTable4 Tables36[4];
};

// Debug video mode only, probably never used in dolphin...
union UVIBorderBlankRegister
{
	u32 Hex;
	struct { u16 Lo, Hi; };
	struct
	{
		unsigned HBE656		:	10; // Border Horizontal Blank End
		unsigned			:	11;
		unsigned HBS656		:	10; // Border Horizontal Blank start
		unsigned BRDR_EN	:	 1; // Border Enable
	};
};

	// urgh, ugly externs.
	extern u32 TargetRefreshRate;

	// For BS2 HLE
	void Preset(bool _bNTSC);
	void SetRegionReg(char _region);

	void Init();	
	void DoState(PointerWrap &p);

	void Read8(u8& _uReturnValue, const u32 _uAddress);
	void Read16(u16& _uReturnValue, const u32 _uAddress);
	void Read32(u32& _uReturnValue, const u32 _uAddress);
				
	void Write16(const u16 _uValue, const u32 _uAddress);
	void Write32(const u32 _uValue, const u32 _uAddress);	

	// returns a pointer to the current visible xfb
	u8* GetXFBPointerTop();
	u8* GetXFBPointerBottom();

    // Update and draw framebuffer
    void Update();

	// UpdateInterrupts: check if we have to generate a new VI Interrupt
	void UpdateInterrupts();

    // Change values pertaining to video mode
    void UpdateParameters();

	int GetTicksPerLine();
	int GetTicksPerFrame();
};

#endif // _VIDEOINTERFACE_H
