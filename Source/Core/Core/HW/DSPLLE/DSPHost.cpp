// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/DSP/DSPHost.h"

#include <string>

#include "Common/CommonTypes.h"
#include "Common/Hash.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/DSP/DSPAnalyzer.h"
#include "Core/DSP/DSPCodeUtil.h"
#include "Core/DSP/DSPCore.h"
#include "Core/DSP/Jit/x64/DSPEmitter.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DSPLLE/DSPSymbols.h"
#include "Core/Host.h"
#include "VideoCommon/OnScreenDisplay.h"

// The user of the DSPCore library must supply a few functions so that the
// emulation core can access the environment it runs in. If the emulation
// core isn't used, for example in an asm/disasm tool, then most of these
// can be stubbed out.

namespace DSP::Host
{
u8 ReadHostMemory(u32 addr)
{
  return DSP::ReadARAM(addr);
}

void WriteHostMemory(u8 value, u32 addr)
{
  DSP::WriteARAM(value, addr);
}

void OSD_AddMessage(const std::string& str, u32 ms)
{
  OSD::AddMessage(str, ms);
}

bool OnThread()
{
  return SConfig::GetInstance().bDSPThread;
}

bool IsWiiHost()
{
  return SConfig::GetInstance().bWii;
}

void InterruptRequest()
{
  // Fire an interrupt on the PPC ASAP.
  DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
}

void CodeLoaded(const u8* ptr, int size)
{
  if (SConfig::GetInstance().m_DumpUCode)
  {
    DSP::DumpDSPCode(ptr, size, g_dsp.iram_crc);
  }

  NOTICE_LOG(DSPLLE, "g_dsp.iram_crc: %08x", g_dsp.iram_crc);

  Symbols::Clear();
  Symbols::AutoDisassembly(0x0, 0x1000);
  Symbols::AutoDisassembly(0x8000, 0x9000);

  UpdateDebugger();

  if (g_dsp_jit)
    g_dsp_jit->ClearIRAM();

  Analyzer::Analyze();
}

void UpdateDebugger()
{
  Host_RefreshDSPDebuggerWindow();
}
}  // namespace DSP::Host
