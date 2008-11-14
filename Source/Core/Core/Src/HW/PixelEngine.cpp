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

#include "PixelEngine.h"

#include "../CoreTiming.h"
#include "../PowerPC/PowerPC.h"
#include "PeripheralInterface.h"
#include "CommandProcessor.h"
#include "CPU.h"
#include "../Core.h"

namespace PixelEngine
{

// internal hardware addresses
enum
{
	CTRL_REGISTER	= 0x00a,
	TOKEN_REG		= 0x00e,
};

// fifo Control Register
union UPECtrlReg
{
	struct 
	{
		unsigned PETokenEnable	:	1;
		unsigned PEFinishEnable	:	1;
		unsigned PEToken		:	1; // write only
		unsigned PEFinish		:	1; // write only
		unsigned				:	12;
	};
	u16 Hex;
	UPECtrlReg() {Hex = 0; }
	UPECtrlReg(u16 _hex) {Hex = _hex; }
};

// STATE_TO_SAVE
static UPECtrlReg g_ctrlReg;
static u16        g_token = 0;		

static bool       g_bSignalTokenInterrupt;
static bool       g_bSignalFinishInterrupt;

int et_SetTokenOnMainThread;
int et_SetFinishOnMainThread;

void DoState(PointerWrap &p)
{
	p.Do(g_ctrlReg);
	p.Do(g_token);
	p.Do(g_bSignalTokenInterrupt);
	p.Do(g_bSignalFinishInterrupt);
}

void UpdateInterrupts();

void SetToken_OnMainThread(u64 userdata, int cyclesLate);
void SetFinish_OnMainThread(u64 userdata, int cyclesLate);

void Init()
{
	g_token = 0;
	g_ctrlReg.Hex = 0;

	et_SetTokenOnMainThread = CoreTiming::RegisterEvent("SetToken", SetToken_OnMainThread);
	et_SetFinishOnMainThread = CoreTiming::RegisterEvent("SetFinish", SetFinish_OnMainThread);
}

void Read16(u16& _uReturnValue, const u32 _iAddress)
{
	LOGV(PIXELENGINE, 3, "(r16): 0x%08x", _iAddress);

	switch (_iAddress & 0xFFF)
	{
	case CTRL_REGISTER:
		_uReturnValue = g_ctrlReg.Hex;
		return;

	case TOKEN_REG:
		_uReturnValue = g_token;
		return;

	default:
		LOG(PIXELENGINE,"(unknown)");
		break;
	}

	_uReturnValue = 0x001;
}

void Write32(const u32 _iValue, const u32 _iAddress)
{
	LOGV(PIXELENGINE, 2, "(w32): 0x%08x @ 0x%08x",_iValue,_iAddress);
}

void Write16(const u16 _iValue, const u32 _iAddress)
{
	LOGV(PIXELENGINE, 3, "(w16): 0x%04x @ 0x%08x",_iValue,_iAddress);

	switch(_iAddress & 0xFFF)
	{
	case CTRL_REGISTER:	
		{
			UPECtrlReg tmpCtrl(_iValue);

			if (tmpCtrl.PEToken)	g_bSignalTokenInterrupt = false;
			if (tmpCtrl.PEFinish)	g_bSignalFinishInterrupt = false;

			g_ctrlReg.PETokenEnable		= tmpCtrl.PETokenEnable;
			g_ctrlReg.PEFinishEnable	= tmpCtrl.PEFinishEnable;
			g_ctrlReg.PEToken = 0;		// this flag is write only
			g_ctrlReg.PEFinish = 0;		// this flag is write only

			UpdateInterrupts();
		}
		break;

	case TOKEN_REG:
		LOG(PIXELENGINE,"WEIRD: program wrote token: %i",_iValue);
		//only the gx pipeline is supposed to be able to write here
		g_token = _iValue;
		break;
	}
}

bool AllowIdleSkipping()
{
	return !Core::g_CoreStartupParameter.bUseDualCore || (!g_ctrlReg.PETokenEnable && !g_ctrlReg.PEFinishEnable);
}

void UpdateInterrupts()
{
	// check if there is a token-interrupt
	if (g_bSignalTokenInterrupt	& g_ctrlReg.PETokenEnable)
		CPeripheralInterface::SetInterrupt(CPeripheralInterface::INT_CAUSE_PE_TOKEN, true);
	else
		CPeripheralInterface::SetInterrupt(CPeripheralInterface::INT_CAUSE_PE_TOKEN, false);

	// check if there is a finish-interrupt
	if (g_bSignalFinishInterrupt & g_ctrlReg.PEFinishEnable)
		CPeripheralInterface::SetInterrupt(CPeripheralInterface::INT_CAUSE_PE_FINISH, true);
	else
		CPeripheralInterface::SetInterrupt(CPeripheralInterface::INT_CAUSE_PE_FINISH, false);
}

void SetToken_OnMainThread(u64 userdata, int cyclesLate)
{
	// In the future: schedule callback that does the rest of this function
	// This way we will be threadsafe
	if (userdata >> 16)
		g_bSignalTokenInterrupt = true;	
	g_token = (u16)(userdata & 0xFFFF);
	LOGV(PIXELENGINE, 1, "VIDEO Plugin wrote token: %i", g_token);
	UpdateInterrupts();
}

void SetFinish_OnMainThread(u64 userdata, int cyclesLate)
{
	g_bSignalFinishInterrupt = 1;	
	UpdateInterrupts();
	CommandProcessor::IncrementGPWDToken();
}

// SetToken
// THIS IS EXECUTED FROM VIDEO THREAD
void SetToken(const u16 _token, const int _bSetTokenAcknowledge)
{
	CoreTiming::ScheduleEvent_Threadsafe(
		0, et_SetTokenOnMainThread, _token | (_bSetTokenAcknowledge << 16));
}

// SetFinish
// THIS IS EXECUTED FROM VIDEO THREAD
void SetFinish()
{
	CoreTiming::ScheduleEvent_Threadsafe(
		0, et_SetFinishOnMainThread);
	LOGV(PIXELENGINE, 2, "VIDEO Set Finish");
}

} // end of namespace PixelEngine
