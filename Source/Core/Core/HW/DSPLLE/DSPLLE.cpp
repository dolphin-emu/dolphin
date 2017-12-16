// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/DSPLLE/DSPLLE.h"

#include <mutex>
#include <string>
#include <thread>

#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Logging/Log.h"
#include "Common/MemoryUtil.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/DSP/DSPAccelerator.h"
#include "Core/DSP/DSPCaptureLogger.h"
#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPHWInterface.h"
#include "Core/DSP/DSPHost.h"
#include "Core/DSP/DSPTables.h"
#include "Core/DSP/Interpreter/DSPInterpreter.h"
#include "Core/HW/DSPLLE/DSPLLEGlobals.h"
#include "Core/HW/Memmap.h"
#include "Core/Host.h"

namespace DSP
{
namespace LLE
{
static Common::Event s_dsp_event;
static Common::Event s_ppc_event;
static bool s_request_disable_thread;

DSPLLE::DSPLLE() = default;

DSPLLE::~DSPLLE()
{
  DSPCore_Shutdown();
  DSP_StopSoundStream();
}

void DSPLLE::DoState(PointerWrap& p)
{
  bool is_hle = false;
  p.Do(is_hle);
  if (is_hle && p.GetMode() == PointerWrap::MODE_READ)
  {
    Core::DisplayMessage("State is incompatible with current DSP engine. Aborting load state.",
                         3000);
    p.SetMode(PointerWrap::MODE_VERIFY);
    return;
  }
  p.Do(g_dsp.r);
  p.Do(g_dsp.pc);
#if PROFILE
  p.Do(g_dsp.err_pc);
#endif
  p.Do(g_dsp.cr);
  p.Do(g_dsp.reg_stack_ptr);
  p.Do(g_dsp.exceptions);
  p.Do(g_dsp.external_interrupt_waiting);

  for (int i = 0; i < 4; i++)
  {
    p.Do(g_dsp.reg_stack[i]);
  }

  p.Do(g_dsp.step_counter);
  p.DoArray(g_dsp.ifx_regs);
  g_dsp.accelerator->DoState(p);
  p.Do(g_dsp.mbox[0]);
  p.Do(g_dsp.mbox[1]);
  Common::UnWriteProtectMemory(g_dsp.iram, DSP_IRAM_BYTE_SIZE, false);
  p.DoArray(g_dsp.iram, DSP_IRAM_SIZE);
  Common::WriteProtectMemory(g_dsp.iram, DSP_IRAM_BYTE_SIZE, false);
  if (p.GetMode() == PointerWrap::MODE_READ)
    Host::CodeLoaded((const u8*)g_dsp.iram, DSP_IRAM_BYTE_SIZE);
  p.DoArray(g_dsp.dram, DSP_DRAM_SIZE);
  p.Do(g_init_hax);
  p.Do(m_cycle_count);

  if (g_dsp_jit)
    g_dsp_jit->DoState(p);
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
      std::lock_guard<std::mutex> dsp_thread_lock(dsp_lle->m_dsp_thread_mutex);
      if (g_dsp_jit)
      {
        DSPCore_RunCycles(cycles);
      }
      else
      {
        DSP::Interpreter::RunCyclesThread(cycles);
      }
      dsp_lle->m_cycle_count.store(0);
    }
    else
    {
      s_ppc_event.Set();
      s_dsp_event.Wait();
    }
  }
}

