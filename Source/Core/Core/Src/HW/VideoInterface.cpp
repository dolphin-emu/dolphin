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

#include "Common.h"
#include "ChunkFile.h"

#include "../PowerPC/PowerPC.h"

#include "../Core.h"			// <- for Core::GetStartupParameter().bUseDualCore
#include "CommandProcessor.h"	// <- for homebrew's XFB draw hack
#include "PeripheralInterface.h"
#include "VideoInterface.h"
#include "Memmap.h"
#include "../PluginManager.h"
#include "../CoreTiming.h"
#include "../HW/SystemTimers.h"
#include "StringUtil.h"
#include "Timer.h"

namespace VideoInterface
{
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
	VI_FB_LEFT_BOTTOM_HI				= 0x24, // FB_LEFT_TOP is second half of XFB info
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
	VI_INTERLACE						= 0x850, // ??? MYSTERY OLD CODE
};

union UVIVerticalTimingRegister
{
	u16 Hex;
	struct
	{
		unsigned EQU	:	 4; // Equalization pulse in half lines
		unsigned ACV	:	10; // Active video in lines per field (TODO: Does it indicate half-lines in progressive mode or 54 MHz mode?)
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
		unsigned IR_MASK	:	 1; // Interrupt Enable Bit
		unsigned			:	 2;
		unsigned IR_INT		:	 1; // Interrupt Status (1=Active) (Write to clear)
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


// STATE_TO_SAVE
// Registers listed in order:
static UVIVerticalTimingRegister	m_VerticalTimingRegister;
static UVIDisplayControlRegister	m_DisplayControlRegister;
static UVIHorizontalTiming0			m_HTiming0;
static UVIHorizontalTiming1			m_HTiming1;
static UVIVBlankTimingRegister		m_VBlankTimingOdd;
static UVIVBlankTimingRegister		m_VBlankTimingEven;
static UVIBurstBlankingRegister		m_BurstBlankingOdd;
static UVIBurstBlankingRegister		m_BurstBlankingEven;
static UVIFBInfoRegister			m_XFBInfoTop;
static UVIFBInfoRegister			m_XFBInfoBottom;
static UVIFBInfoRegister			m_3DFBInfoTop;		// Start making your stereoscopic demos! :p
static UVIFBInfoRegister			m_3DFBInfoBottom;
static u16							m_VBeamPos = 0;
static u16							m_HBeamPos = 0;
static UVIInterruptRegister			m_InterruptRegister[4];
static UVILatchRegister				m_LatchRegister[2];
static UVIHorizontalStepping		m_HorizontalStepping;
static UVIHorizontalScaling			m_HorizontalScaling;
static SVIFilterCoefTables			m_FilterCoefTables;
static u32							m_UnkAARegister = 0;// ??? 0x00FF0000
static u16							m_Clock = 0;		// 0: 27MHz, 1: 54MHz
static u16							m_DTVStatus = 0;	// Region char and component cable bit (only low 2bits are used?)
static u16							m_FBWidth = 0;		// Only correct when scaling is enabled?
static UVIBorderBlankRegister		m_BorderHBlank;
// 0xcc002076 - 0xcc00207f is full of 0x00FF: unknown
// 0xcc002080 - 0xcc002100 even more unknown


static u32 TicksPerFrame = 0;
static u32 s_lineCount = 0;
static u32 s_upperFieldBegin = 0;
static u32 s_lowerFieldBegin = 0;

float TargetRefreshRate = 0.0;
float ActualRefreshRate = 0.0;
s64 SyncTicksProgress = 0;

void DoState(PointerWrap &p)
{
	p.Do(m_VerticalTimingRegister);
	p.Do(m_DisplayControlRegister);
	p.Do(m_HTiming0);
	p.Do(m_HTiming1);
	p.Do(m_VBlankTimingOdd);
	p.Do(m_VBlankTimingEven);
	p.Do(m_BurstBlankingOdd);
	p.Do(m_BurstBlankingEven);
	p.Do(m_XFBInfoTop);
	p.Do(m_XFBInfoBottom);
	p.Do(m_3DFBInfoTop);
	p.Do(m_3DFBInfoBottom);
	p.Do(m_VBeamPos);
	p.Do(m_HBeamPos);
	p.DoArray(m_InterruptRegister, 4);
	p.DoArray(m_LatchRegister, 2);
	p.Do(m_HorizontalStepping);
	p.Do(m_HorizontalScaling);
	p.Do(m_FilterCoefTables);
	p.Do(m_UnkAARegister);
	p.Do(m_Clock);
	p.Do(m_DTVStatus);
	p.Do(m_FBWidth);
	p.Do(m_BorderHBlank);

	p.Do(TicksPerFrame);
	p.Do(s_lineCount);
	p.Do(s_upperFieldBegin);
	p.Do(s_lowerFieldBegin);
}

void PreInit(bool _bNTSC)
{
	TicksPerFrame = 0;
	s_lineCount = 0;
	s_upperFieldBegin = 0;
	s_lowerFieldBegin = 0;

	m_VerticalTimingRegister.EQU = 6;

	m_DisplayControlRegister.ENB = 1;
	m_DisplayControlRegister.FMT = _bNTSC ? 0 : 1;

	m_HTiming0.HLW = 429;
	m_HTiming0.HCE = 105;
	m_HTiming0.HCS = 71;
	m_HTiming1.HSY = 64;
	m_HTiming1.HBE640 = 162;
	m_HTiming1.HBS640 = 373;

	m_VBlankTimingOdd.PRB = 502;
	m_VBlankTimingOdd.PSB = 5;
	m_VBlankTimingEven.PRB = 503;
	m_VBlankTimingEven.PSB = 4;

	m_BurstBlankingOdd.BS0 = 12;
	m_BurstBlankingOdd.BE0 = 520;
	m_BurstBlankingOdd.BS2 = 12;
	m_BurstBlankingOdd.BE2 = 520;
	m_BurstBlankingEven.BS0 = 13;
	m_BurstBlankingEven.BE0 = 519;
	m_BurstBlankingEven.BS2 = 13;
	m_BurstBlankingEven.BE2 = 519;

	m_InterruptRegister[0].HCT = 430;
	m_InterruptRegister[0].VCT = 263;
	m_InterruptRegister[0].IR_MASK = 1;
	m_InterruptRegister[0].IR_INT = 0;
	m_InterruptRegister[1].HCT = 1;
	m_InterruptRegister[1].VCT = 1;
	m_InterruptRegister[1].IR_MASK = 1;
	m_InterruptRegister[1].IR_INT = 0;

	m_HorizontalStepping.FbSteps = 40;
	m_HorizontalStepping.FieldSteps = 40;

	m_Clock = 0;

	// Say component cable is plugged
	m_DTVStatus = 1;

	UpdateTiming();
}

void SetRegionReg(char _region)
{
	m_DTVStatus = _region | (m_DTVStatus & 1);
}

void Init()
{
	for (int i = 0; i < 4; i++)
		m_InterruptRegister[i].Hex = 0;

	m_DisplayControlRegister.Hex = 0;

	s_lineCount = 0;
	s_upperFieldBegin = 0;
	s_lowerFieldBegin = 0;
}

void Read8(u8& _uReturnValue, const u32 _iAddress)
{
	// Just like 32bit VI transfers, this is technically not allowed,
	// but the hardware accepts it. Action Replay uses this.
	u16 val = 0;

	if (_iAddress % 2 == 0)
	{
		Read16(val, _iAddress);
		_uReturnValue = (u8)(val >> 8);
	}
	else
	{
		Read16(val, _iAddress - 1);
		_uReturnValue = (u8)val;
	}

	INFO_LOG(VIDEOINTERFACE, "(r 8): 0x%02x, 0x%08x", _uReturnValue, _iAddress);
}

void Read16(u16& _uReturnValue, const u32 _iAddress)
{
	switch (_iAddress & 0xFFF)
	{
	case VI_VERTICAL_TIMING:
		_uReturnValue = m_VerticalTimingRegister.Hex;
		return;

	case VI_CONTROL_REGISTER:
		_uReturnValue = m_DisplayControlRegister.Hex;
		return;

	case VI_HORIZONTAL_TIMING_0_HI:
		_uReturnValue = m_HTiming0.Hi;
		break;
	case VI_HORIZONTAL_TIMING_0_LO:
		_uReturnValue = m_HTiming0.Lo;
		break;

	case VI_HORIZONTAL_TIMING_1_HI:
		_uReturnValue = m_HTiming1.Hi;
		break;
	case VI_HORIZONTAL_TIMING_1_LO:
		_uReturnValue = m_HTiming1.Lo;
		break;

	case VI_VBLANK_TIMING_ODD_HI:
		_uReturnValue = m_VBlankTimingOdd.Hi;
		break;
	case VI_VBLANK_TIMING_ODD_LO:
		_uReturnValue = m_VBlankTimingOdd.Lo;
		break;

	case VI_VBLANK_TIMING_EVEN_HI:
		_uReturnValue = m_VBlankTimingEven.Hi;
		break;
	case VI_VBLANK_TIMING_EVEN_LO:
		_uReturnValue = m_VBlankTimingEven.Lo;
		break;

	case VI_BURST_BLANKING_ODD_HI:
		_uReturnValue = m_BurstBlankingOdd.Hi;
		break;
	case VI_BURST_BLANKING_ODD_LO:
		_uReturnValue = m_BurstBlankingOdd.Lo;
		break;

	case VI_BURST_BLANKING_EVEN_HI:
		_uReturnValue = m_BurstBlankingEven.Hi;
		break;
	case VI_BURST_BLANKING_EVEN_LO:
		_uReturnValue = m_BurstBlankingEven.Lo;
		break;

	case VI_FB_LEFT_TOP_HI:
		_uReturnValue = m_XFBInfoTop.Hi;
		break;
	case VI_FB_LEFT_TOP_LO:
		_uReturnValue = m_XFBInfoTop.Lo;
		break;

	case VI_FB_RIGHT_TOP_HI:
		_uReturnValue = m_3DFBInfoTop.Hi;
		break;
	case VI_FB_RIGHT_TOP_LO:
		_uReturnValue = m_3DFBInfoTop.Lo;
		break;

	case VI_FB_LEFT_BOTTOM_HI:
		_uReturnValue = m_XFBInfoBottom.Hi;
		break;
	case VI_FB_LEFT_BOTTOM_LO:
		_uReturnValue = m_XFBInfoBottom.Lo;
		break;

	case VI_FB_RIGHT_BOTTOM_HI:
		_uReturnValue = m_3DFBInfoBottom.Hi;
		break;
	case VI_FB_RIGHT_BOTTOM_LO:
		_uReturnValue = m_3DFBInfoBottom.Lo;
		break;

	case VI_VERTICAL_BEAM_POSITION:
    	_uReturnValue = m_VBeamPos;
		return;

	case VI_HORIZONTAL_BEAM_POSITION:
        _uReturnValue = m_HBeamPos;
		return;

	// RETRACE STUFF ...
	case VI_PRERETRACE_HI:
		_uReturnValue =	m_InterruptRegister[0].Hi;
		return;
	case VI_PRERETRACE_LO:
		_uReturnValue =	m_InterruptRegister[0].Lo;
		return;

	case VI_POSTRETRACE_HI:
		_uReturnValue =	m_InterruptRegister[1].Hi;
		return;
	case VI_POSTRETRACE_LO:
		_uReturnValue =	m_InterruptRegister[1].Lo;
		return;

	case VI_DISPLAY_INTERRUPT_2_HI:
		_uReturnValue =	m_InterruptRegister[2].Hi;
		return;
	case VI_DISPLAY_INTERRUPT_2_LO:
		_uReturnValue =	m_InterruptRegister[2].Lo;
		return;

	case VI_DISPLAY_INTERRUPT_3_HI:
		_uReturnValue =	m_InterruptRegister[3].Hi;
		return;
	case VI_DISPLAY_INTERRUPT_3_LO:
		_uReturnValue =	m_InterruptRegister[3].Lo;
		return;

	case VI_DISPLAY_LATCH_0_HI:
		_uReturnValue = m_LatchRegister[0].Hi;
		break;
	case VI_DISPLAY_LATCH_0_LO:
		_uReturnValue = m_LatchRegister[0].Lo;
		break;

	case VI_DISPLAY_LATCH_1_HI:
		_uReturnValue = m_LatchRegister[1].Hi;
		break;
	case VI_DISPLAY_LATCH_1_LO:
		_uReturnValue = m_LatchRegister[1].Lo;
		break;

	case VI_HSCALEW:
		_uReturnValue = m_HorizontalStepping.Hex;
		break;

	case VI_HSCALER:
		_uReturnValue = m_HorizontalScaling.Hex;
		break;

	case VI_FILTER_COEF_0_HI:
		_uReturnValue = m_FilterCoefTables.Tables02[0].Hi;
		break;
	case VI_FILTER_COEF_0_LO:
		_uReturnValue = m_FilterCoefTables.Tables02[0].Lo;
		break;
	case VI_FILTER_COEF_1_HI:
		_uReturnValue = m_FilterCoefTables.Tables02[1].Hi;
		break;
	case VI_FILTER_COEF_1_LO:
		_uReturnValue = m_FilterCoefTables.Tables02[1].Lo;
		break;
	case VI_FILTER_COEF_2_HI:
		_uReturnValue = m_FilterCoefTables.Tables02[2].Hi;
		break;
	case VI_FILTER_COEF_2_LO:
		_uReturnValue = m_FilterCoefTables.Tables02[2].Lo;
		break;
	case VI_FILTER_COEF_3_HI:
		_uReturnValue = m_FilterCoefTables.Tables36[0].Hi;
		break;
	case VI_FILTER_COEF_3_LO:
		_uReturnValue = m_FilterCoefTables.Tables36[0].Lo;
		break;
	case VI_FILTER_COEF_4_HI:
		_uReturnValue = m_FilterCoefTables.Tables36[1].Hi;
		break;
	case VI_FILTER_COEF_4_LO:
		_uReturnValue = m_FilterCoefTables.Tables36[1].Lo;
		break;
	case VI_FILTER_COEF_5_HI:
		_uReturnValue = m_FilterCoefTables.Tables36[2].Hi;
		break;
	case VI_FILTER_COEF_5_LO:
		_uReturnValue = m_FilterCoefTables.Tables36[2].Lo;
		break;
	case VI_FILTER_COEF_6_HI:
		_uReturnValue = m_FilterCoefTables.Tables36[3].Hi;
		break;
	case VI_FILTER_COEF_6_LO:
		_uReturnValue = m_FilterCoefTables.Tables36[3].Lo;
		break;

	case VI_UNK_AA_REG_HI:
		_uReturnValue = (m_UnkAARegister & 0xffff0000) >> 16;
		WARN_LOG(VIDEOINTERFACE, "(r16) unknown AA reg, not sure what it does :)");
		break;
	case VI_UNK_AA_REG_LO:
		_uReturnValue = m_UnkAARegister & 0x0000ffff;
		WARN_LOG(VIDEOINTERFACE, "(r16) unknown AA reg, not sure what it does :)");
		break;

	case VI_CLOCK:
		_uReturnValue = m_Clock;
		break;

	case VI_DTV_STATUS:
		_uReturnValue = m_DTVStatus;
		break;

	case VI_FBWIDTH:
		_uReturnValue = m_FBWidth;
		break;

	case VI_BORDER_BLANK_END:
		_uReturnValue = m_BorderHBlank.Lo;
		break;
	case VI_BORDER_BLANK_START:
		_uReturnValue = m_BorderHBlank.Hi;
		break;

	default:
		ERROR_LOG(VIDEOINTERFACE, "(r16) unk reg %x", _iAddress & 0xfff);
		_uReturnValue = 0x0;
		break;
	}

	DEBUG_LOG(VIDEOINTERFACE, "(r16): 0x%04x, 0x%08x", _uReturnValue, _iAddress);
}

void Write16(const u16 _iValue, const u32 _iAddress)
{
	DEBUG_LOG(VIDEOINTERFACE, "(w16): 0x%04x, 0x%08x",_iValue,_iAddress);

	//Somewhere it sets screen width.. we need to communicate this to the gfx plugin...

	switch (_iAddress & 0xFFF)
	{
	case VI_VERTICAL_TIMING:
		m_VerticalTimingRegister.Hex = _iValue;
		break;

	case VI_CONTROL_REGISTER:
		{
			UVIDisplayControlRegister tmpConfig(_iValue);
			m_DisplayControlRegister.ENB = tmpConfig.ENB;
			m_DisplayControlRegister.NIN = tmpConfig.NIN;
			m_DisplayControlRegister.DLR = tmpConfig.DLR;
			m_DisplayControlRegister.LE0 = tmpConfig.LE0;
			m_DisplayControlRegister.LE1 = tmpConfig.LE1;
			m_DisplayControlRegister.FMT = tmpConfig.FMT;

			if (tmpConfig.RST)
			{
				m_DisplayControlRegister.RST = 0;
			}

            UpdateTiming();
		}		
		break;

	case VI_HORIZONTAL_TIMING_0_HI:
		m_HTiming0.Hi = _iValue;
		break;
	case VI_HORIZONTAL_TIMING_0_LO:
		m_HTiming0.Lo = _iValue;
		break;

	case VI_HORIZONTAL_TIMING_1_HI:
		m_HTiming1.Hi = _iValue;
		break;
	case VI_HORIZONTAL_TIMING_1_LO:
		m_HTiming1.Lo = _iValue;
		break;

	case VI_VBLANK_TIMING_ODD_HI:
		m_VBlankTimingOdd.Hi = _iValue;
		break;
	case VI_VBLANK_TIMING_ODD_LO:
		m_VBlankTimingOdd.Lo = _iValue;
		break;

	case VI_VBLANK_TIMING_EVEN_HI:
		m_VBlankTimingEven.Hi = _iValue;
		break;
	case VI_VBLANK_TIMING_EVEN_LO:
		m_VBlankTimingEven.Lo = _iValue;
		break;

	case VI_BURST_BLANKING_ODD_HI:
		m_BurstBlankingOdd.Hi = _iValue;
		break;
	case VI_BURST_BLANKING_ODD_LO:
		m_BurstBlankingOdd.Lo = _iValue;
		break;

	case VI_BURST_BLANKING_EVEN_HI:
		m_BurstBlankingEven.Hi = _iValue;
		break;
	case VI_BURST_BLANKING_EVEN_LO:
		m_BurstBlankingEven.Lo = _iValue;
		break;

	case VI_FB_LEFT_TOP_HI:
		m_XFBInfoTop.Hi = _iValue;
		if (m_XFBInfoTop.CLRPOFF) m_XFBInfoTop.POFF = 0;
		break;
	case VI_FB_LEFT_TOP_LO:
		m_XFBInfoTop.Lo = _iValue;
		break;

	case VI_FB_RIGHT_TOP_HI:
		m_3DFBInfoTop.Hi = _iValue;
		if (m_3DFBInfoTop.CLRPOFF) m_3DFBInfoTop.POFF = 0;
		break;
	case VI_FB_RIGHT_TOP_LO:
		m_3DFBInfoTop.Lo = _iValue;
		break;

	case VI_FB_LEFT_BOTTOM_HI:
		m_XFBInfoBottom.Hi = _iValue;
		if (m_XFBInfoBottom.CLRPOFF) m_XFBInfoBottom.POFF = 0;
		break;
	case VI_FB_LEFT_BOTTOM_LO:
		m_XFBInfoBottom.Lo = _iValue;
		break;

	case VI_FB_RIGHT_BOTTOM_HI:
		m_3DFBInfoBottom.Hi = _iValue;
		if (m_3DFBInfoBottom.CLRPOFF) m_3DFBInfoBottom.POFF = 0;
		break;
	case VI_FB_RIGHT_BOTTOM_LO:
		m_3DFBInfoBottom.Lo = _iValue;
		break;

	case VI_VERTICAL_BEAM_POSITION:
		WARN_LOG(VIDEOINTERFACE, "Change Vertical Beam Position to 0x%04x - Not documented or implemented", _iValue);
		break;

	case VI_HORIZONTAL_BEAM_POSITION:
		WARN_LOG(VIDEOINTERFACE, "Change Horizontal Beam Position to 0x%04x - Not documented or implemented", _iValue);
		break;

		// RETRACE STUFF ...
	case VI_PRERETRACE_HI:
		m_InterruptRegister[0].Hi = _iValue;
		UpdateInterrupts();
		break;
	case VI_PRERETRACE_LO:
		m_InterruptRegister[0].Lo = _iValue;
		UpdateInterrupts();
		break;

	case VI_POSTRETRACE_HI:
		m_InterruptRegister[1].Hi = _iValue;
		UpdateInterrupts();
		break;
	case VI_POSTRETRACE_LO:
		m_InterruptRegister[1].Lo = _iValue;
		UpdateInterrupts();
		break;

	case VI_DISPLAY_INTERRUPT_2_HI:
		m_InterruptRegister[2].Hi = _iValue;
		UpdateInterrupts();
		break;
	case VI_DISPLAY_INTERRUPT_2_LO:
		m_InterruptRegister[2].Lo = _iValue;
		UpdateInterrupts();
		break;

	case VI_DISPLAY_INTERRUPT_3_HI:
		m_InterruptRegister[3].Hi = _iValue;
		UpdateInterrupts();
		break;
	case VI_DISPLAY_INTERRUPT_3_LO:
		m_InterruptRegister[3].Lo = _iValue;
		UpdateInterrupts();
		break;

	case VI_DISPLAY_LATCH_0_HI:
		m_LatchRegister[0].Hi = _iValue;
		break;
	case VI_DISPLAY_LATCH_0_LO:
		m_LatchRegister[0].Lo = _iValue;
		break;

	case VI_DISPLAY_LATCH_1_HI:
		m_LatchRegister[1].Hi = _iValue;
		break;
	case VI_DISPLAY_LATCH_1_LO:
		m_LatchRegister[1].Lo = _iValue;
		break;

	case VI_HSCALEW:
		m_HorizontalStepping.Hex = _iValue;
		break;

	case VI_HSCALER:
		m_HorizontalScaling.Hex = _iValue;
		break;

	case VI_FILTER_COEF_0_HI:
		m_FilterCoefTables.Tables02[0].Hi = _iValue;
		break;
	case VI_FILTER_COEF_0_LO:
		m_FilterCoefTables.Tables02[0].Lo = _iValue;
		break;
	case VI_FILTER_COEF_1_HI:
		m_FilterCoefTables.Tables02[1].Hi = _iValue;
		break;
	case VI_FILTER_COEF_1_LO:
		m_FilterCoefTables.Tables02[1].Lo = _iValue;
		break;
	case VI_FILTER_COEF_2_HI:
		m_FilterCoefTables.Tables02[2].Hi = _iValue;
		break;
	case VI_FILTER_COEF_2_LO:
		m_FilterCoefTables.Tables02[2].Lo = _iValue;
		break;
	case VI_FILTER_COEF_3_HI:
		m_FilterCoefTables.Tables36[0].Hi = _iValue;
		break;
	case VI_FILTER_COEF_3_LO:
		m_FilterCoefTables.Tables36[0].Lo = _iValue;
		break;
	case VI_FILTER_COEF_4_HI:
		m_FilterCoefTables.Tables36[1].Hi = _iValue;
		break;
	case VI_FILTER_COEF_4_LO:
		m_FilterCoefTables.Tables36[1].Lo = _iValue;
		break;
	case VI_FILTER_COEF_5_HI:
		m_FilterCoefTables.Tables36[2].Hi = _iValue;
		break;
	case VI_FILTER_COEF_5_LO:
		m_FilterCoefTables.Tables36[2].Lo = _iValue;
		break;
	case VI_FILTER_COEF_6_HI:
		m_FilterCoefTables.Tables36[3].Hi = _iValue;
		break;
	case VI_FILTER_COEF_6_LO:
		m_FilterCoefTables.Tables36[3].Lo = _iValue;
		break;

	case VI_UNK_AA_REG_HI:
		m_UnkAARegister = (m_UnkAARegister & 0x0000ffff) | (u32)(_iValue << 16);
		WARN_LOG(VIDEOINTERFACE, "(w16) to unknown AA reg, not sure what it does :)");
		break;
	case VI_UNK_AA_REG_LO:
		m_UnkAARegister = (m_UnkAARegister & 0xffff0000) | _iValue;
		WARN_LOG(VIDEOINTERFACE, "(w16) to unknown AA reg, not sure what it does :)");
		break;

	case VI_CLOCK:
		m_Clock = _iValue;
		break;

	case VI_DTV_STATUS:
		m_DTVStatus = _iValue;
		break;

	case VI_FBWIDTH:
		m_FBWidth = _iValue;
		break;

	case VI_BORDER_BLANK_END:
		m_BorderHBlank.Lo = _iValue;
		break;
	case VI_BORDER_BLANK_START:
		m_BorderHBlank.Hi = _iValue;
		break;

	default:
		ERROR_LOG(VIDEOINTERFACE, "(w16) %04x to unk reg %x", _iValue, _iAddress & 0xfff);
		break;
	}
}

void Read32(u32& _uReturnValue, const u32 _iAddress)
{
	u16 Hi = 0, Lo = 0;
	Read16(Hi, _iAddress);
	Read16(Lo, _iAddress + 2);
	_uReturnValue = (Hi << 16) | Lo;

	INFO_LOG(VIDEOINTERFACE, "(r32): 0x%08x, 0x%08x", _uReturnValue, _iAddress);
}

void Write32(const u32 _iValue, const u32 _iAddress)
{
	INFO_LOG(VIDEOINTERFACE, "(w32): 0x%08x, 0x%08x", _iValue, _iAddress);

	// Allow 32-bit writes to the VI: although this is officially not
	// allowed, the hardware seems to accept it (for example, DesktopMan GC
	// Tetris uses it).
	Write16(_iValue >> 16, _iAddress);
	Write16(_iValue & 0xFFFF, _iAddress + 2);
}

void UpdateInterrupts()
{
	if ((m_InterruptRegister[0].IR_INT && m_InterruptRegister[0].IR_MASK) ||
		(m_InterruptRegister[1].IR_INT && m_InterruptRegister[1].IR_MASK) ||
		(m_InterruptRegister[2].IR_INT && m_InterruptRegister[2].IR_MASK) ||
		(m_InterruptRegister[3].IR_INT && m_InterruptRegister[3].IR_MASK))
	{
		CPeripheralInterface::SetInterrupt(CPeripheralInterface::INT_CAUSE_VI, true);
	}
	else
	{
		CPeripheralInterface::SetInterrupt(CPeripheralInterface::INT_CAUSE_VI, false);
	}
}

u32 GetXFBAddressTop()
{
	if (m_XFBInfoTop.POFF)
		return m_XFBInfoTop.FBB << 5;
	else
		return m_XFBInfoTop.FBB;
}

u32 GetXFBAddressBottom()
{
	// POFF for XFB bottom is connected to POFF for XFB top
	if (m_XFBInfoTop.POFF)
		return m_XFBInfoBottom.FBB << 5;
	else
		return m_XFBInfoBottom.FBB;
}


// NTSC is 60 FPS, right?
// Wrong, it's about 59.94 FPS. The NTSC engineers had to slightly lower
// the field rate from 60 FPS when they added color to the standard.
// This was done to prevent analog interference between the video and
// audio signals. PAL has no similar reduction; it is exactly 50 FPS.
const double NTSC_FIELD_RATE = 60.0 / 1.001;
const u32 NTSC_LINE_COUNT = 525;
// These line numbers indicate the beginning of the "active video" in a frame.
// An NTSC frame has the lower field first followed by the upper field.
// TODO: Is this true for PAL-M? Is this true for EURGB60?
const u32 NTSC_LOWER_BEGIN = 21;
const u32 NTSC_UPPER_BEGIN = 283;

const double PAL_FIELD_RATE = 50.0;
const u32 PAL_LINE_COUNT = 625;
// These line numbers indicate the beginning of the "active video" in a frame.
// A PAL frame has the upper field first followed by the lower field.
const u32 PAL_UPPER_BEGIN = 23; // TODO: Actually 23.5!
const u32 PAL_LOWER_BEGIN = 336;

// Screenshot and screen message
void UpdateTiming()
{
    switch (m_DisplayControlRegister.FMT)
    {

    case 0: // NTSC
    case 2: // PAL-M

        TicksPerFrame = (u32)(SystemTimers::GetTicksPerSecond() / (NTSC_FIELD_RATE / 2.0));
		s_lineCount = m_DisplayControlRegister.NIN ? (NTSC_LINE_COUNT+1)/2 : NTSC_LINE_COUNT;
		// TODO: The game may have some control over these parameters (not that it's useful).
		s_upperFieldBegin = NTSC_UPPER_BEGIN;
		s_lowerFieldBegin = NTSC_LOWER_BEGIN;
        break;

    case 1: // PAL

        TicksPerFrame = (u32)(SystemTimers::GetTicksPerSecond() / (PAL_FIELD_RATE / 2.0));
		s_lineCount = m_DisplayControlRegister.NIN ? (PAL_LINE_COUNT+1)/2 : PAL_LINE_COUNT;
		s_upperFieldBegin = PAL_UPPER_BEGIN;
		s_lowerFieldBegin = PAL_LOWER_BEGIN;
        break;

	case 3: // Debug
		PanicAlert("Debug video mode not implemented");
		break;

    default:
        PanicAlert("Unknown Video Format - CVideoInterface");
        break;

    }
}

int getTicksPerLine() {
	if (s_lineCount == 0)
		return 100000;
	return TicksPerFrame / s_lineCount;
}


static void BeginField(FieldType field)
{
	u32 fbWidth = m_HorizontalStepping.FieldSteps * 16;
	u32 fbHeight = (m_HorizontalStepping.FbSteps / m_HorizontalStepping.FieldSteps) * m_VerticalTimingRegister.ACV;

	// TODO: Are the "Bottom Field" and "Top Field" registers actually more
	// like "First Field" and "Second Field" registers? There's an important
	// difference because NTSC and PAL have opposite field orders.

	u32 xfbAddr = (field == FIELD_LOWER) ? GetXFBAddressBottom() : GetXFBAddressTop();

	static const char* const fieldTypeNames[] = { "Progressive", "Upper", "Lower" };

	DEBUG_LOG(VIDEOINTERFACE, "(VI->BeginField): addr: %.08X | FieldSteps %u | FbSteps %u | ACV %u | Field %s",
		xfbAddr, m_HorizontalStepping.FieldSteps, m_HorizontalStepping.FbSteps, m_VerticalTimingRegister.ACV,
		fieldTypeNames[field]
	);

	Common::PluginVideo* video = CPluginManager::GetInstance().GetVideo();
	if (xfbAddr && video->IsValid())
		video->Video_BeginField(xfbAddr, field, fbWidth, fbHeight);
}

static void EndField()
{
	Common::PluginVideo* video = CPluginManager::GetInstance().GetVideo();
	if (video->IsValid())
		video->Video_EndField();
}


// Purpose 1: Send VI interrupt for every screen refresh
// Purpose 2: Execute XFB copy in homebrew games
// Run when: This is run 15700 times per second on full speed
void Update()
{
	// Update the target refresh rate
	TargetRefreshRate = (float)((m_DisplayControlRegister.FMT == 0 || m_DisplayControlRegister.FMT == 2)
		? NTSC_FIELD_RATE : PAL_FIELD_RATE);

	// Calculate actual refresh rate
	static u64 LastTick = 0;
	static s64 UpdateCheck = timeGetTime() + 1000, TickProgress = 0;
	s64 curTime = timeGetTime();
	if (UpdateCheck < (int)curTime)
	{
		UpdateCheck = curTime + 1000;
		TickProgress = CoreTiming::GetTicks() - LastTick;
		// Calculated CPU-GPU synced ticks for the dual core mode too
		// NOTICE_LOG(VIDEO, "Removed: %s Mhz", ThS(SyncTicksProgress / 1000000, false).c_str());
		SyncTicksProgress += TickProgress;
		// Multipled by two because of the way TicksPerFrame is calculated (divided by 25 and 30
		// rather than 50 and 60)

		ActualRefreshRate = (float)(((double)SyncTicksProgress / (double)TicksPerFrame) * 2.0);		
		LastTick = CoreTiming::GetTicks();
		SyncTicksProgress = 0;
	}

	// TODO: What's the correct behavior for progressive mode?

	if (m_VBeamPos == s_upperFieldBegin + m_VerticalTimingRegister.ACV)
		EndField();

	if (m_VBeamPos == s_lowerFieldBegin + m_VerticalTimingRegister.ACV)
		EndField();

    m_VBeamPos++;
    if (m_VBeamPos > s_lineCount)
        m_VBeamPos = 1;

	for (int i = 0; i < 4; ++i)
	{
		if (m_InterruptRegister[i].VCT == m_VBeamPos)
			m_InterruptRegister[i].IR_INT = 1;
	}
	UpdateInterrupts();

	if (m_VBeamPos == s_upperFieldBegin)
		BeginField(m_DisplayControlRegister.NIN ? FIELD_PROGRESSIVE : FIELD_UPPER);

	if (m_VBeamPos == s_lowerFieldBegin)
		BeginField(m_DisplayControlRegister.NIN ? FIELD_PROGRESSIVE : FIELD_LOWER);
}

} // namespace
