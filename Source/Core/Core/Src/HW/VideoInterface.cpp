// Copyright (C) 2003-2008 Dolphin Project.

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

#include "PeripheralInterface.h"
#include "VideoInterface.h"
#include "Memmap.h"
#include "../PluginManager.h"
#include "../CoreTiming.h"
#include "../HW/SystemTimers.h"

namespace VideoInterface
{
// VI Internal Hardware Addresses
enum
{
	VI_VERTICAL_TIMING					= 0x0,
	VI_CONTROL_REGISTER	                = 0x02,
	VI_FRAMEBUFFER_TOP_HI				= 0x01C,
	VI_FRAMEBUFFER_TOP_LO				= 0x01E,
	VI_FRAMEBUFFER_BOTTOM_HI			= 0x024,
	VI_FRAMEBUFFER_BOTTOM_LO			= 0x026,
	VI_VERTICAL_BEAM_POSITION			= 0x02C,
	VI_HORIZONTAL_BEAM_POSITION			= 0x02e,
	VI_PRERETRACE						= 0x030,
	VI_POSTRETRACE						= 0x034,
	VI_DI2                              = 0x038,
	VI_DI3                              = 0x03C,
	VI_INTERLACE						= 0x850,
	VI_HSCALEW                          = 0x048,
	VI_HSCALER                          = 0x04A,
	VI_FBWIDTH							= 0x070,
};

union UVIVerticalTimingRegister
{
	u16 Hex;
	struct
	{
		unsigned EQU	:	4;
		unsigned ACV	:	10;
		unsigned		:	2;
	};
	UVIVerticalTimingRegister(u16 _hex) { Hex = _hex;}
	UVIVerticalTimingRegister() { Hex = 0;}
};

union UVIDisplayControlRegister
{
	u16 Hex;
	struct
	{
		unsigned ENB	:	1;
		unsigned RST	:	1;
		unsigned NIN	:	1;
		unsigned DLR	:	1;
		unsigned LE0	:	2;
		unsigned LE1	:	2;
		unsigned FMT	:	2;
		unsigned		:	6;
	};
	UVIDisplayControlRegister(u16 _hex) { Hex = _hex;}
	UVIDisplayControlRegister() { Hex = 0;}
};

// VI Interrupt Register
union UVIInterruptRegister
{
	u32 Hex;
	struct
	{
		u16 Lo;
		u16 Hi;
	};
	struct  
	{		
		unsigned HCT		:	11;
		unsigned			:	5;
		unsigned VCT		:	11;
		unsigned			:	1;
		unsigned IR_MASK	:	1;
		unsigned			:	2;
		unsigned IR_INT		:	1;
	};
};

union UVIHorizontalStepping
{
	u16 Hex;
	struct
	{
		unsigned FbSteps		:	8;
		unsigned FieldSteps		:	8;
	};
};

union UVIHorizontalScaling
{
	u16 Hex;
	struct
	{
		unsigned STP		:	9;
		unsigned			:	3;
		unsigned HS_EN		:	1;
		unsigned			:	3;
	};
	UVIHorizontalScaling(u16 _hex) { Hex = _hex;}
	UVIHorizontalScaling() { Hex = 0;}
};

union UVIFrameBufferAddress
{
	u32 Hex;
	struct
	{
		u16 Lo;
		u16 Hi;
	};
	UVIFrameBufferAddress(u16 _hex) { Hex = _hex;}
	UVIFrameBufferAddress() { Hex = 0;}
};

// STATE_TO_SAVE
static UVIVerticalTimingRegister m_VIVerticalTimingRegister;

static UVIDisplayControlRegister m_VIDisplayControlRegister;

// Framebuffers
static UVIFrameBufferAddress m_FrameBufferTop;		// normal framebuffer address
static UVIFrameBufferAddress m_FrameBufferBottom;

// VI Interrupt Registers
static UVIInterruptRegister m_VIInterruptRegister[4];

static UVIHorizontalStepping m_VIHorizontalStepping;
static UVIHorizontalScaling m_VIHorizontalScaling;

u8 m_UVIUnknownRegs[0x1000];

static u16 HorizontalBeamPos = 0;
static u16 VerticalBeamPos = 0;

static u32 TicksPerFrame = 0;
static u32 LineCount = 0;
static u32 LinesPerField = 0;
static u64 LastTime = 0;
static u32 NextXFBRender = 0;

// only correct when scaling is enabled?
static u16 FbWidth = 0;

void DoState(PointerWrap &p)
{
	p.Do(m_VIVerticalTimingRegister);
	p.Do(m_VIDisplayControlRegister);
	p.Do(m_FrameBufferTop);
	p.Do(m_FrameBufferBottom);
	p.Do(m_VIInterruptRegister);
	p.DoArray(m_UVIUnknownRegs, 0x1000);
	p.Do(HorizontalBeamPos);
	p.Do(VerticalBeamPos);
	p.Do(TicksPerFrame);
	p.Do(LineCount);
	p.Do(LinesPerField);
	p.Do(LastTime);
	// p.Do(NextXFBRender);   // Activate when changing saves next time.
}

void Init()
{
	for (int i = 0; i < 4; i++)
		m_VIInterruptRegister[i].Hex = 0x00;

	m_VIDisplayControlRegister.Hex = 0x0000;
	m_VIDisplayControlRegister.ENB = 0;
	m_VIDisplayControlRegister.FMT = 0;

	NextXFBRender = 1;
}

void Read16(u16& _uReturnValue, const u32 _iAddress)
{
	switch (_iAddress & 0xFFF)
	{
	case VI_VERTICAL_TIMING:
		_uReturnValue = m_VIVerticalTimingRegister.Hex;
		return;

	case VI_CONTROL_REGISTER:
        LOGV(VIDEOINTERFACE, 3, "VideoInterface(r16): VI_CONTROL_REGISTER 0x%08x", m_VIDisplayControlRegister.Hex);
		_uReturnValue = m_VIDisplayControlRegister.Hex;
		return;

	case VI_FRAMEBUFFER_TOP_HI:
		_uReturnValue = m_FrameBufferTop.Hi;
		break;

	case VI_FRAMEBUFFER_TOP_LO:
		_uReturnValue = m_FrameBufferTop.Lo;
		break;

	case VI_FRAMEBUFFER_BOTTOM_HI:
		_uReturnValue = m_FrameBufferBottom.Hi;
		break;

	case VI_FRAMEBUFFER_BOTTOM_LO:
		_uReturnValue = m_FrameBufferBottom.Lo;
		break;

	case VI_VERTICAL_BEAM_POSITION:
    	_uReturnValue = VerticalBeamPos;
		return;

	case VI_HORIZONTAL_BEAM_POSITION:
        _uReturnValue = HorizontalBeamPos;
		return;

	// RETRACE STUFF ...
	case VI_PRERETRACE:
		_uReturnValue =	m_VIInterruptRegister[0].Hi;
		return;

	case VI_POSTRETRACE:
		_uReturnValue =	m_VIInterruptRegister[1].Hi;
		return;

	case VI_DI2:
		_uReturnValue =	m_VIInterruptRegister[2].Hi;
		return;

	case VI_DI3:
		_uReturnValue =	m_VIInterruptRegister[3].Hi;
		return;

	case VI_HSCALEW:
		_uReturnValue = m_VIHorizontalStepping.Hex;
		break;

	case VI_HSCALER:
		_uReturnValue = m_VIHorizontalScaling.Hex;
		break;

	case VI_FBWIDTH:
		_uReturnValue = FbWidth;
		break;

	default:		
		_uReturnValue = *(u16*)&m_UVIUnknownRegs[_iAddress & 0xFFF];
		return;
	}

	_uReturnValue = 0x0;
}

void Write16(const u16 _iValue, const u32 _iAddress)
{
	LOGV(VIDEOINTERFACE, 3, "(w16): 0x%04x, 0x%08x",_iValue,_iAddress);

	//Somewhere it sets screen width.. we need to communicate this to the gfx plugin...

	switch (_iAddress & 0xFFF)
	{
	case VI_VERTICAL_TIMING:
		m_VIVerticalTimingRegister.Hex = _iValue;
		break;

	case VI_CONTROL_REGISTER:
		{
			UVIDisplayControlRegister tmpConfig(_iValue);
			m_VIDisplayControlRegister.ENB = tmpConfig.ENB;
			m_VIDisplayControlRegister.NIN = tmpConfig.NIN;
			m_VIDisplayControlRegister.DLR = tmpConfig.DLR;
			m_VIDisplayControlRegister.LE0 = tmpConfig.LE0;
			m_VIDisplayControlRegister.LE1 = tmpConfig.LE1;
			m_VIDisplayControlRegister.FMT = tmpConfig.FMT;

			if (tmpConfig.RST)
			{
				m_VIDisplayControlRegister.RST = 0;
			}

            UpdateTiming();
		}		
		break;

	case VI_FRAMEBUFFER_TOP_HI:
		m_FrameBufferTop.Hi = _iValue;
		break;

	case VI_FRAMEBUFFER_TOP_LO:
		m_FrameBufferTop.Lo = _iValue;
		break;

	case VI_FRAMEBUFFER_BOTTOM_HI:
		m_FrameBufferBottom.Hi = _iValue;
		break;

	case VI_FRAMEBUFFER_BOTTOM_LO:
		m_FrameBufferBottom.Lo = _iValue;
		break;

	case VI_VERTICAL_BEAM_POSITION:
		_dbg_assert_(VIDEOINTERFACE,0);
		break;

	case VI_HORIZONTAL_BEAM_POSITION:
		_dbg_assert_(VIDEOINTERFACE,0);
		break;

		// RETRACE STUFF ...
	case VI_PRERETRACE:
		m_VIInterruptRegister[0].Hi = _iValue;
		UpdateInterrupts();
		break;

	case VI_POSTRETRACE:
		m_VIInterruptRegister[1].Hi = _iValue;
		UpdateInterrupts();
		break;

	case VI_DI2:
		m_VIInterruptRegister[2].Hi = _iValue;
		UpdateInterrupts();
		break;

	case VI_DI3:
		m_VIInterruptRegister[3].Hi = _iValue;
		UpdateInterrupts();
		break;

	case VI_HSCALEW:
		m_VIHorizontalStepping.Hex = _iValue;
		break;

	case VI_HSCALER:
		m_VIHorizontalScaling.Hex = _iValue;
		break;

	case VI_FBWIDTH:
		FbWidth = _iValue;
		break;

	default:
		*(u16*)&m_UVIUnknownRegs[_iAddress & 0xFFF] = _iValue;
		break;
	}
}

void Read32(u32& _uReturnValue, const u32 _iAddress)
{
	LOG(VIDEOINTERFACE, "(r32): 0x%08x", _iAddress);

	if ((_iAddress & 0xFFF) < 0x20)
	{
		_uReturnValue = 0;
		return;
	}
	switch (_iAddress & 0xFFF)
	{
	case 0x000:
	case 0x004:
	case 0x008:
	case 0x00C:
		_uReturnValue = 0; //FL
		return;

	default:
		//_assert_(0);
		_uReturnValue = 0xAFFE;
		return;
	}
}

void Write32(const u32 _iValue, const u32 _iAddress)
{
	LOG(VIDEOINTERFACE, "(w32): 0x%08x, 0x%08x",_iValue,_iAddress);

	// Allow 32-bit writes to the VI: although this is officially not
	// allowed, the hardware seems to accept it (for example, DesktopMan GC
	// Tetris uses it).
	Write16(_iValue >> 16, _iAddress);
	Write16(_iValue & 0xFFFF, _iAddress + 2);
}

void UpdateInterrupts()
{
	if ((m_VIInterruptRegister[0].IR_INT & m_VIInterruptRegister[0].IR_MASK) ||
		(m_VIInterruptRegister[1].IR_INT & m_VIInterruptRegister[1].IR_MASK) ||
		(m_VIInterruptRegister[2].IR_INT & m_VIInterruptRegister[2].IR_MASK) ||
		(m_VIInterruptRegister[3].IR_INT & m_VIInterruptRegister[3].IR_MASK))
	{
		CPeripheralInterface::SetInterrupt(CPeripheralInterface::INT_CAUSE_VI, true);
	}
	else
	{
		CPeripheralInterface::SetInterrupt(CPeripheralInterface::INT_CAUSE_VI, false);
	}
}

void GenerateVIInterrupt(VIInterruptType _VIInterrupt)
{
	switch(_VIInterrupt)
	{
	case INT_PRERETRACE:	m_VIInterruptRegister[0].IR_INT = 1; break;
	case INT_POSTRETRACE:	m_VIInterruptRegister[1].IR_INT = 1; break;
	case INT_REG_2:			m_VIInterruptRegister[2].IR_INT = 1; break;
	case INT_REG_3:			m_VIInterruptRegister[3].IR_INT = 1; break;
	}

	UpdateInterrupts();

	// debug check
	if ((m_VIInterruptRegister[2].IR_MASK == 1) ||
		(m_VIInterruptRegister[3].IR_MASK == 1))
	{
		PanicAlert("m_VIInterruptRegister[2 and 3] activated - Tell F|RES :)");
	}
}

u8* GetFrameBufferPointer()
{
	return Memory::GetPointer(VideoInterface::m_FrameBufferTop.Hex);
}

void PreInit(bool _bNTSC)
{
	TicksPerFrame = 0;
	LineCount = 0;
	LastTime = 0;

	Write16(0x4769, 0xcc002004);
	Write16(0x01ad, 0xcc002006);
	Write16(0x5140, 0xcc00200a);
	Write16(0x02ea, 0xcc002008);
	Write16(0x0006, 0xcc002000);
	Write16(0x01f6, 0xcc00200e);
	Write16(0x0005, 0xcc00200c);
	Write16(0x01f7, 0xcc002012);
	Write16(0x0004, 0xcc002010);
	Write16(0x410c, 0xcc002016);
	Write16(0x410c, 0xcc002014);
	Write16(0x40ed, 0xcc00201a);
	Write16(0x40ed, 0xcc002018);
	Write16(0x2828, 0xcc002048);
	Write16(0x0001, 0xcc002036);
	Write16(0x1001, 0xcc002034);
	Write16(0x01ae, 0xcc002032);
	Write16(0x1107, 0xcc002030);
	Write16(0x0000, 0xcc00206c);
	Write16(0x0001, 0xcc00206e);  // component cable is connected

    if (_bNTSC)
        Write16(0x0001, 0xcc002002);	// STATUS REG
    else
        Write16(0x0101, 0xcc002002);	// STATUS REG
}

void UpdateTiming()
{
    switch (m_VIDisplayControlRegister.FMT)
    {
    case 0:
    case 2:
        TicksPerFrame = SystemTimers::GetTicksPerSecond() / 30;
        LineCount = m_VIDisplayControlRegister.NIN ? 263 : 525;
		LinesPerField = 263;
        break;

    case 1:
        TicksPerFrame = SystemTimers::GetTicksPerSecond() / 25;
        LineCount = m_VIDisplayControlRegister.NIN ? 313 : 625;
		LinesPerField = 313;
        break;

    default:
        PanicAlert("Unknown Video Format - CVideoInterface");
        break;
    }
}

void Update()
{
    while ((CoreTiming::GetTicks() - LastTime) > (TicksPerFrame / LineCount))
    {
        LastTime += (TicksPerFrame / LineCount);

        //
        VerticalBeamPos++;
        if (VerticalBeamPos > LineCount)
        {
            VerticalBeamPos = 1;
        }

		if (VerticalBeamPos == NextXFBRender)
		{
			u8* xfbPtr = 0;
			int yOffset = 0;

			if (NextXFBRender == 1)
			{
				NextXFBRender = LinesPerField;
				// The & mask is a hack for mario kart
				u32 addr = (VideoInterface::m_FrameBufferTop.Hex & 0xFFFFFFF) | 0x80000000;
				if (addr >= 0x80000000 &&
					addr <= (0x81800000-640*480*2))
					xfbPtr = Memory::GetPointer(addr);
			}
			else
			{
				NextXFBRender = 1;
				u32 addr = (VideoInterface::m_FrameBufferBottom.Hex & 0xFFFFFFF) | 0x80000000;
				if (addr >= 0x80000000 &&
					addr <= (0x81800000-640*480*2))
					xfbPtr = Memory::GetPointer(addr);
				yOffset = -1;
			}
			Common::PluginVideo* video = CPluginManager::GetInstance().GetVideo();
			if (xfbPtr && video->IsValid())
			{
				int fbWidth = m_VIHorizontalStepping.FieldSteps * 16;
				int fbHeight = (m_VIHorizontalStepping.FbSteps / m_VIHorizontalStepping.FieldSteps) * m_VIVerticalTimingRegister.ACV;				
				video->Video_UpdateXFB(xfbPtr, fbWidth, fbHeight, yOffset);
			}
		}

        // check INT_PRERETRACE
        if (m_VIInterruptRegister[0].VCT == VerticalBeamPos)
        {
            m_VIInterruptRegister[0].IR_INT = 1;
            UpdateInterrupts();
        }

        // INT_POSTRETRACE
        if (m_VIInterruptRegister[1].VCT == VerticalBeamPos)
        {
            m_VIInterruptRegister[1].IR_INT = 1;
            UpdateInterrupts();
        }
    }
}

}
