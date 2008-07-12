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

#include <string.h>

#include "SerialInterface.h"
#include "SerialInterface_Devices.h"

#include "PeripheralInterface.h"
#include "CPU.h"

#include "../PowerPC/PowerPC.h"

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
	NUMBER_OF_CHANNELS	= 0x04
};

// SI Internal Hardware Addresses
enum
{
	SI_CHANNEL_0_OUT	= 0x00,
	SI_CHANNEL_0_IN_HI	= 0x04,
	SI_CHANNEL_0_IN_LO	= 0x08,
	SI_CHANNEL_1_OUT	= 0x0C,
	SI_CHANNEL_1_IN_HI	= 0x10,
	SI_CHANNEL_1_IN_LO	= 0x14,
	SI_CHANNEL_2_OUT	= 0x18,
	SI_CHANNEL_2_IN_HI	= 0x1C,
	SI_CHANNEL_2_IN_LO	= 0x20,
	SI_CHANNEL_3_OUT	= 0x24,
	SI_CHANNEL_3_IN_HI	= 0x28,
	SI_CHANNEL_3_IN_LO	= 0x2C,
	SI_POLL				= 0x30,
	SI_COM_CSR			= 0x34,
	SI_STATUS_REG		= 0x38,
	SI_EXI_CLOCK_COUNT	= 0x3C,
};

// SI Channel Output
union USIChannelOut
{
	u32 Hex;
	struct  
	{		
		unsigned OUTPUT1	:	8;
		unsigned OUTPUT0	:	8;
		unsigned CMD		:	8;
		unsigned			:	8;
	};
};

// SI Channel Input High u32
union USIChannelIn_Hi
{
	u32 Hex;
	struct  
	{		
		unsigned INPUT3		:	8;
		unsigned INPUT2		:	8;
		unsigned INPUT1		:	8;
		unsigned INPUT0		:	6;
		unsigned ERRLATCH	:	1;		// 0: no error  1: Error latched. Check SISR.
		unsigned ERRSTAT	:	1;		// 0: no error  1: error on last transfer
	};
};

// SI Channel Input Low u32
union USIChannelIn_Lo
{
	u32 Hex;
	struct  
	{
		unsigned INPUT7		:	8;
		unsigned INPUT6		:	8;
		unsigned INPUT5		:	8;
		unsigned INPUT4		:	8;
	};
};

