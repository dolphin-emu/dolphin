// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"
#include "ChunkFile.h"
#include "../ConfigManager.h"
#include "../CoreTiming.h"
#include "../Movie.h"
#include "../NetPlayClient.h"

#include "SystemTimers.h"
#include "ProcessorInterface.h"
#include "VideoInterface.h"

#include "SI.h"
#include "SI_DeviceGBA.h"

namespace SerialInterface
{

static int changeDevice;

void RunSIBuffer();
void UpdateInterrupts();

// SI Interrupt Types
enum SIInterruptType
{
	INT_RDSTINT		= 0,
	INT_TCINT		= 1,
};
static void GenerateSIInterrupt(SIInterruptType _SIInterrupt);

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
		u32 OUTPUT1	:	8;
		u32 OUTPUT0	:	8;
		u32 CMD		:	8;
		u32			:	8;
	};
};

// SI Channel Input High u32
union USIChannelIn_Hi
{
	u32 Hex;
	struct
	{
		u32 INPUT3		:	8;
		u32 INPUT2		:	8;
		u32 INPUT1		:	8;
		u32 INPUT0		:	6;
		u32 ERRLATCH	:	1; // 0: no error  1: Error latched. Check SISR.
		u32 ERRSTAT		:	1; // 0: no error  1: error on last transfer
	};
};

// SI Channel Input Low u32
union USIChannelIn_Lo
{
	u32 Hex;
	struct
	{
		u32 INPUT7		:	8;
		u32 INPUT6		:	8;
		u32 INPUT5		:	8;
		u32 INPUT4		:	8;
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
		u32 VBCPY3	:	1;  // 1: write to output buffer only on vblank
		u32 VBCPY2	:	1;
		u32 VBCPY1	:	1;
		u32 VBCPY0	:	1;
		u32 EN3		:	1;  // Enable polling of channel
		u32 EN2		:	1;  //  does not affect communication RAM transfers
		u32 EN1		:	1;
		u32 EN0		:	1;
		u32 Y		:	8;  // Polls per frame
		u32 X		:	10; // Polls per X lines. begins at vsync, min 7, max depends on video mode
		u32			:	6;
	};
};

// SI Communication Control Status Register
union USIComCSR
{
	u32 Hex;
	struct
	{
		u32 TSTART		:	1; // write: start transfer  read: transfer status
		u32 CHANNEL		:	2; // determines which SI channel will be used on the communication interface.
		u32				:	3;
		u32 CALLBEN		:	1; // Callback enable
		u32 CMDEN		:	1; // Command enable?
		u32 INLNGTH		:	7;
		u32				:	1;
		u32 OUTLNGTH	:	7; // Communication Channel Output Length in bytes
		u32				:	1;
		u32 CHANEN		:	1; // Channel enable?
		u32 CHANNUM		:	2; // Channel number?
		u32 RDSTINTMSK	:	1; // Read Status Interrupt Status Mask
		u32 RDSTINT		:	1; // Read Status Interrupt Status
		u32 COMERR		:	1; // Communication Error (set 0)
		u32 TCINTMSK	:	1; // Transfer Complete Interrupt Mask
		u32 TCINT		:	1; // Transfer Complete Interrupt
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
		u32 UNRUN3	:	1; // (RWC) write 1: bit cleared  read 1: main proc underrun error
		u32 OVRUN3	:	1; // (RWC) write 1: bit cleared  read 1: overrun error
		u32 COLL3	:	1; // (RWC) write 1: bit cleared  read 1: collision error
		u32 NOREP3	:	1; // (RWC) write 1: bit cleared  read 1: response error
		u32 WRST3	:	1; // (R) 1: buffer channel0 not copied
		u32 RDST3	:	1; // (R) 1: new Data available
		u32			:	2; // 7:6
		u32 UNRUN2	:	1; // (RWC) write 1: bit cleared  read 1: main proc underrun error
		u32 OVRUN2	:	1; // (RWC) write 1: bit cleared  read 1: overrun error
		u32 COLL2	:	1; // (RWC) write 1: bit cleared  read 1: collision error
		u32 NOREP2	:	1; // (RWC) write 1: bit cleared  read 1: response error
		u32 WRST2	:	1; // (R) 1: buffer channel0 not copied
		u32 RDST2	:	1; // (R) 1: new Data available
		u32			:	2; // 15:14
		u32 UNRUN1	:	1; // (RWC) write 1: bit cleared  read 1: main proc underrun error
		u32 OVRUN1	:	1; // (RWC) write 1: bit cleared  read 1: overrun error
		u32 COLL1	:	1; // (RWC) write 1: bit cleared  read 1: collision error
		u32 NOREP1	:	1; // (RWC) write 1: bit cleared  read 1: response error
		u32 WRST1	:	1; // (R) 1: buffer channel0 not copied
		u32 RDST1	:	1; // (R) 1: new Data available
		u32			:	2; // 23:22
		u32 UNRUN0	:	1; // (RWC) write 1: bit cleared  read 1: main proc underrun error
		u32 OVRUN0	:	1; // (RWC) write 1: bit cleared  read 1: overrun error
		u32 COLL0	:	1; // (RWC) write 1: bit cleared  read 1: collision error
		u32 NOREP0	:	1; // (RWC) write 1: bit cleared  read 1: response error
		u32 WRST0	:	1; // (R) 1: buffer channel0 not copied
		u32 RDST0	:	1; // (R) 1: new Data available
		u32			:	1;
		u32 WR		:	1; // (RW) write 1 start copy, read 0 copy done
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
		u32 LOCK	:	1; // 1: prevents CPU from setting EXI clock to 32MHz
		u32			:	0;
	};
};