static bool LoadDSPRom(u16* rom, const std::string& filename, u32 size_in_bytes)
{
  std::string bytes;
  if (!File::ReadFileToString(filename, bytes))
    return false;

  if (bytes.size() != size_in_bytes)
  {
    ERROR_LOG(DSPLLE, "%s has a wrong size (%zu, expected %u)", filename.c_str(), bytes.size(),
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

  opts->core_type = DSPInitOptions::CORE_INTERPRETER;
#ifdef _M_X86
  if (SConfig::GetInstance().m_DSPEnableJIT)
    opts->core_type = DSPInitOptions::CORE_JIT;
#endif

  if (SConfig::GetInstance().m_DSPCaptureLog)
  {
    const std::string pcap_path = File::GetUserPath(D_DUMPDSP_IDX) + "dsp.pcap";
    opts->capture_logger = new PCAPDSPCaptureLogger(pcap_path);
  }

  return true;
}

bool DSPLLE::Initialize(bool wii, bool dsp_thread)
{
  s_request_disable_thread = false;

  DSPInitOptions opts;
  if (!FillDSPInitOptions(&opts))
    return false;
  if (!DSPCore_Init(opts))
    return false;

  // needs to be after DSPCore_Init for the dspjit ptr
  if (Core::WantsDeterminism() || !g_dsp_jit)
    dsp_thread = false;

  m_wii = wii;
  m_is_dsp_on_thread = dsp_thread;

  // DSPLLE directly accesses the fastmem arena.
  // TODO: The fastmem arena is only supposed to be used by the JIT:
  // among other issues, its size is only 1GB on 32-bit targets.
  g_dsp.cpu_ram = Memory::physical_base;
  DSPCore_Reset();

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
  if (m_is_dsp_on_thread)
  {
    m_is_running.Clear();
    s_ppc_event.Set();
    s_dsp_event.Set();
    m_dsp_thread.join();
  }
}

void DSPLLE::Shutdown()
{
  DSPCore_Shutdown();
}

u16 DSPLLE::DSP_WriteControlRegister(u16 value)
{
  DSP::Interpreter::WriteCR(value);

  if (value & 2)
  {
    if (!m_is_dsp_on_thread)
    {
      DSPCore_CheckExternalInterrupt();
      DSPCore_CheckExceptions();
    }
    else
    {
      // External interrupt pending: this is the zelda ucode.
      // Disable the DSP thread because there is no performance gain.
      s_request_disable_thread = true;

      DSPCore_SetExternalInterrupt(true);
    }
  }

  return DSP::Interpreter::ReadCR();
}

u16 DSPLLE::DSP_ReadControlRegister()
{
  return DSP::Interpreter::ReadCR();
}

u16 DSPLLE::DSP_ReadMailBoxHigh(bool cpu_mailbox)
{
  return gdsp_mbox_read_h(cpu_mailbox ? MAILBOX_CPU : MAILBOX_DSP);
}

u16 DSPLLE::DSP_ReadMailBoxLow(bool cpu_mailbox)
{
  return gdsp_mbox_read_l(cpu_mailbox ? MAILBOX_CPU : MAILBOX_DSP);
}

void DSPLLE::DSP_WriteMailBoxHigh(bool cpu_mailbox, u16 value)
{
  if (cpu_mailbox)
  {
    if (gdsp_mbox_peek(MAILBOX_CPU) & 0x80000000)
    {
      ERROR_LOG(DSPLLE, "Mailbox isn't empty ... strange");
    }

#if PROFILE
    if (value == 0xBABE)
    {
      ProfilerStart();
    }
#endif

    gdsp_mbox_write_h(MAILBOX_CPU, value);
  }
  else
  {
    ERROR_LOG(DSPLLE, "CPU can't write to DSP mailbox");
  }
}

void DSPLLE::DSP_WriteMailBoxLow(bool cpu_mailbox, u16 value)
{
  if (cpu_mailbox)
  {
    gdsp_mbox_write_l(MAILBOX_CPU, value);
  }
  else
  {
    ERROR_LOG(DSPLLE, "CPU can't write to DSP mailbox");
  }
}

void DSPLLE::DSP_Update(int cycles)
{
  int dsp_cycles = cycles / 6;

  if (dsp_cycles <= 0)
    return;

  if (m_is_dsp_on_thread)
  {
    if (s_request_disable_thread || Core::WantsDeterminism())
    {
      DSP_StopSoundStream();
      m_is_dsp_on_thread = false;
      s_request_disable_thread = false;
      SConfig::GetInstance().bDSPThread = false;
    }
  }

  // If we're not on a thread, run cycles here.
  if (!m_is_dsp_on_thread)
  {
    // ~1/6th as many cycles as the period PPC-side.
    DSPCore_RunCycles(dsp_cycles);
  }
  else
  {
    // Wait for DSP thread to complete its cycle. Note: this logic should be thought through.
    s_ppc_event.Wait();
    m_cycle_count.fetch_add(dsp_cycles);
    s_dsp_event.Set();
  }
}

u32 DSPLLE::DSP_UpdateRate()
{
  return 12600;  // TO BE TWEAKED
}

void DSPLLE::PauseAndLock(bool do_lock, bool unpause_on_unlock)
{
  if (do_lock)
    m_dsp_thread_mutex.lock();
  else
    m_dsp_thread_mutex.unlock();
}
}  // namespace LLE
}  // namespace DSP