// SI Channel
struct SSIChannel
{
	USIChannelOut	m_Out;		
	USIChannelIn_Hi m_InHi;
	USIChannelIn_Lo m_InLo;
	ISIDevice*		m_pDevice;
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
		unsigned CHANNEL    :	2;		// determines which SI channel will be used the communication interface.
		unsigned			:	5;
		unsigned INLNGTH	:	7;
		unsigned			:	1;
		unsigned OUTLNGTH	:	7;		// Communication Channel Output Length in bytes
		unsigned			:	4;
		unsigned RDSTINTMSK	:	1;		// Read Status Interrupt Status Mask
		unsigned RDSTINT	:	1;		// Read Status Interrupt Status
		unsigned COMERR		:	1;		// Communication Error (set 0)
		unsigned TCINTMSK	:	1;		// Transfer Complete Interrupt Mask
		unsigned TCINT		:	1;		// Transfer Complete Interrupt
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
		unsigned UNRUN3	:	1;		// (RWC) write 1: bit cleared  read 1 main proc underrun error
		unsigned OVRUN3	:	1;		// (RWC) write 1: bit cleared  read 1 overrun error
		unsigned COLL3	:	1;		// (RWC) write 1: bit cleared  read 1 collision error
		unsigned NOREP3	:	1;		// (RWC) write 1: bit cleared  read 1 response error
		unsigned WRST3	:	1;		// (R) 1: buffer channel0 not copied
		unsigned RDST3	:	1;		// (R) 1: new Data available
		unsigned		:	2;		// 7:6
		unsigned UNRUN2	:	1;		// (RWC) write 1: bit cleared  read 1 main proc underrun error
		unsigned OVRUN2	:	1;		// (RWC) write 1: bit cleared  read 1 overrun error
		unsigned COLL2	:	1;		// (RWC) write 1: bit cleared  read 1 collision error
		unsigned NOREP2	:	1;		// (RWC) write 1: bit cleared  read 1 response error
		unsigned WRST2	:	1;		// (R) 1: buffer channel0 not copied
		unsigned RDST2	:	1;		// (R) 1: new Data available
		unsigned		:	2;		// 15:14
		unsigned UNRUN1	:	1;		// (RWC) write 1: bit cleared  read 1 main proc underrun error
		unsigned OVRUN1	:	1;		// (RWC) write 1: bit cleared  read 1 overrun error
		unsigned COLL1	:	1;		// (RWC) write 1: bit cleared  read 1 collision error
		unsigned NOREP1	:	1;		// (RWC) write 1: bit cleared  read 1 response error
		unsigned WRST1	:	1;		// (R) 1: buffer channel0 not copied
		unsigned RDST1	:	1;		// (R) 1: new Data available
		unsigned		:	2;		// 23:22
		unsigned UNRUN0	:	1;		// (RWC) write 1: bit cleared  read 1 main proc underrun error
		unsigned OVRUN0	:	1;		// (RWC) write 1: bit cleared  read 1 overrun error
		unsigned COLL0	:	1;		// (RWC) write 1: bit cleared  read 1 collision error
		unsigned NOREP0	:	1;		// (RWC) write 1: bit cleared  read 1 response error
		unsigned WRST0	:	1;		// (R) 1: buffer channel0 not copied
		unsigned RDST0	:	1;		// (R) 1: new Data available
		unsigned		:	1;
		unsigned WR		:	1;		// (RW) write 1 start copy, read 0 copy done
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

SSIChannel			g_Channel[NUMBER_OF_CHANNELS];
USIPoll				g_Poll;
USIComCSR			g_ComCSR;
USIStatusReg		g_StatusReg;
USIEXIClockCount	g_EXIClockCount;
u8					g_SIBuffer[128];

static void GenerateSIInterrupt(SIInterruptType _SIInterrupt);	
void RunSIBuffer();
void UpdateInterrupts();		

void Init()
{
	for (int i = 0; i < NUMBER_OF_CHANNELS; i++)
	{
		g_Channel[i].m_Out.Hex = 0;
		g_Channel[i].m_InHi.Hex = 0;
		g_Channel[i].m_InLo.Hex = 0;		
	}	

	g_Channel[0].m_pDevice = new CSIDevice_GCController(0);
	g_Channel[1].m_pDevice = new CSIDevice_Dummy(1);//new CSIDevice_GCController(1);
	g_Channel[2].m_pDevice = new CSIDevice_Dummy(2);//new CSIDevice_GCController(2);
	g_Channel[3].m_pDevice = new CSIDevice_Dummy(3);//new CSIDevice_GCController(3);

	g_Poll.Hex = 0;
	g_ComCSR.Hex = 0;
	g_StatusReg.Hex = 0;
	g_EXIClockCount.Hex = 0;
	memset(g_SIBuffer, 0xce, 128);
}

void Shutdown()
{
	for (int i=0; i<NUMBER_OF_CHANNELS; i++)
	{
		delete g_Channel[i].m_pDevice;
		g_Channel[i].m_pDevice = NULL;
	}
}

void Read32(u32& _uReturnValue, const u32 _iAddress)
{
	LOG(SERIALINTERFACE, "(r32): 0x%08x", _iAddress);

	// SIBuffer
	if ((_iAddress >= 0xCC006480 && _iAddress < 0xCC006500) ||
		(_iAddress >= 0xCD006480 && _iAddress < 0xCD006500))
	{
		_uReturnValue = *(u32*)&g_SIBuffer[_iAddress & 0x7F];
		return;
	}

	// registers
	switch (_iAddress & 0x3FF)
	{
		//===================================================================================================
		// Channel 0
		//===================================================================================================
		case SI_CHANNEL_0_OUT:		
			_uReturnValue = g_Channel[0].m_Out.Hex;
			return;

		case SI_CHANNEL_0_IN_HI:	
			g_StatusReg.RDST0 = 0;
			UpdateInterrupts();
			_uReturnValue = g_Channel[0].m_InHi.Hex;
			return;

		case SI_CHANNEL_0_IN_LO:	
			g_StatusReg.RDST0 = 0;
			UpdateInterrupts();
			_uReturnValue = g_Channel[0].m_InLo.Hex;
			return;

		//===================================================================================================
		// Channel 1
		//===================================================================================================
		case SI_CHANNEL_1_OUT:		
			_uReturnValue = g_Channel[1].m_Out.Hex;
			return;

		case SI_CHANNEL_1_IN_HI:	
			g_StatusReg.RDST1 = 0;
			UpdateInterrupts();
			_uReturnValue = g_Channel[1].m_InHi.Hex;
			return;

		case SI_CHANNEL_1_IN_LO:	
			g_StatusReg.RDST1 = 0;
			UpdateInterrupts();
			_uReturnValue = g_Channel[1].m_InLo.Hex;
			return;

		//===================================================================================================
		// Channel 2
		//===================================================================================================
		case SI_CHANNEL_2_OUT:		
			_uReturnValue = g_Channel[2].m_Out.Hex;
			return;

		case SI_CHANNEL_2_IN_HI:	
			g_StatusReg.RDST2 = 0;
			UpdateInterrupts();
			_uReturnValue = g_Channel[2].m_InHi.Hex;
			return;

		case SI_CHANNEL_2_IN_LO:	
			g_StatusReg.RDST2 = 0;
			UpdateInterrupts();
			_uReturnValue = g_Channel[2].m_InLo.Hex;
			return;

		//===================================================================================================
		// Channel 3
		//===================================================================================================
		case SI_CHANNEL_3_OUT:		
			_uReturnValue = g_Channel[3].m_Out.Hex;
			return;

		case SI_CHANNEL_3_IN_HI:
			g_StatusReg.RDST3 = 0;
			UpdateInterrupts();
			_uReturnValue = g_Channel[3].m_InHi.Hex;
			return;

		case SI_CHANNEL_3_IN_LO:	
			g_StatusReg.RDST3 = 0;
			UpdateInterrupts();
			_uReturnValue = g_Channel[3].m_InLo.Hex;
			return;

		case SI_POLL:				_uReturnValue = g_Poll.Hex; return;
		case SI_COM_CSR:			_uReturnValue = g_ComCSR.Hex; return;
		case SI_STATUS_REG:			_uReturnValue = g_StatusReg.Hex; return;

		case SI_EXI_CLOCK_COUNT:	_uReturnValue = g_EXIClockCount.Hex; return;

		default:
			LOG(SERIALINTERFACE, "(r32-unk): 0x%08x", _iAddress);
			_dbg_assert_(SERIALINTERFACE,0);			
			break;
	}	

	// error
	_uReturnValue = 0xdeadbeef;
}

void Write32(const u32 _iValue, const u32 _iAddress)
{
	LOG(SERIALINTERFACE, "(w32): 0x%08x 0x%08x", _iValue,_iAddress);

	// SIBuffer
	if ((_iAddress >= 0xCC006480 && _iAddress < 0xCC006500) ||
		(_iAddress >= 0xCD006480 && _iAddress < 0xCD006500))
	{
		*(u32*)&g_SIBuffer[_iAddress & 0x7F] = _iValue;
		return;
	}

	// registers
	switch (_iAddress & 0x3FF)
	{
		case SI_CHANNEL_0_OUT:		g_Channel[0].m_Out.Hex = _iValue; break;
		case SI_CHANNEL_0_IN_HI:	g_Channel[0].m_InHi.Hex = _iValue; break;
		case SI_CHANNEL_0_IN_LO:	g_Channel[0].m_InLo.Hex = _iValue; break;
		case SI_CHANNEL_1_OUT:		g_Channel[1].m_Out.Hex = _iValue; break;
		case SI_CHANNEL_1_IN_HI:	g_Channel[1].m_InHi.Hex = _iValue; break;
		case SI_CHANNEL_1_IN_LO:	g_Channel[1].m_InLo.Hex = _iValue; break;
		case SI_CHANNEL_2_OUT:		g_Channel[2].m_Out.Hex = _iValue; break;
		case SI_CHANNEL_2_IN_HI:	g_Channel[2].m_InHi.Hex = _iValue; break;
		case SI_CHANNEL_2_IN_LO:	g_Channel[2].m_InLo.Hex = _iValue; break;
		case SI_CHANNEL_3_OUT:		g_Channel[3].m_Out.Hex = _iValue; break;
		case SI_CHANNEL_3_IN_HI:	g_Channel[3].m_InHi.Hex = _iValue; break;
		case SI_CHANNEL_3_IN_LO:	g_Channel[3].m_InLo.Hex = _iValue; break;

		case SI_POLL:				
			g_Poll.Hex = _iValue; 
			break;

		case SI_COM_CSR:			
			{
				USIComCSR tmpComCSR(_iValue);
				
				g_ComCSR.CHANNEL	= tmpComCSR.CHANNEL;
				g_ComCSR.INLNGTH	= tmpComCSR.INLNGTH;
				g_ComCSR.OUTLNGTH	= tmpComCSR.OUTLNGTH;
				g_ComCSR.RDSTINTMSK = tmpComCSR.RDSTINTMSK;
				g_ComCSR.TCINTMSK	= tmpComCSR.TCINTMSK;

				g_ComCSR.COMERR		= 0;
								
				if (tmpComCSR.RDSTINT)	g_ComCSR.RDSTINT = 0;
				if (tmpComCSR.TCINT)	g_ComCSR.TCINT = 0;

				// be careful: runsi-buffer after updating the INT flags
				if (tmpComCSR.TSTART)	RunSIBuffer();
				UpdateInterrupts();
			}
			break;

		case SI_STATUS_REG:	
			{
				USIStatusReg tmpStatus(_iValue);
				
				// just update the writable bits
				g_StatusReg.NOREP0	= tmpStatus.NOREP0 ? 1 : 0;
				g_StatusReg.COLL0	= tmpStatus.COLL0 ? 1 : 0;
				g_StatusReg.OVRUN0	= tmpStatus.OVRUN0 ? 1 : 0;
				g_StatusReg.UNRUN0	= tmpStatus.UNRUN0 ? 1 : 0;

				g_StatusReg.NOREP1	= tmpStatus.NOREP1 ? 1 : 0;
				g_StatusReg.COLL1	= tmpStatus.COLL1 ? 1 : 0;
				g_StatusReg.OVRUN1	= tmpStatus.OVRUN1 ? 1 : 0;
				g_StatusReg.UNRUN1	= tmpStatus.UNRUN1 ? 1 : 0;

				g_StatusReg.NOREP2	= tmpStatus.NOREP2 ? 1 : 0;
				g_StatusReg.COLL2	= tmpStatus.COLL2 ? 1 : 0;
				g_StatusReg.OVRUN2	= tmpStatus.OVRUN2 ? 1 : 0;
				g_StatusReg.UNRUN2	= tmpStatus.UNRUN2 ? 1 : 0;

				g_StatusReg.NOREP3	= tmpStatus.NOREP3 ? 1 : 0;
				g_StatusReg.COLL3	= tmpStatus.COLL3 ? 1 : 0;
				g_StatusReg.OVRUN3	= tmpStatus.OVRUN3 ? 1 : 0;
				g_StatusReg.UNRUN3	= tmpStatus.UNRUN3 ? 1 : 0;

				// send command to devices
				if (tmpStatus.WR)
				{
					g_StatusReg.WR = 0;
					g_Channel[0].m_pDevice->SendCommand(g_Channel[0].m_Out.Hex);
					g_Channel[1].m_pDevice->SendCommand(g_Channel[1].m_Out.Hex);
					g_Channel[2].m_pDevice->SendCommand(g_Channel[2].m_Out.Hex);
					g_Channel[3].m_pDevice->SendCommand(g_Channel[3].m_Out.Hex);

					g_StatusReg.WRST0 = 0;
					g_StatusReg.WRST1 = 0;
					g_StatusReg.WRST2 = 0;
					g_StatusReg.WRST3 = 0;
				}
			}
			break;

		case SI_EXI_CLOCK_COUNT:	
			g_EXIClockCount.Hex = _iValue; 
			break;

		case 0x80:
			LOG(SERIALINTERFACE, "WII something at 0xCD006480");
			break;

		default:
			_dbg_assert_(SERIALINTERFACE,0);
			break;
	}	
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
	// update channels
	g_StatusReg.RDST0 = g_Channel[0].m_pDevice->GetData(g_Channel[0].m_InHi.Hex, g_Channel[0].m_InLo.Hex) ? 1 : 0;
	g_StatusReg.RDST1 = g_Channel[1].m_pDevice->GetData(g_Channel[1].m_InHi.Hex, g_Channel[1].m_InLo.Hex) ? 1 : 0;
	g_StatusReg.RDST2 = g_Channel[2].m_pDevice->GetData(g_Channel[2].m_InHi.Hex, g_Channel[2].m_InLo.Hex) ? 1 : 0;
	g_StatusReg.RDST3 = g_Channel[3].m_pDevice->GetData(g_Channel[3].m_InHi.Hex, g_Channel[3].m_InLo.Hex) ? 1 : 0;

	// update interrupts
	UpdateInterrupts();
}

void RunSIBuffer()
{	
	// math inLength
	int inLength = g_ComCSR.INLNGTH;
	if (inLength == 0) 
		inLength = 128;
	else 
		inLength++;

	// math outLength
	int outLength = g_ComCSR.OUTLNGTH;
	if (outLength == 0) 
		outLength = 128;
	else 
		outLength++;

	int numOutput = g_Channel[g_ComCSR.CHANNEL].m_pDevice->RunBuffer(g_SIBuffer, inLength);

	LOG(SERIALINTERFACE, "RunSIBuffer     (intLen: %i    outLen: %i) (processed: %i)", inLength, outLength, numOutput);

	// Transfer completed
	GenerateSIInterrupt(INT_TCINT);
	g_ComCSR.TSTART = 0;
}

} // end of namespace SerialInterface

