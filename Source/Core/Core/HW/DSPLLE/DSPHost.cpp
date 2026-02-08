// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/DSP/DSPHost.h"

#include <string>

#include "Common/CommonTypes.h"
#include "Common/Hash.h"
#include "Common/Logging/Log.h"
#include "Core/Config/MainSettings.h"
#include "Core/DSP/DSPAnalyzer.h"
#include "Core/DSP/DSPCodeUtil.h"
#include "Core/DSP/DSPCore.h"
#include "Core/DSP/Jit/x64/DSPEmitter.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DSPLLE/DSPSymbols.h"
#include "Core/HW/Memmap.h"
#include "Core/Host.h"
#include "Core/System.h"
#include "VideoCommon/OnScreenDisplay.h"

// The user of the DSPCore library must supply a few functions so that the
// emulation core can access the environment it runs in. If the emulation
// core isn't used, for example in an asm/disasm tool, then most of these
// can be stubbed out.

namespace DSP::Host
{
u8 ReadHostMemory(u32 addr)
{
  return Core::System::GetInstance().GetDSP().ReadARAM(addr);
}

void WriteHostMemory(u8 value, u32 addr)
{
  Core::System::GetInstance().GetDSP().WriteARAM(value, addr);
}

void DMAToDSP(u16* dst, u32 addr, u32 size)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  memory.CopyFromEmuSwapped(dst, addr, size);
}

void DMAFromDSP(const u16* src, u32 addr, u32 size)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  memory.CopyToEmuSwapped(addr, src, size);
}

void OSD_AddMessage(std::string str, u32 ms)
{
  OSD::AddMessage(std::move(str), ms);
}

bool OnThread()
{
  return Config::Get(Config::MAIN_DSP_THREAD);
}

bool IsWiiHost()
{
  return Core::System::GetInstance().IsWii();
}

void InterruptRequest()
{
  // Fire an interrupt on the PPC ASAP.
  Core::System::GetInstance().GetDSP().GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
}

void CodeLoaded(DSPCore& dsp, u32 addr, size_t size)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  CodeLoaded(dsp, memory.GetPointerForRange(addr, size), size);
}

void CodeLoaded(DSPCore& dsp, const u8* ptr, size_t size)
{
  auto& state = dsp.DSPState();
  const u32 iram_crc = Common::HashEctor(ptr, size);
  state.SetIRAMCRC(iram_crc);

  if (Config::Get(Config::MAIN_DUMP_UCODE))
  {
    DSP::DumpDSPCode(ptr, size, iram_crc);
  }

  NOTICE_LOG_FMT(DSPLLE, "g_dsp.iram_crc: {:08x}", iram_crc);

  Symbols::Clear();
  Symbols::AutoDisassembly(state, 0x0, 0x1000);
  Symbols::AutoDisassembly(state, 0x8000, 0x9000);

  UpdateDebugger();

  dsp.ClearIRAM();
  state.GetAnalyzer().Analyze(state);
}

void UpdateDebugger()
{
  Host_RefreshDSPDebuggerWindow();
}
}  // namespace DSP::Host