// STATE_TO_SAVE
static SSIChannel			g_Channel[NUMBER_OF_CHANNELS];
static USIPoll				g_Poll;
static USIComCSR			g_ComCSR;
static USIStatusReg			g_StatusReg;
static USIEXIClockCount		g_EXIClockCount;
static u8					g_SIBuffer[128];

void DoState(PointerWrap &p)
{
	for(int i = 0; i < NUMBER_OF_CHANNELS; i++)
	{
		p.Do(g_Channel[i].m_InHi.Hex);
		p.Do(g_Channel[i].m_InLo.Hex);
		p.Do(g_Channel[i].m_Out.Hex);
		
		ISIDevice* pDevice = g_Channel[i].m_pDevice;
		SIDevices type = pDevice->GetDeviceType();
		p.Do(type);
		ISIDevice* pSaveDevice = (type == pDevice->GetDeviceType()) ? pDevice : SIDevice_Create(type, i);
		pSaveDevice->DoState(p);
		if(pSaveDevice != pDevice)
		{
			// if we had to create a temporary device, discard it if we're not loading.
			// also, if no movie is active, we'll assume the user wants to keep their current devices
			// instead of the ones they had when the savestate was created.
			if(p.GetMode() != PointerWrap::MODE_READ ||
				(!Movie::IsRecordingInput() && !Movie::IsPlayingInput()))
			{
				delete pSaveDevice;
			}
			else
			{
				AddDevice(pSaveDevice);
			}
		}
	}
	p.Do(g_Poll);
	p.DoPOD(g_ComCSR);
	p.DoPOD(g_StatusReg);
	p.Do(g_EXIClockCount);
	p.Do(g_SIBuffer);
}	


void Init()
{
	for (int i = 0; i < NUMBER_OF_CHANNELS; i++)
	{	
		g_Channel[i].m_Out.Hex = 0;
		g_Channel[i].m_InHi.Hex = 0;
		g_Channel[i].m_InLo.Hex = 0;

		if (Movie::IsRecordingInput() || Movie::IsPlayingInput())
			AddDevice(Movie::IsUsingPad(i) ?  (Movie::IsUsingBongo(i) ? SIDEVICE_GC_TARUKONGA : SIDEVICE_GC_CONTROLLER) : SIDEVICE_NONE, i);
		else if (NetPlay::IsNetPlayRunning())
			AddDevice((SIDevices) g_NetPlaySettings.m_Controllers[i], i);
		else
			AddDevice(SConfig::GetInstance().m_SIDevice[i], i);
	}

	g_Poll.Hex = 0;
	g_Poll.X = 7;

	g_ComCSR.Hex = 0;

	g_StatusReg.Hex = 0;

	g_EXIClockCount.Hex = 0;
	//g_EXIClockCount.LOCK = 1; // Supposedly set on reset, but logs from real wii don't look like it is...
	memset(g_SIBuffer, 0, 128);

	changeDevice = CoreTiming::RegisterEvent("ChangeSIDevice", ChangeDeviceCallback);
}

