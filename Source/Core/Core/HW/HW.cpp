// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/State.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/CPU.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/EXI.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/HW.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/VideoInterface.h"
#include "Core/HW/WII_IPC.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCAnalyst.h"

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
		ExpansionInterface::Init(); // Needs to be initialized before Memory
		Memory::Init();
		DSP::Init(SConfig::GetInstance().m_LocalCoreStartupParameter.bDSPHLE);
		DVDInterface::Init();
		GPFifo::Init();
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
