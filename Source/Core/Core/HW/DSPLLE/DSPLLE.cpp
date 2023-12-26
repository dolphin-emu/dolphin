// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/DSPLLE/DSPLLE.h"

#include <mutex>
#include <string>
#include <thread>

#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MemoryUtil.h"
#include "Common/Thread.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/DSP/DSPAccelerator.h"
#include "Core/DSP/DSPCaptureLogger.h"
#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPHost.h"
#include "Core/DSP/DSPTables.h"
#include "Core/DSP/Interpreter/DSPInterpreter.h"
#include "Core/DSP/Jit/DSPEmitterBase.h"
#include "Core/HW/Memmap.h"
#include "Core/Host.h"

namespace DSP::LLE
{
DSPLLE::DSPLLE() = default;

DSPLLE::~DSPLLE()
{
  m_dsp_core.Shutdown();
  DSP_StopSoundStream();
}

void DSPLLE::DoState(PointerWrap& p)
{
  bool is_hle = false;
  p.Do(is_hle);
  if (is_hle && p.IsReadMode())
  {
    Core::DisplayMessage("State is incompatible with current DSP engine. Aborting load state.",
                         3000);
    p.SetVerifyMode();
    return;
  }
  m_dsp_core.DoState(p);
  p.Do(m_cycle_count);
}

// Regular thread
void DSPLLE::DSPThread(DSPLLE* dsp_lle)
{
  Common::SetCurrentThreadName("DSP thread");

  while (dsp_lle->m_is_running.IsSet())
  {
    const int cycles = static_cast<int>(dsp_lle->m_cycle_count.load());
    if (cycles > 0)
    {
      std::unique_lock dsp_thread_lock(dsp_lle->m_dsp_thread_mutex, std::try_to_lock);
      if (dsp_thread_lock)
      {
        if (dsp_lle->m_dsp_core.IsJITCreated())
        {
          dsp_lle->m_dsp_core.RunCycles(cycles);
        }
        else
        {
          dsp_lle->m_dsp_core.GetInterpreter().RunCyclesThread(cycles);
        }
        dsp_lle->m_cycle_count.store(0);
        continue;
      }
    }

    dsp_lle->m_ppc_event.Set();
    dsp_lle->m_dsp_event.Wait();
  }
}

static bool LoadDSPRom(u16* rom, const std::string& filename, u32 size_in_bytes)
{
  std::string bytes;
  if (!File::ReadFileToString(filename, bytes))
    return false;

  if (bytes.size() != size_in_bytes)
  {
    ERROR_LOG_FMT(DSPLLE, "{} has a wrong size ({}, expected {})", filename, bytes.size(),
                  size_in_bytes);
    return false;
  }

  const u16* words = reinterpret_cast<const u16*>(bytes.c_str());
  for (u32 i = 0; i < size_in_bytes / 2; ++i)
    rom[i] = Common::swap16(words[i]);

  return true;
}

static bool FillDSPInitOptions(DSPInitOptions* opts)
{
  std::string irom_file = File::GetUserPath(D_GCUSER_IDX) + DSP_IROM;
  std::string coef_file = File::GetUserPath(D_GCUSER_IDX) + DSP_COEF;

  if (!File::Exists(irom_file))
    irom_file = File::GetSysDirectory() + GC_SYS_DIR DIR_SEP DSP_IROM;
  if (!File::Exists(coef_file))
    coef_file = File::GetSysDirectory() + GC_SYS_DIR DIR_SEP DSP_COEF;

  if (!LoadDSPRom(opts->irom_contents.data(), irom_file, DSP_IROM_BYTE_SIZE))
    return false;
  if (!LoadDSPRom(opts->coef_contents.data(), coef_file, DSP_COEF_BYTE_SIZE))
    return false;

  opts->core_type = DSPInitOptions::CoreType::Interpreter;
#ifdef _M_X86_64
  if (Config::Get(Config::MAIN_DSP_JIT))
    opts->core_type = DSPInitOptions::CoreType::JIT64;
#endif

  if (Config::Get(Config::MAIN_DSP_CAPTURE_LOG))
  {
    const std::string pcap_path = File::GetUserPath(D_DUMPDSP_IDX) + "dsp.pcap";
    opts->capture_logger = new PCAPDSPCaptureLogger(pcap_path);
  }

  return true;
}

bool DSPLLE::Initialize(bool wii, bool dsp_thread)
{
  m_request_disable_thread = false;

  DSPInitOptions opts;
  if (!FillDSPInitOptions(&opts))
    return false;
  if (!m_dsp_core.Initialize(opts))
    return false;

  // needs to be after DSPCore_Init for the dspjit ptr
  if (Core::WantsDeterminism() || !m_dsp_core.IsJITCreated())
    dsp_thread = false;

  m_wii = wii;
  m_is_dsp_on_thread = dsp_thread;

  m_dsp_core.Reset();

  InitInstructionTable();

  if (dsp_thread)
  {
    m_is_running.Set(true);
    m_dsp_thread = std::thread(DSPThread, this);
  }

  Host_RefreshDSPDebuggerWindow();
  return true;
}

void DSPLLE::DSP_StopSoundStream()
{
  if (!m_is_dsp_on_thread)
    return;

  m_is_running.Clear();
  m_ppc_event.Set();
  m_dsp_event.Set();
  m_dsp_thread.join();
}

void DSPLLE::Shutdown()
{
  m_dsp_core.Shutdown();
}

u16 DSPLLE::DSP_WriteControlRegister(u16 value)
{
  m_dsp_core.GetInterpreter().WriteControlRegister(value);

  if ((value & CR_EXTERNAL_INT) != 0)
  {
    if (m_is_dsp_on_thread)
    {
      // External interrupt pending: this is the zelda ucode.
      // Disable the DSP thread because there is no performance gain.
      m_request_disable_thread = true;

      m_dsp_core.SetExternalInterrupt(true);
    }
    else
    {
      m_dsp_core.CheckExternalInterrupt();
      m_dsp_core.CheckExceptions();
    }
  }

  return DSP_ReadControlRegister();
}

u16 DSPLLE::DSP_ReadControlRegister()
{
  return m_dsp_core.GetInterpreter().ReadControlRegister();
}

u16 DSPLLE::DSP_ReadMailBoxHigh(bool cpu_mailbox)
{
  return m_dsp_core.ReadMailboxHigh(cpu_mailbox ? Mailbox::CPU : Mailbox::DSP);
}

u16 DSPLLE::DSP_ReadMailBoxLow(bool cpu_mailbox)
{
  return m_dsp_core.ReadMailboxLow(cpu_mailbox ? Mailbox::CPU : Mailbox::DSP);
}

void DSPLLE::DSP_WriteMailBoxHigh(bool cpu_mailbox, u16 value)
{
  if (cpu_mailbox)
  {
    if ((m_dsp_core.PeekMailbox(Mailbox::CPU) & 0x80000000) != 0)
    {
      // the DSP didn't read the previous value
      WARN_LOG_FMT(DSPLLE, "Mailbox isn't empty ... strange");
    }

    m_dsp_core.WriteMailboxHigh(Mailbox::CPU, value);
  }
  else
  {
    ERROR_LOG_FMT(DSPLLE, "CPU can't write to DSP mailbox");
  }
}

void DSPLLE::DSP_WriteMailBoxLow(bool cpu_mailbox, u16 value)
{
  if (cpu_mailbox)
  {
    m_dsp_core.WriteMailboxLow(Mailbox::CPU, value);
  }
  else
  {
    ERROR_LOG_FMT(DSPLLE, "CPU can't write to DSP mailbox");
  }
}

void DSPLLE::DSP_Update(int cycles)
{
  const int dsp_cycles = cycles / 6;

  if (dsp_cycles <= 0)
    return;

  if (m_is_dsp_on_thread)
  {
    if (m_request_disable_thread || Core::WantsDeterminism())
    {
      DSP_StopSoundStream();
      m_is_dsp_on_thread = false;
      m_request_disable_thread = false;
      Config::SetBaseOrCurrent(Config::MAIN_DSP_THREAD, false);
    }
  }

  // If we're not on a thread, run cycles here.
  if (!m_is_dsp_on_thread)
  {
    // ~1/6th as many cycles as the period PPC-side.
    m_dsp_core.RunCycles(dsp_cycles);
  }
  else
  {
    // Wait for DSP thread to complete its cycle. Note: this logic should be thought through.
    m_ppc_event.Wait();
    m_cycle_count.fetch_add(dsp_cycles);
    m_dsp_event.Set();
  }
}

u32 DSPLLE::DSP_UpdateRate()
{
  return 12600;  // TO BE TWEAKED
}

void DSPLLE::PauseAndLock(bool do_lock)
{
  if (do_lock)
  {
    m_dsp_thread_mutex.lock();
  }
  else
  {
    m_dsp_thread_mutex.unlock();

    if (m_is_dsp_on_thread)
    {
      // Signal the DSP thread so it can perform any outstanding work now (if any)
      m_ppc_event.Wait();
      m_dsp_event.Set();
    }
  }
}
}  // namespace DSP::LLE