void Shutdown()
{
	for (int i = 0; i < NUMBER_OF_CHANNELS; i++)
		RemoveDevice(i);
	GBAConnectionWaiter_Shutdown();
}

void Read32(u32& _uReturnValue, const u32 _iAddress)
{
	// SIBuffer
	if ((_iAddress >= 0xCC006480 && _iAddress < 0xCC006500) ||
		(_iAddress >= 0xCD006480 && _iAddress < 0xCD006500))
	{
		_uReturnValue = *(u32*)&g_SIBuffer[_iAddress & 0x7F];
		return;
	}

	// error if not changed in the switch
	_uReturnValue = 0xdeadbeef;

	// registers
	switch (_iAddress & 0x3FF)
	{
	//////////////////////////////////////////////////////////////////////////
	// Channel 0
	//////////////////////////////////////////////////////////////////////////
	case SI_CHANNEL_0_OUT:
		_uReturnValue = g_Channel[0].m_Out.Hex;
		break;

	case SI_CHANNEL_0_IN_HI:
		g_StatusReg.RDST0 = 0;
		UpdateInterrupts();
		_uReturnValue = g_Channel[0].m_InHi.Hex;
		break;

	case SI_CHANNEL_0_IN_LO:
		g_StatusReg.RDST0 = 0;
		UpdateInterrupts();
		_uReturnValue = g_Channel[0].m_InLo.Hex;
		break;

	//////////////////////////////////////////////////////////////////////////
	// Channel 1
	//////////////////////////////////////////////////////////////////////////
	case SI_CHANNEL_1_OUT:
		_uReturnValue = g_Channel[1].m_Out.Hex;
		break;

	case SI_CHANNEL_1_IN_HI:
		g_StatusReg.RDST1 = 0;
		UpdateInterrupts();
		_uReturnValue = g_Channel[1].m_InHi.Hex;
		break;

	case SI_CHANNEL_1_IN_LO:
		g_StatusReg.RDST1 = 0;
		UpdateInterrupts();
		_uReturnValue = g_Channel[1].m_InLo.Hex;
		break;

	//////////////////////////////////////////////////////////////////////////
	// Channel 2
	//////////////////////////////////////////////////////////////////////////
	case SI_CHANNEL_2_OUT:
		_uReturnValue = g_Channel[2].m_Out.Hex;
		break;

	case SI_CHANNEL_2_IN_HI:
		g_StatusReg.RDST2 = 0;
		UpdateInterrupts();
		_uReturnValue = g_Channel[2].m_InHi.Hex;
		break;

	case SI_CHANNEL_2_IN_LO:
		g_StatusReg.RDST2 = 0;
		UpdateInterrupts();
		_uReturnValue = g_Channel[2].m_InLo.Hex;
		break;

	//////////////////////////////////////////////////////////////////////////
	// Channel 3
	//////////////////////////////////////////////////////////////////////////
	case SI_CHANNEL_3_OUT:
		_uReturnValue = g_Channel[3].m_Out.Hex;
		break;

	case SI_CHANNEL_3_IN_HI:
		g_StatusReg.RDST3 = 0;
		UpdateInterrupts();
		_uReturnValue = g_Channel[3].m_InHi.Hex;
		break;

	case SI_CHANNEL_3_IN_LO:
		g_StatusReg.RDST3 = 0;
		UpdateInterrupts();
		_uReturnValue = g_Channel[3].m_InLo.Hex;
		break;

	//////////////////////////////////////////////////////////////////////////
	// Other
	//////////////////////////////////////////////////////////////////////////
	case SI_POLL:				_uReturnValue = g_Poll.Hex; break;
	case SI_COM_CSR:			_uReturnValue = g_ComCSR.Hex; break;
	case SI_STATUS_REG:			_uReturnValue = g_StatusReg.Hex; break;

	case SI_EXI_CLOCK_COUNT:	_uReturnValue = g_EXIClockCount.Hex; break;

	default:
		INFO_LOG(SERIALINTERFACE, "(r32-unk): 0x%08x", _iAddress);
		_dbg_assert_(SERIALINTERFACE,0);
		break;
	}

	DEBUG_LOG(SERIALINTERFACE, "(r32) 0x%08x - 0x%08x", _iAddress, _uReturnValue);
}

