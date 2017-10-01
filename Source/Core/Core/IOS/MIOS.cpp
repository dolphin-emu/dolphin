// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/MIOS.h"

#include <cstring>
#include <utility>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/DSPEmulator.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"

namespace IOS
{
namespace HLE
{
namespace MIOS
{
static void ReinitHardware()
{
  SConfig::GetInstance().bWii = false;

  // IOS clears mem2 and overwrites it with pseudo-random data (for security).
  std::memset(Memory::m_pEXRAM, 0, Memory::EXRAM_SIZE);
  // MIOS appears to only reset the DI and the PPC.
  DVDInterface::Reset();
  PowerPC::Reset();
  // Note: this is specific to Dolphin and is required because we initialised it in Wii mode.
  DSP::Reinit(SConfig::GetInstance().bDSPHLE);
  DSP::GetDSPEmulator()->Initialize(SConfig::GetInstance().bWii, SConfig::GetInstance().bDSPThread);

  SystemTimers::ChangePPCClock(SystemTimers::Mode::GC);
}

constexpr u32 ADDRESS_INIT_SEMAPHORE = 0x30f8;

bool Load()
{
  Memory::Write_U32(0x00000000, ADDRESS_INIT_SEMAPHORE);
  Memory::Write_U32(0x09142001, 0x3180);

  ReinitHardware();
  NOTICE_LOG(IOS, "Reinitialised hardware.");

  // Load symbols for the IPL if they exist.
  g_symbolDB.Clear();
  if (g_symbolDB.LoadMap(File::GetUserPath(D_MAPS_IDX) + "mios-ipl.map"))
  {
    ::HLE::Clear();
    ::HLE::PatchFunctions();
  }

  const PowerPC::CoreMode core_mode = PowerPC::GetMode();
  PowerPC::SetMode(PowerPC::CoreMode::Interpreter);
  MSR = 0;
  PC = 0x3400;
  NOTICE_LOG(IOS, "Loaded MIOS and bootstrapped PPC.");

  // IOS writes 0 to 0x30f8 before bootstrapping the PPC. Once started, the IPL eventually writes
  // 0xdeadbeef there, then waits for it to be cleared by IOS before continuing.
  while (Memory::Read_U32(ADDRESS_INIT_SEMAPHORE) != 0xdeadbeef)
    PowerPC::SingleStep();
  PowerPC::SetMode(core_mode);

  Memory::Write_U32(0x00000000, ADDRESS_INIT_SEMAPHORE);
  NOTICE_LOG(IOS, "IPL ready.");
  SConfig::GetInstance().m_is_mios = true;
  DVDInterface::UpdateRunningGameMetadata();
  return true;
}
}  // namespace MIOS
}  // namespace HLE
}  // namespace IOS
