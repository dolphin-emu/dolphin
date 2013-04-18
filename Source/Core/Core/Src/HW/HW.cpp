// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"
#include "Thunk.h"
#include "../Core.h"
#include "HW.h"
#include "../PowerPC/PowerPC.h"
#include "CPU.h"
#include "DSP.h"
#include "DVDInterface.h"
#include "EXI.h"
#include "GPFifo.h"
#include "Memmap.h"
#include "ProcessorInterface.h"
#include "SI.h"
#include "AudioInterface.h"
#include "VideoInterface.h"
#include "WII_IPC.h"
#include "../ConfigManager.h"
#include "../CoreTiming.h"
#include "SystemTimers.h"
#include "../IPC_HLE/WII_IPC_HLE.h"
#include "../State.h"
#include "../PowerPC/PPCAnalyst.h"

namespace HW
{
	void Init()
	{
		CoreTiming::Init();
		SystemTimers::PreInit();

		State::Init();

		// Init the whole Hardware
		AudioInterface::Init();
		VideoInterface::Init();
		SerialInterface::Init();
		ProcessorInterface::Init();
		Memory::Init();
		DSP::Init(SConfig::GetInstance().m_LocalCoreStartupParameter.bDSPHLE);
		DVDInterface::Init();
		GPFifo::Init();
		ExpansionInterface::Init();
		CCPU::Init(SConfig::GetInstance().m_LocalCoreStartupParameter.iCPUCore);
		SystemTimers::Init();

		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
		{
			WII_IPCInterface::Init();
			WII_IPC_HLE_Interface::Init();
		}
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

		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
		{
			WII_IPCInterface::Shutdown();
			WII_IPC_HLE_Interface::Shutdown();
		}

		State::Shutdown();
		CoreTiming::Shutdown();
	}

	void DoState(PointerWrap &p)
	{
		Memory::DoState(p);
		p.DoMarker("Memory");
		VideoInterface::DoState(p);
		p.DoMarker("VideoInterface");
		SerialInterface::DoState(p);
		p.DoMarker("SerialInterface");
		ProcessorInterface::DoState(p);
		p.DoMarker("ProcessorInterface");
		DSP::DoState(p);
		p.DoMarker("DSP");
		DVDInterface::DoState(p);
		p.DoMarker("DVDInterface");
		GPFifo::DoState(p);
		p.DoMarker("GPFifo");
		ExpansionInterface::DoState(p);
		p.DoMarker("ExpansionInterface");
		AudioInterface::DoState(p);
		p.DoMarker("AudioInterface");

		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
		{
			WII_IPCInterface::DoState(p);
			p.DoMarker("WII_IPCInterface");
			WII_IPC_HLE_Interface::DoState(p);
			p.DoMarker("WII_IPC_HLE_Interface");
		}

		p.DoMarker("WIIHW");
	}
}
