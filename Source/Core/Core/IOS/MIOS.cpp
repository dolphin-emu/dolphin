// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/MIOS.h"

#include <cstring>
#include <utility>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/DSPEmulator.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/Wiimote.h"
#include "Core/Host.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

namespace IOS::HLE::MIOS
{
static void ReinitHardware(Core::System& system)
{
  system.SetIsWii(false);

  // IOS clears mem2 and overwrites it with pseudo-random data (for security).
  auto& memory = system.GetMemory();
  std::memset(memory.GetEXRAM(), 0, memory.GetExRamSizeReal());
  // MIOS appears to only reset the DI and the PPC.
  // HACK However, resetting DI will reset the DTK config, which is set by the system menu
  // (and not by MIOS), causing games that use DTK to break.  Perhaps MIOS doesn't actually
  // reset DI fully, in such a way that the DTK config isn't cleared?
  // system.GetDVDInterface().ResetDrive(true);
  system.GetPowerPC().Reset();
  Wiimote::ResetAllWiimotes();
  // Note: this is specific to Dolphin and is required because we initialised it in Wii mode.
  auto& dsp = system.GetDSP();
  dsp.Reinit(Config::Get(Config::MAIN_DSP_HLE));
  dsp.GetDSPEmulator()->Initialize(system.IsWii(), Config::Get(Config::MAIN_DSP_THREAD));

  system.GetSystemTimers().ChangePPCClock(SystemTimers::Mode::GC);
}

constexpr u32 ADDRESS_INIT_SEMAPHORE = 0x30f8;

bool Load(Core::System& system)
{
  auto& memory = system.GetMemory();

  ASSERT(Core::IsCPUThread());
  Core::CPUThreadGuard guard(system);

  memory.Write_U32(0x00000000, ADDRESS_INIT_SEMAPHORE);
  memory.Write_U32(0x09142001, 0x3180);

  ReinitHardware(system);
  NOTICE_LOG_FMT(IOS, "Reinitialised hardware.");

  // Load symbols for the IPL if they exist.
  if (!g_symbolDB.IsEmpty())
  {
    g_symbolDB.Clear();
    Host_NotifyMapLoaded();
  }
  if (g_symbolDB.LoadMap(guard, File::GetUserPath(D_MAPS_IDX) + "mios-ipl.map"))
  {
    ::HLE::Clear();
    ::HLE::PatchFunctions(system);
    Host_NotifyMapLoaded();
  }

  auto& power_pc = system.GetPowerPC();

  const PowerPC::CoreMode core_mode = power_pc.GetMode();
  power_pc.SetMode(PowerPC::CoreMode::Interpreter);

  PowerPC::PowerPCState& ppc_state = power_pc.GetPPCState();
  ppc_state.msr.Hex = 0;
  ppc_state.pc = 0x3400;
  PowerPC::MSRUpdated(ppc_state);

  NOTICE_LOG_FMT(IOS, "Loaded MIOS and bootstrapped PPC.");

  // IOS writes 0 to 0x30f8 before bootstrapping the PPC. Once started, the IPL eventually writes
  // 0xdeadbeef there, then waits for it to be cleared by IOS before continuing.
  while (memory.Read_U32(ADDRESS_INIT_SEMAPHORE) != 0xdeadbeef)
    power_pc.SingleStep();
  power_pc.SetMode(core_mode);

  memory.Write_U32(0x00000000, ADDRESS_INIT_SEMAPHORE);
  NOTICE_LOG_FMT(IOS, "IPL ready.");
  system.SetIsMIOS(true);
  system.GetDVDInterface().UpdateRunningGameMetadata();
  SConfig::OnNewTitleLoad(guard);
  return true;
}
}  // namespace IOS::HLE::MIOS
