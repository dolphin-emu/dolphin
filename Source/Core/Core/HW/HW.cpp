// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/HW.h"

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/CPU.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/VideoInterface.h"
#include "Core/HW/WII_IPC.h"
#include "Core/IOS/IOS.h"
#include "Core/State.h"
#include "Core/WiiRoot.h"

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
  ExpansionInterface::Init();  // Needs to be initialized before Memory
  Memory::Init();
  DSP::Init(SConfig::GetInstance().bDSPHLE);
  DVDInterface::Init();
  GPFifo::Init();
  CPU::Init(SConfig::GetInstance().cpu_core);
  SystemTimers::Init();

  if (SConfig::GetInstance().bWii)
  {
    // The NAND should only be initialised once per emulation session.
    Core::InitializeWiiRoot(Core::WantsDeterminism());
    IOS::Init();
    IOS::HLE::Init();  // Depends on Memory
  }
}

void Shutdown()
{
  // IOS should always be shut down regardless of bWii because it can be running in GC mode (MIOS).
  IOS::HLE::Shutdown();  // Depends on Memory
  IOS::Shutdown();
  Core::ShutdownWiiRoot();

  SystemTimers::Shutdown();
  CPU::Shutdown();
  DVDInterface::Shutdown();
  DSP::Shutdown();
  Memory::Shutdown();
  ExpansionInterface::Shutdown();
  SerialInterface::Shutdown();
  AudioInterface::Shutdown();

  State::Shutdown();
  CoreTiming::Shutdown();
}

void DoState(PointerWrap& p)
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

  if (SConfig::GetInstance().bWii)
  {
    IOS::DoState(p);
    p.DoMarker("IOS");
    IOS::HLE::GetIOS()->DoState(p);
    p.DoMarker("IOS::HLE");
  }

  p.DoMarker("WIIHW");
}
}  // namespace HW