void Write32(const u32 _iValue, const u32 _iAddress)
{
	DEBUG_LOG(SERIALINTERFACE, "(w32) 0x%08x @ 0x%08x", _iValue, _iAddress);

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
		INFO_LOG(SERIALINTERFACE, "Wrote Poll: X=%03d Y=%03d %s%s%s%s%s%s%s%s",
			g_Poll.X, g_Poll.Y,
			g_Poll.EN0 ? "EN0 ":" ", g_Poll.EN1 ? "EN1 ":" ",
			g_Poll.EN2 ? "EN2 ":" ", g_Poll.EN3 ? "EN3 ":" ",
			g_Poll.VBCPY0 ? "VBCPY0 ":" ", g_Poll.VBCPY1 ? "VBCPY1 ":" ",
			g_Poll.VBCPY2 ? "VBCPY2 ":" ", g_Poll.VBCPY3 ? "VBCPY3 ":" ");
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

			// be careful: run si-buffer after updating the INT flags
			if (tmpComCSR.TSTART)	RunSIBuffer();
			UpdateInterrupts();
		}
		break;

	case SI_STATUS_REG:
		{
			USIStatusReg tmpStatus(_iValue);

			// clear bits ( if(tmp.bit) SISR.bit=0 )
			if (tmpStatus.NOREP0)	g_StatusReg.NOREP0 = 0;
			if (tmpStatus.COLL0)	g_StatusReg.COLL0 = 0;
			if (tmpStatus.OVRUN0)	g_StatusReg.OVRUN0 = 0;
			if (tmpStatus.UNRUN0)	g_StatusReg.UNRUN0 = 0;

			if (tmpStatus.NOREP1)	g_StatusReg.NOREP1 = 0;
			if (tmpStatus.COLL1)	g_StatusReg.COLL1 = 0;
			if (tmpStatus.OVRUN1)	g_StatusReg.OVRUN1 = 0;
			if (tmpStatus.UNRUN1)	g_StatusReg.UNRUN1 = 0;

			if (tmpStatus.NOREP2)	g_StatusReg.NOREP2 = 0;
			if (tmpStatus.COLL2)	g_StatusReg.COLL2 = 0;
			if (tmpStatus.OVRUN2)	g_StatusReg.OVRUN2 = 0;
			if (tmpStatus.UNRUN2)	g_StatusReg.UNRUN2 = 0;

			if (tmpStatus.NOREP3)	g_StatusReg.NOREP3 = 0;
			if (tmpStatus.COLL3)	g_StatusReg.COLL3 = 0;
			if (tmpStatus.OVRUN3)	g_StatusReg.OVRUN3 = 0;
			if (tmpStatus.UNRUN3)	g_StatusReg.UNRUN3 = 0;

			// send command to devices
			if (tmpStatus.WR)
			{
				g_Channel[0].m_pDevice->SendCommand(g_Channel[0].m_Out.Hex, g_Poll.EN0);
				g_Channel[1].m_pDevice->SendCommand(g_Channel[1].m_Out.Hex, g_Poll.EN1);
				g_Channel[2].m_pDevice->SendCommand(g_Channel[2].m_Out.Hex, g_Poll.EN2);
				g_Channel[3].m_pDevice->SendCommand(g_Channel[3].m_Out.Hex, g_Poll.EN3);

				g_StatusReg.WR = 0;
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

	case 0x80: // Bogus? never seen it with ma own eyes
		INFO_LOG(SERIALINTERFACE, "WII something at 0xCD006480");
		break;

	default:
		_dbg_assert_(SERIALINTERFACE, 0);
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
		ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_SI, true);
	}
	else
	{
		ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_SI, false);
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

