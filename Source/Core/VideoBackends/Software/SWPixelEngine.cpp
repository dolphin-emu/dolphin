// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// http://developer.nvidia.com/object/General_FAQ.html#t6 !!!!!

#include "Common/ChunkFile.h"
#include "Common/Common.h"

#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/ProcessorInterface.h"

#include "VideoBackends/Software/SWCommandProcessor.h"
#include "VideoBackends/Software/SWPixelEngine.h"


namespace SWPixelEngine
{

enum
{
	INT_CAUSE_PE_TOKEN  =  0x200, // GP Token
	INT_CAUSE_PE_FINISH =  0x400, // GP Finished
};

// STATE_TO_SAVE
PEReg pereg;

static bool g_bSignalTokenInterrupt;
static bool g_bSignalFinishInterrupt;

static int et_SetTokenOnMainThread;
static int et_SetFinishOnMainThread;

void DoState(PointerWrap &p)
{
	p.DoPOD(pereg);
	p.Do(g_bSignalTokenInterrupt);
	p.Do(g_bSignalFinishInterrupt);
	p.Do(et_SetTokenOnMainThread);
	p.Do(et_SetFinishOnMainThread);
}

void UpdateInterrupts();

void SetToken_OnMainThread(u64 userdata, int cyclesLate);
void SetFinish_OnMainThread(u64 userdata, int cyclesLate);

void Init()
{
	memset(&pereg, 0, sizeof(pereg));

	et_SetTokenOnMainThread = false;
	g_bSignalFinishInterrupt = false;

	et_SetTokenOnMainThread = CoreTiming::RegisterEvent("SetToken", SetToken_OnMainThread);
	et_SetFinishOnMainThread = CoreTiming::RegisterEvent("SetFinish", SetFinish_OnMainThread);
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
	// Directly map reads and writes to the pereg structure.
	for (size_t i = 0; i < sizeof (pereg) / sizeof (u16); ++i)
	{
		u16* ptr = (u16*)&pereg + i;
		mmio->Register(base | (i * 2),
			MMIO::DirectRead<u16>(ptr),
			MMIO::DirectWrite<u16>(ptr)
		);
	}

	// The control register has some more complex logic to perform on writes.
	mmio->RegisterWrite(base | PE_CTRL_REGISTER,
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			UPECtrlReg tmpCtrl(val);

			if (tmpCtrl.PEToken)  g_bSignalTokenInterrupt = false;
			if (tmpCtrl.PEFinish) g_bSignalFinishInterrupt = false;

			pereg.ctrl.PETokenEnable = tmpCtrl.PETokenEnable;
			pereg.ctrl.PEFinishEnable = tmpCtrl.PEFinishEnable;
			pereg.ctrl.PEToken = 0;  // this flag is write only
			pereg.ctrl.PEFinish = 0; // this flag is write only

			UpdateInterrupts();
		})
	);
}

bool AllowIdleSkipping()
{
	return !SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread || (!pereg.ctrl.PETokenEnable && !pereg.ctrl.PEFinishEnable);
}

void UpdateInterrupts()
{
	// check if there is a token-interrupt
	if (g_bSignalTokenInterrupt & pereg.ctrl.PETokenEnable)
		ProcessorInterface::SetInterrupt(INT_CAUSE_PE_TOKEN, true);
	else
		ProcessorInterface::SetInterrupt(INT_CAUSE_PE_TOKEN, false);

	// check if there is a finish-interrupt
	if (g_bSignalFinishInterrupt & pereg.ctrl.PEFinishEnable)
		ProcessorInterface::SetInterrupt(INT_CAUSE_PE_FINISH, true);
	else
		ProcessorInterface::SetInterrupt(INT_CAUSE_PE_FINISH, false);
}


// Called only if BPMEM_PE_TOKEN_INT_ID is ack by GP
void SetToken_OnMainThread(u64 userdata, int cyclesLate)
{
	g_bSignalTokenInterrupt = true;
	INFO_LOG(PIXELENGINE, "VIDEO Backend raises INT_CAUSE_PE_TOKEN (btw, token: %04x)", pereg.token);
	UpdateInterrupts();
}

void SetFinish_OnMainThread(u64 userdata, int cyclesLate)
{
	g_bSignalFinishInterrupt = true;
	UpdateInterrupts();
}

// SetToken
// THIS IS EXECUTED FROM VIDEO THREAD
void SetToken(const u16 _token, const int _bSetTokenAcknowledge)
{
	pereg.token = _token;
	if (_bSetTokenAcknowledge) // set token INT
	{
		CoreTiming::ScheduleEvent_Threadsafe(0, et_SetTokenOnMainThread,
			_token | (_bSetTokenAcknowledge << 16));
	}
}

// SetFinish
// THIS IS EXECUTED FROM VIDEO THREAD
void SetFinish()
{
	CoreTiming::ScheduleEvent_Threadsafe(0, et_SetFinishOnMainThread, 0);
	INFO_LOG(PIXELENGINE, "VIDEO Set Finish");
}

} // end of namespace SWPixelEngine
