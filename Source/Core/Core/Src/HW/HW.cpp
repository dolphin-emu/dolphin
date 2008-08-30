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
#include "Thunk.h"
#include "../Core.h"
#include "HW.h"
#include "../PowerPC/PowerPC.h"
#include "CPU.h"
#include "CommandProcessor.h"
#include "DSP.h"
#include "DVDInterface.h"
#include "EXI.h"
#include "GPFifo.h"
#include "Memmap.h"
#include "PeripheralInterface.h"
#include "PixelEngine.h"
#include "SerialInterface.h"
#include "AudioInterface.h"
#include "VideoInterface.h"
#include "WII_IPC.h"
#include "../Plugins/Plugin_Video.h"
#include "../CoreTiming.h"
#include "SystemTimers.h"
#include "../IPC_HLE/WII_IPC_HLE.h"
#include "../State.h"

#define CURVERSION 0x0001

namespace HW
{
	void Init()
	{
		Thunk_Init(); // not really hw, but this way we know it's inited first :P
		State_Init();

		// Init the whole Hardware
		AudioInterface::Init();
		PixelEngine::Init();
		CommandProcessor::Init();
		VideoInterface::Init();
		SerialInterface::Init();
		CPeripheralInterface::Init();
		Memory::Init();
		DSP::Init();
		DVDInterface::Init();
		GPFifo::Init();
		ExpansionInterface::Init();
		CCPU::Init();
		SystemTimers::Init();

        WII_IPC_HLE_Interface::Init();
        WII_IPCInterface::Init();
	}

	void Shutdown()
	{
		SystemTimers::Shutdown();
		CCPU::Shutdown();
		ExpansionInterface::Shutdown();
		DVDInterface::Shutdown();
		DSP::Shutdown();
		Memory::Shutdown();
		SerialInterface::Shutdown();
		AudioInterface::Shutdown();

        WII_IPC_HLE_Interface::Shutdown();
        WII_IPCInterface::Shutdown();
		
		State_Shutdown();
		Thunk_Shutdown();
		
		CoreTiming::UnregisterAllEvents();
	}

	void DoState(PointerWrap &p)
	{
		Memory::DoState(p);
		PixelEngine::DoState(p);
		CommandProcessor::DoState(p);
		VideoInterface::DoState(p);
		SerialInterface::DoState(p);
		CPeripheralInterface::DoState(p);
		DSP::DoState(p);
		DVDInterface::DoState(p);
		GPFifo::DoState(p);
		ExpansionInterface::DoState(p);
		AudioInterface::DoState(p);
		CoreTiming::DoState(p);
        WII_IPCInterface::DoState(p);
	}
}
