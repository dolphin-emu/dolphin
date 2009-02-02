// Copyright (C) 2003-2009 Dolphin Project.

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
#include "../ConfigManager.h"

#include "PeripheralInterface.h"

#include "SI.h"
#include "SI_Channel.h"


namespace SerialInterface
{

// SI Interrupt Types
enum SIInterruptType
{
	INT_RDSTINT		= 0,
	INT_TCINT		= 1,
};

// SI number of channels
enum
{
	NUMBER_OF_CHANNELS	= 4
};

// SI Poll: Controls how often a device is polled
union USIPoll
{
	u32 Hex;
	struct
	{
		unsigned VBCPY3		:	1;
		unsigned VBCPY2		:	1;
		unsigned VBCPY1		:	1;
		unsigned VBCPY0		:	1;
		unsigned EN3		:	1;
		unsigned EN2		:	1;
		unsigned EN1		:	1;
		unsigned EN0		:	1;
		unsigned Y			:  10;
		unsigned X			:  10;
		unsigned			:	6;
	};
};

// SI Communication Control Status Register
union USIComCSR
{
	u32 Hex;
	struct
	{
		unsigned TSTART		:	1;
		unsigned CHANNEL    :	2; // determines which SI channel will be used the communication interface.
		unsigned			:	5;
		unsigned INLNGTH	:	7;
		unsigned			:	1;
		unsigned OUTLNGTH	:	7; // Communication Channel Output Length in bytes
		unsigned			:	4;
		unsigned RDSTINTMSK	:	1; // Read Status Interrupt Status Mask
		unsigned RDSTINT	:	1; // Read Status Interrupt Status
		unsigned COMERR		:	1; // Communication Error (set 0)
		unsigned TCINTMSK	:	1; // Transfer Complete Interrupt Mask
		unsigned TCINT		:	1; // Transfer Complete Interrupt
	};
	USIComCSR() {Hex = 0;}
	USIComCSR(u32 _hex) {Hex = _hex;}
};

// SI Status Register
union USIStatusReg
{
	u32 Hex;
	struct
	{
		unsigned UNRUN3	:	1; // (RWC) write 1: bit cleared  read 1: main proc underrun error
		unsigned OVRUN3	:	1; // (RWC) write 1: bit cleared  read 1: overrun error
		unsigned COLL3	:	1; // (RWC) write 1: bit cleared  read 1: collision error
		unsigned NOREP3	:	1; // (RWC) write 1: bit cleared  read 1: response error
		unsigned WRST3	:	1; // (R) 1: buffer channel0 not copied
		unsigned RDST3	:	1; // (R) 1: new Data available
		unsigned		:	2; // 7:6
		unsigned UNRUN2	:	1; // (RWC) write 1: bit cleared  read 1: main proc underrun error
		unsigned OVRUN2	:	1; // (RWC) write 1: bit cleared  read 1: overrun error
		unsigned COLL2	:	1; // (RWC) write 1: bit cleared  read 1: collision error
		unsigned NOREP2	:	1; // (RWC) write 1: bit cleared  read 1: response error
		unsigned WRST2	:	1; // (R) 1: buffer channel0 not copied
		unsigned RDST2	:	1; // (R) 1: new Data available
		unsigned		:	2; // 15:14
		unsigned UNRUN1	:	1; // (RWC) write 1: bit cleared  read 1: main proc underrun error
		unsigned OVRUN1	:	1; // (RWC) write 1: bit cleared  read 1: overrun error
		unsigned COLL1	:	1; // (RWC) write 1: bit cleared  read 1: collision error
		unsigned NOREP1	:	1; // (RWC) write 1: bit cleared  read 1: response error
		unsigned WRST1	:	1; // (R) 1: buffer channel0 not copied
		unsigned RDST1	:	1; // (R) 1: new Data available
		unsigned		:	2; // 23:22
		unsigned UNRUN0	:	1; // (RWC) write 1: bit cleared  read 1: main proc underrun error
		unsigned OVRUN0	:	1; // (RWC) write 1: bit cleared  read 1: overrun error
		unsigned COLL0	:	1; // (RWC) write 1: bit cleared  read 1: collision error
		unsigned NOREP0	:	1; // (RWC) write 1: bit cleared  read 1: response error
		unsigned WRST0	:	1; // (R) 1: buffer channel0 not copied
		unsigned RDST0	:	1; // (R) 1: new Data available
		unsigned		:	1;
		unsigned WR		:	1; // (RW) write 1 start copy, read 0 copy done
	};
	USIStatusReg() {Hex = 0;}
	USIStatusReg(u32 _hex) {Hex = _hex;}
};

// SI EXI Clock Count
union USIEXIClockCount
{
	u32 Hex;
	struct
	{
		unsigned LOCK	:	1;
		unsigned		:  30;
	};
};

// STATE_TO_SAVE
CSIChannel				*g_Channel; // Save the channel state here?
static USIPoll			g_Poll;
static USIComCSR		g_ComCSR;
static USIStatusReg		g_StatusReg;
static USIEXIClockCount	g_EXIClockCount;
static u8				g_SIBuffer[128];

static void GenerateSIInterrupt(SIInterruptType _SIInterrupt); 
void RunSIBuffer();
void UpdateInterrupts();


void Init()
{
	g_Channel = new CSIChannel[NUMBER_OF_CHANNELS];

	for (int i = 0; i < NUMBER_OF_CHANNELS; i++)
	{
		g_Channel[i].m_ChannelId = i;
		g_Channel[i].AddDevice(SConfig::GetInstance().m_SIDevice[i], i);
	}

	g_Poll.Hex = 0;
	g_ComCSR.Hex = 0;
	g_StatusReg.Hex = 0;
	g_EXIClockCount.Hex = 0;
	memset(g_SIBuffer, 0xce, 128);
}

void Shutdown()
{
	delete [] g_Channel;
	g_Channel = 0;
}

void DoState(PointerWrap &p)
{
	// p.DoArray(g_Channel);
	p.Do(g_Poll);
	p.Do(g_ComCSR);
	p.Do(g_StatusReg);
	p.Do(g_EXIClockCount);
	p.Do(g_SIBuffer);
}

void Read32(u32& _uReturnValue, const u32 _iAddress)
{
	LOGV(SERIALINTERFACE, 3, "(r32): 0x%08x", _iAddress);

	// SIBuffer
	if ((_iAddress >= 0xCC006480 && _iAddress < 0xCC006500) ||
		(_iAddress >= 0xCD006480 && _iAddress < 0xCD006500))
	{
		_uReturnValue = *(u32*)&g_SIBuffer[_iAddress & 0x7F];
		return;
	}

	// (shuffle2) these assignments are taken from EXI,
	// the iAddr amounted to the same, so I'm guessing the rest are too :D
	unsigned int iAddr = _iAddress & 0x3FF;
	unsigned int iRegister = (iAddr >> 2) % 5;
	unsigned int iChannel = (iAddr >> 2) / 5;

	if (iChannel < NUMBER_OF_CHANNELS)
		g_Channel[iChannel].Read32(_uReturnValue, iAddr);
}

void Write32(const u32 _iValue, const u32 _iAddress)
{
	LOGV(SERIALINTERFACE, 3, "(w32): 0x%08x 0x%08x", _iValue, _iAddress);

	// SIBuffer
	if ((_iAddress >= 0xCC006480 && _iAddress < 0xCC006500) ||
		(_iAddress >= 0xCD006480 && _iAddress < 0xCD006500))
	{
		*(u32*)&g_SIBuffer[_iAddress & 0x7F] = _iValue;
		return;
	}

	int iAddr = _iAddress & 0x3FF;
	int iRegister = (iAddr >> 2) % 5;
	int iChannel = (iAddr >> 2) / 5;

	if (iChannel < NUMBER_OF_CHANNELS)
		g_Channel[iChannel].Write32(_iValue, iAddr);
}

void UpdateInterrupts()
{
	// check if we have to update the RDSTINT flag
	if (g_StatusReg.RDST0 || g_StatusReg.RDST1 ||
		g_StatusReg.RDST2 || g_StatusReg.RDST3)
		g_ComCSR.RDSTINT = 1;
	else
		g_ComCSR.RDSTINT = 0;

	// check if we have to generate an interrupt
	if ((g_ComCSR.RDSTINT	& g_ComCSR.RDSTINTMSK) ||
		(g_ComCSR.TCINT		& g_ComCSR.TCINTMSK))
	{
		CPeripheralInterface::SetInterrupt(CPeripheralInterface::INT_CAUSE_SI, true);
	}
	else
	{
		CPeripheralInterface::SetInterrupt(CPeripheralInterface::INT_CAUSE_SI, false);
	}
}

void GenerateSIInterrupt(SIInterruptType _SIInterrupt)
{
	switch(_SIInterrupt)
	{
	case INT_RDSTINT:	g_ComCSR.RDSTINT = 1; break;
	case INT_TCINT:		g_ComCSR.TCINT = 1; break;
	}

	UpdateInterrupts();
}

void UpdateDevices()
{
	// Update channels
	g_StatusReg.RDST0 = g_Channel[0].m_pDevice->GetData(g_Channel[0].m_InHi.Hex, g_Channel[0].m_InLo.Hex) ? 1 : 0;
	g_StatusReg.RDST1 = g_Channel[1].m_pDevice->GetData(g_Channel[1].m_InHi.Hex, g_Channel[1].m_InLo.Hex) ? 1 : 0;
	g_StatusReg.RDST2 = g_Channel[2].m_pDevice->GetData(g_Channel[2].m_InHi.Hex, g_Channel[2].m_InLo.Hex) ? 1 : 0;
	g_StatusReg.RDST3 = g_Channel[3].m_pDevice->GetData(g_Channel[3].m_InHi.Hex, g_Channel[3].m_InLo.Hex) ? 1 : 0;

	// Update interrupts
	UpdateInterrupts();
}

void RunSIBuffer()
{
	// Math inLength
	int inLength = g_ComCSR.INLNGTH;
	if (inLength == 0)
		inLength = 128;
	else
		inLength++;

	// Math outLength
	int outLength = g_ComCSR.OUTLNGTH;
	if (outLength == 0)
		outLength = 128;
	else
		outLength++;

	#ifdef LOGGING
		int numOutput =
	#endif
		g_Channel[g_ComCSR.CHANNEL].m_pDevice->RunBuffer(g_SIBuffer, inLength);

	LOGV(SERIALINTERFACE, 2, "RunSIBuffer     (intLen: %i    outLen: %i) (processed: %i)", inLength, outLength, numOutput);

	// Transfer completed
	GenerateSIInterrupt(INT_TCINT);
	g_ComCSR.TSTART = 0;
}

} // end of namespace SerialInterface