void RemoveDevice(int _iDeviceNumber)
{
	delete g_Channel[_iDeviceNumber].m_pDevice;
	g_Channel[_iDeviceNumber].m_pDevice = NULL;
}

void AddDevice(ISIDevice* pDevice)
{
	int _iDeviceNumber = pDevice->GetDeviceNumber();

	//_dbg_assert_(SERIALINTERFACE, _iDeviceNumber < NUMBER_OF_CHANNELS);

	// delete the old device
	RemoveDevice(_iDeviceNumber);

	// create the new one
	g_Channel[_iDeviceNumber].m_pDevice = pDevice;
}

void AddDevice(const SIDevices _device, int _iDeviceNumber)
{
	ISIDevice* pDevice = SIDevice_Create(_device, _iDeviceNumber);
	AddDevice(pDevice);
}

void SetNoResponse(u32 channel)
{
	// raise the NO RESPONSE error
	switch (channel)
	{
	case 0: g_StatusReg.NOREP0 = 1; break;
	case 1: g_StatusReg.NOREP1 = 1; break;
	case 2: g_StatusReg.NOREP2 = 1; break;
	case 3: g_StatusReg.NOREP3 = 1; break;
	}
	g_ComCSR.COMERR = 1;
}

void ChangeDeviceCallback(u64 userdata, int cyclesLate)
{
	u8 channel = (u8)(userdata >> 32);

	g_Channel[channel].m_Out.Hex = 0;
	g_Channel[channel].m_InHi.Hex = 0;
	g_Channel[channel].m_InLo.Hex = 0;

	SetNoResponse(channel);

	AddDevice((SIDevices)(u32)userdata, channel);
}

void ChangeDevice(SIDevices device, int channel)
{
	// Called from GUI, so we need to make it thread safe.
	// Let the hardware see no device for .5b cycles
	CoreTiming::ScheduleEvent_Threadsafe(0, changeDevice, ((u64)channel << 32) | SIDEVICE_NONE);
	CoreTiming::ScheduleEvent_Threadsafe(500000000, changeDevice, ((u64)channel << 32) | device);
}

void UpdateDevices()
{
	// Update channels and set the status bit if there's new data
	g_StatusReg.RDST0 = !!g_Channel[0].m_pDevice->GetData(g_Channel[0].m_InHi.Hex, g_Channel[0].m_InLo.Hex);
	g_StatusReg.RDST1 = !!g_Channel[1].m_pDevice->GetData(g_Channel[1].m_InHi.Hex, g_Channel[1].m_InLo.Hex);
	g_StatusReg.RDST2 = !!g_Channel[2].m_pDevice->GetData(g_Channel[2].m_InHi.Hex, g_Channel[2].m_InLo.Hex);
	g_StatusReg.RDST3 = !!g_Channel[3].m_pDevice->GetData(g_Channel[3].m_InHi.Hex, g_Channel[3].m_InLo.Hex);

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

	int numOutput = g_Channel[g_ComCSR.CHANNEL].m_pDevice->RunBuffer(g_SIBuffer, inLength);

	DEBUG_LOG(SERIALINTERFACE, "RunSIBuffer     (intLen: %i    outLen: %i) (processed: %i)", inLength, outLength, numOutput);

	// Transfer completed
	GenerateSIInterrupt(INT_TCINT);
	g_ComCSR.TSTART = 0;
}

int GetTicksToNextSIPoll()
{
	// Poll for input at regular intervals (once per frame) when playing or recording a movie
	if (Movie::IsPlayingInput() || Movie::IsRecordingInput())
	{
		return SystemTimers::GetTicksPerSecond() / VideoInterface::TargetRefreshRate;
	}
	if (NetPlay::IsNetPlayRunning())
		return SystemTimers::GetTicksPerSecond() / VideoInterface::TargetRefreshRate / 2;

	if (!g_Poll.Y && g_Poll.X)
		return VideoInterface::GetTicksPerLine() * g_Poll.X;
	else if (!g_Poll.Y)
		return SystemTimers::GetTicksPerSecond() / 60;

	return min(VideoInterface::GetTicksPerFrame() / g_Poll.Y, VideoInterface::GetTicksPerLine() * g_Poll.X);
}

} // end of namespace SerialInterface
