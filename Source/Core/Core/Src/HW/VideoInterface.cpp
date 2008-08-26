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
#include "../Plugins/Plugin_Video.h"
#include "../CoreTiming.h"
#include "../HW/SystemTimers.h"

namespace VideoInterface
{
// VI Internal Hardware Addresses
enum
{
	VI_CONTROL_REGISTER	                = 0x02,
	VI_FRAMEBUFFER_1					= 0x01C,
	VI_FRAMEBUFFER_2					= 0x024,
	VI_VERTICAL_BEAM_POSITION			= 0x02C,
	VI_HORIZONTAL_BEAM_POSITION			= 0x02e,
	VI_PRERETRACE						= 0x030,
	VI_POSTRETRACE						= 0x034,
	VI_DI2                              = 0x038,
	VI_DI3                              = 0x03C,
	VI_INTERLACE						= 0x850,
	VI_HSCALEW                          = 0x04A,
	VI_HSCALER                          = 0x04C,
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

// STATE_TO_SAVE
static UVIDisplayControlRegister m_VIDisplayControlRegister;

// Framebuffers
static u32 m_FrameBuffer1;		// normal framebuffer address
static u32 m_FrameBuffer2;		// framebuffer for 3d buffer address

// VI Interrupt Registers
static UVIInterruptRegister m_VIInterruptRegister[4];

u8 m_UVIUnknownRegs[0x1000];

static u16 HorizontalBeamPos = 0;
static u16 VerticalBeamPos = 0;

static u32 TicksPerFrame = 0;
static u32 LineCount = 0;
static u64 LastTime = 0;

void DoState(ChunkFile &f)
{
	f.Descend("VI  ");
	f.Do(m_VIDisplayControlRegister);
	f.Do(m_FrameBuffer1);
	f.Do(m_FrameBuffer2);
	f.Do(m_VIInterruptRegister);
	f.Do(m_UVIUnknownRegs, 0x1000);
	f.Do(HorizontalBeamPos);
	f.Do(VerticalBeamPos);
	f.Do(TicksPerFrame);
	f.Do(LineCount);
	f.Do(LastTime);
	f.Ascend();
}

void Init()
{
	for (int i = 0; i < 4; i++)
		m_VIInterruptRegister[i].Hex = 0x00;

	m_VIDisplayControlRegister.Hex = 0x0000;
	m_VIDisplayControlRegister.ENB = 0;
	m_VIDisplayControlRegister.FMT = 0;
}

void Read16(u16& _uReturnValue, const u32 _iAddress)
{
	switch (_iAddress & 0xFFF)
	{
	case VI_CONTROL_REGISTER:
        LOG(VIDEOINTERFACE, "VideoInterface(r16): VI_CONTROL_REGISTER 0x%08x", m_VIDisplayControlRegister.Hex);
		_uReturnValue = m_VIDisplayControlRegister.Hex;
		return;

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

	case 0x38:
		_uReturnValue =	m_VIInterruptRegister[2].Hi;
		return;

	case 0x3C:
		_uReturnValue =	m_VIInterruptRegister[3].Hi;
		return;

	default:
		_uReturnValue = *(u16*)&m_UVIUnknownRegs[_iAddress & 0xFFF];
		return;
	}

	_uReturnValue = 0x0;
}

void Write16(const u16 _iValue, const u32 _iAddress)
{
	LOG(VIDEOINTERFACE, "(w16): 0x%04x, 0x%08x",_iValue,_iAddress);
	
	//Somewhere it sets screen width.. we need to communicate this to the gfx plugin...

	switch (_iAddress & 0xFFF)
	{
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
		{
			// int width = _iValue&0x3FF;
			
		}
		break;
	case VI_HSCALER:
		{
			// int hsEnable = (_iValue&(1<<12)) ? true : false;
		}
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

	switch(_iAddress & 0xFFF)
	{
	case VI_FRAMEBUFFER_1:
		m_FrameBuffer1 = _iValue;
		break;
	case VI_FRAMEBUFFER_2:
		m_FrameBuffer2 = _iValue;
		break;

	default:
		// Allow 32-bit writes to the VI: although this is officially not
		// allowed, the hardware seems to accept it (for example, DesktopMan GC
		// Tetris uses it).
		Write16(_iValue >> 16, _iAddress);
		Write16(_iValue & 0xFFFF, _iAddress + 2);
		break;
	}
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
	return Memory::GetPointer(VideoInterface::m_FrameBuffer1);
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
        break;

    case 1:
        TicksPerFrame = SystemTimers::GetTicksPerSecond() / 25;
        LineCount = m_VIDisplayControlRegister.NIN ? 313 : 625;
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

