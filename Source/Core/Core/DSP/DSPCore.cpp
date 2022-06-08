// Copyright 2008 Dolphin Emulator Project
// Copyright 2004 Duddie & Tratax
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/DSP/DSPCore.h"

#include <algorithm>
#include <array>
#include <memory>
#include <type_traits>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Hash.h"
#include "Common/MemoryUtil.h"
#include "Common/MsgHandler.h"

#include "Core/DSP/DSPAccelerator.h"
#include "Core/DSP/DSPAnalyzer.h"
#include "Core/DSP/DSPHost.h"
#include "Core/DSP/Interpreter/DSPInterpreter.h"
#include "Core/DSP/Jit/DSPEmitterBase.h"

namespace DSP
{
// Returns false if the hash fails and the user hits "Yes"
static bool VerifyRoms(const SDSP& dsp)
{
  struct DspRomHashes
  {
    u32 hash_irom;  // dsp_rom.bin
    u32 hash_drom;  // dsp_coef.bin
  };

  static const std::array<DspRomHashes, 7> known_roms = {{
      // Official Nintendo ROM
      {0x66f334fe, 0xf3b93527},

      // v0.1: LM1234 replacement ROM (Zelda UCode only)
      {0x9c8f593c, 0x10000001},

      // v0.2: delroth's improvement on LM1234 replacement ROM (Zelda and AX only,
      // IPL/Card/GBA still broken)
      {0xd9907f71, 0xb019c2fb},

      // v0.2.1: above with improved resampling coefficients
      {0xd9907f71, 0xdb6880c1},

      // v0.3: above with support for GBA ucode
      {0x3aa4a793, 0xa4a575f5},

      // v0.3.1: above with fix to skip bootucode_ax when running from ROM entrypoint
      {0x128ea7a2, 0xa4a575f5},

      // v0.4: above with fixes for invalid use of SRS instruction
      {0xe789b5a5, 0xa4a575f5},
  }};

  const u32 hash_irom =
      Common::HashAdler32(reinterpret_cast<const u8*>(dsp.irom), DSP_IROM_BYTE_SIZE);
  const u32 hash_drom =
      Common::HashAdler32(reinterpret_cast<const u8*>(dsp.coef), DSP_COEF_BYTE_SIZE);
  int rom_idx = -1;

  for (size_t i = 0; i < known_roms.size(); ++i)
  {
    const DspRomHashes& rom = known_roms[i];
    if (hash_irom == rom.hash_irom && hash_drom == rom.hash_drom)
      rom_idx = static_cast<int>(i);
  }

  if (rom_idx < 0)
  {
    if (AskYesNoFmtT("Your DSP ROMs have incorrect hashes.\n\n"
                     "Delete the dsp_rom.bin and dsp_coef.bin files in the GC folder in the User "
                     "folder to use the free DSP ROM, or replace them with good dumps from a real "
                     "GameCube/Wii.\n\n"
                     "Would you like to stop now to fix the problem?\n"
                     "If you select \"No\", audio might be garbled."))
    {
      return false;
    }
  }

  if (rom_idx >= 1 && rom_idx <= 5)
  {
    if (AskYesNoFmtT(
            "You are using an old free DSP ROM made by the Dolphin Team.\n"
            "Due to emulation accuracy improvements, this ROM no longer works correctly.\n\n"
            "Delete the dsp_rom.bin and dsp_coef.bin files in the GC folder in the User folder "
            "to use the most recent free DSP ROM, or replace them with good dumps from a real "
            "GameCube/Wii.\n\n"
            "Would you like to stop now to fix the problem?\n"
            "If you select \"No\", audio might be garbled."))
    {
      return false;
    }
  }

  return true;
}

class LLEAccelerator final : public Accelerator
{
public:
  explicit LLEAccelerator(SDSP& dsp) : m_dsp{dsp} {}

protected:
  u8 ReadMemory(u32 address) override { return Host::ReadHostMemory(address); }
  void WriteMemory(u32 address, u8 value) override { Host::WriteHostMemory(value, address); }
  void OnEndException() override { m_dsp.SetException(ExceptionType::AcceleratorOverflow); }

private:
  SDSP& m_dsp;
};

SDSP::SDSP(DSPCore& core) : m_dsp_core{core}
{
}

SDSP::~SDSP() = default;

bool SDSP::Initialize(const DSPInitOptions& opts)
{
  m_step_counter = 0;
  m_accelerator = std::make_unique<LLEAccelerator>(*this);

  irom = static_cast<u16*>(Common::AllocateMemoryPages(DSP_IROM_BYTE_SIZE));
  iram = static_cast<u16*>(Common::AllocateMemoryPages(DSP_IRAM_BYTE_SIZE));
  dram = static_cast<u16*>(Common::AllocateMemoryPages(DSP_DRAM_BYTE_SIZE));
  coef = static_cast<u16*>(Common::AllocateMemoryPages(DSP_COEF_BYTE_SIZE));

  std::memcpy(irom, opts.irom_contents.data(), DSP_IROM_BYTE_SIZE);
  std::memcpy(coef, opts.coef_contents.data(), DSP_COEF_BYTE_SIZE);

  // Try to load real ROM contents.
  if (!VerifyRoms(*this))
  {
    FreeMemoryPages();
    return false;
  }

  std::memset(&r, 0, sizeof(r));

  std::fill(std::begin(reg_stack_ptrs), std::end(reg_stack_ptrs), 0);

  for (auto& stack : reg_stacks)
    std::fill(std::begin(stack), std::end(stack), 0);

  // Fill IRAM with HALT opcodes.
  std::fill(iram, iram + DSP_IRAM_SIZE, 0x0021);

  // Just zero out DRAM.
  std::fill(dram, dram + DSP_DRAM_SIZE, 0);

  // Copied from a real console after the custom UCode has been loaded.
  // These are the indexing wrapping registers.
  std::fill(std::begin(r.wr), std::end(r.wr), 0xffff);

  r.sr |= SR_INT_ENABLE;
  r.sr |= SR_EXT_INT_ENABLE;

  control_reg = CR_INIT | CR_HALT;
  InitializeIFX();
  // Mostly keep IRAM write protected. We unprotect only when DMA-ing
  // in new ucodes.
  Common::WriteProtectMemory(iram, DSP_IRAM_BYTE_SIZE, false);
  return true;
}

void SDSP::Reset()
{
  pc = DSP_RESET_VECTOR;
  std::fill(std::begin(r.wr), std::end(r.wr), 0xffff);
}

void SDSP::Shutdown()
{
  FreeMemoryPages();
}

void SDSP::FreeMemoryPages()
{
  Common::FreeMemoryPages(irom, DSP_IROM_BYTE_SIZE);
  Common::FreeMemoryPages(iram, DSP_IRAM_BYTE_SIZE);
  Common::FreeMemoryPages(dram, DSP_DRAM_BYTE_SIZE);
  Common::FreeMemoryPages(coef, DSP_COEF_BYTE_SIZE);
  irom = nullptr;
  iram = nullptr;
  dram = nullptr;
  coef = nullptr;
}

void SDSP::SetException(ExceptionType exception)
{
  exceptions |= 1 << static_cast<std::underlying_type_t<ExceptionType>>(exception);
}

void SDSP::SetExternalInterrupt(bool val)
{
  external_interrupt_waiting.store(val, std::memory_order_release);
}

void SDSP::CheckExternalInterrupt()
{
  if (!IsSRFlagSet(SR_EXT_INT_ENABLE))
    return;

  // Signal the SPU about new mail
  SetException(ExceptionType::ExternalInterrupt);

  control_reg &= ~CR_EXTERNAL_INT;
}

void SDSP::CheckExceptions()
{
  // Early out to skip the loop in the common case.
  if (exceptions == 0)
    return;

  for (int i = 7; i > 0; i--)
  {
    // Seems exp int are not masked by sr_int_enable
    if ((exceptions & (1U << i)) != 0)
    {
      if (IsSRFlagSet(SR_INT_ENABLE) || i == static_cast<int>(ExceptionType::ExternalInterrupt))
      {
        // store pc and sr until RTI
        StoreStack(StackRegister::Call, pc);
        StoreStack(StackRegister::Data, r.sr);

        pc = static_cast<u16>(i * 2);
        exceptions &= ~(1 << i);
        if (i == 7)
          r.sr &= ~SR_EXT_INT_ENABLE;
        else
          r.sr &= ~SR_INT_ENABLE;
        break;
      }
      else
      {
#if defined(_DEBUG) || defined(DEBUGFAST)
        ERROR_LOG_FMT(DSPLLE, "Firing exception {} failed", i);
#endif
      }
    }
  }
}

u16 SDSP::ReadRegister(size_t reg) const
{
  switch (reg)
  {
  case DSP_REG_AR0:
  case DSP_REG_AR1:
  case DSP_REG_AR2:
  case DSP_REG_AR3:
    return r.ar[reg - DSP_REG_AR0];
  case DSP_REG_IX0:
  case DSP_REG_IX1:
  case DSP_REG_IX2:
  case DSP_REG_IX3:
    return r.ix[reg - DSP_REG_IX0];
  case DSP_REG_WR0:
  case DSP_REG_WR1:
  case DSP_REG_WR2:
  case DSP_REG_WR3:
    return r.wr[reg - DSP_REG_WR0];
  case DSP_REG_ST0:
  case DSP_REG_ST1:
  case DSP_REG_ST2:
  case DSP_REG_ST3:
    return r.st[reg - DSP_REG_ST0];
  case DSP_REG_ACH0:
  case DSP_REG_ACH1:
    return r.ac[reg - DSP_REG_ACH0].h;
  case DSP_REG_CR:
    return r.cr;
  case DSP_REG_SR:
    return r.sr;
  case DSP_REG_PRODL:
    return r.prod.l;
  case DSP_REG_PRODM:
    return r.prod.m;
  case DSP_REG_PRODH:
    return r.prod.h;
  case DSP_REG_PRODM2:
    return r.prod.m2;
  case DSP_REG_AXL0:
  case DSP_REG_AXL1:
    return r.ax[reg - DSP_REG_AXL0].l;
  case DSP_REG_AXH0:
  case DSP_REG_AXH1:
    return r.ax[reg - DSP_REG_AXH0].h;
  case DSP_REG_ACL0:
  case DSP_REG_ACL1:
    return r.ac[reg - DSP_REG_ACL0].l;
  case DSP_REG_ACM0:
  case DSP_REG_ACM1:
    return r.ac[reg - DSP_REG_ACM0].m;
  default:
    ASSERT_MSG(DSPLLE, 0, "cannot happen");
    return 0;
  }
}

void SDSP::WriteRegister(size_t reg, u16 val)
{
  switch (reg)
  {
  case DSP_REG_AR0:
  case DSP_REG_AR1:
  case DSP_REG_AR2:
  case DSP_REG_AR3:
    r.ar[reg - DSP_REG_AR0] = val;
    break;
  case DSP_REG_IX0:
  case DSP_REG_IX1:
  case DSP_REG_IX2:
  case DSP_REG_IX3:
    r.ix[reg - DSP_REG_IX0] = val;
    break;
  case DSP_REG_WR0:
  case DSP_REG_WR1:
  case DSP_REG_WR2:
  case DSP_REG_WR3:
    r.wr[reg - DSP_REG_WR0] = val;
    break;
  case DSP_REG_ST0:
  case DSP_REG_ST1:
  case DSP_REG_ST2:
  case DSP_REG_ST3:
    r.st[reg - DSP_REG_ST0] = val;
    break;
  case DSP_REG_ACH0:
  case DSP_REG_ACH1:
    r.ac[reg - DSP_REG_ACH0].h = val;
    break;
  case DSP_REG_CR:
    r.cr = val;
    break;
  case DSP_REG_SR:
    r.sr = val;
    break;
  case DSP_REG_PRODL:
    r.prod.l = val;
    break;
  case DSP_REG_PRODM:
    r.prod.m = val;
    break;
  case DSP_REG_PRODH:
    r.prod.h = val;
    break;
  case DSP_REG_PRODM2:
    r.prod.m2 = val;
    break;
  case DSP_REG_AXL0:
  case DSP_REG_AXL1:
    r.ax[reg - DSP_REG_AXL0].l = val;
    break;
  case DSP_REG_AXH0:
  case DSP_REG_AXH1:
    r.ax[reg - DSP_REG_AXH0].h = val;
    break;
  case DSP_REG_ACL0:
  case DSP_REG_ACL1:
    r.ac[reg - DSP_REG_ACL0].l = val;
    break;
  case DSP_REG_ACM0:
  case DSP_REG_ACM1:
    r.ac[reg - DSP_REG_ACM0].m = val;
    break;
  }
}

void SDSP::DoState(PointerWrap& p)
{
  p.Do(r);
  p.Do(pc);
  p.Do(control_reg);
  p.Do(control_reg_init_code_clear_time);
  p.Do(reg_stack_ptrs);
  p.Do(exceptions);
  p.Do(external_interrupt_waiting);

  for (auto& stack : reg_stacks)
  {
    p.Do(stack);
  }

  p.Do(m_step_counter);
  p.DoArray(m_ifx_regs);
  m_accelerator->DoState(p);
  p.Do(m_mailbox[0]);
  p.Do(m_mailbox[1]);
  Common::UnWriteProtectMemory(iram, DSP_IRAM_BYTE_SIZE, false);
  p.DoArray(iram, DSP_IRAM_SIZE);
  Common::WriteProtectMemory(iram, DSP_IRAM_BYTE_SIZE, false);
  // TODO: This uses the wrong endianness (producing bad disassembly)
  // and a bogus byte count (producing bad hashes)
  if (p.IsReadMode())
    Host::CodeLoaded(m_dsp_core, reinterpret_cast<const u8*>(iram), DSP_IRAM_BYTE_SIZE);
  p.DoArray(dram, DSP_DRAM_SIZE);
}

DSPCore::DSPCore()
    : m_dsp{*this}, m_dsp_interpreter{std::make_unique<Interpreter::Interpreter>(*this)}
{
}

DSPCore::~DSPCore() = default;

bool DSPCore::Initialize(const DSPInitOptions& opts)
{
  if (!m_dsp.Initialize(opts))
    return false;

  m_init_hax = false;

  // Initialize JIT, if necessary
  if (opts.core_type == DSPInitOptions::CoreType::JIT64)
    m_dsp_jit = JIT::CreateDSPEmitter(*this);

  m_dsp_cap.reset(opts.capture_logger);

  m_core_state = State::Running;
  return true;
}

void DSPCore::Shutdown()
{
  if (m_core_state == State::Stopped)
    return;

  m_core_state = State::Stopped;

  m_dsp_jit.reset();
  m_dsp.Shutdown();
  m_dsp_cap.reset();
}

// Delegate to JIT or interpreter as appropriate.
// Handle state changes and stepping.
int DSPCore::RunCycles(int cycles)
{
  if (m_dsp_jit)
  {
    return m_dsp_jit->RunCycles(static_cast<u16>(cycles));
  }

  while (cycles > 0)
  {
    switch (m_core_state)
    {
    case State::Running:
// Seems to slow things down
#if defined(_DEBUG) || defined(DEBUGFAST)
      cycles = m_dsp_interpreter->RunCyclesDebug(cycles);
#else
      cycles = m_dsp_interpreter->RunCycles(cycles);
#endif
      break;

    case State::Stepping:
      m_step_event.Wait();
      if (m_core_state != State::Stepping)
        continue;

      m_dsp_interpreter->Step();
      cycles--;

      Host::UpdateDebugger();
      break;
    case State::Stopped:
      break;
    }
  }
  return cycles;
}

void DSPCore::Step()
{
  if (m_core_state == State::Stepping)
    m_step_event.Set();
}

void DSPCore::Reset()
{
  m_dsp.Reset();
  m_dsp.GetAnalyzer().Analyze(m_dsp);
}

void DSPCore::ClearIRAM()
{
  if (!m_dsp_jit)
    return;

  m_dsp_jit->ClearIRAM();
}

void DSPCore::SetState(State new_state)
{
  m_core_state = new_state;

  // kick the event, in case we are waiting
  if (new_state == State::Running)
    m_step_event.Set();

  Host::UpdateDebugger();
}

State DSPCore::GetState() const
{
  return m_core_state;
}

void DSPCore::SetException(ExceptionType exception)
{
  m_dsp.SetException(exception);
}

void DSPCore::SetExternalInterrupt(bool val)
{
  m_dsp.SetExternalInterrupt(val);
}

void DSPCore::CheckExternalInterrupt()
{
  m_dsp.CheckExternalInterrupt();
}

void DSPCore::CheckExceptions()
{
  m_dsp.CheckExceptions();
}

u16 DSPCore::ReadRegister(size_t reg) const
{
  return m_dsp.ReadRegister(reg);
}

void DSPCore::WriteRegister(size_t reg, u16 val)
{
  m_dsp.WriteRegister(reg, val);
}

u32 DSPCore::PeekMailbox(Mailbox mailbox) const
{
  return m_dsp.PeekMailbox(mailbox);
}

u16 DSPCore::ReadMailboxLow(Mailbox mailbox)
{
  return m_dsp.ReadMailboxLow(mailbox);
}

u16 DSPCore::ReadMailboxHigh(Mailbox mailbox)
{
  return m_dsp.ReadMailboxHigh(mailbox);
}

void DSPCore::WriteMailboxLow(Mailbox mailbox, u16 value)
{
  m_dsp.WriteMailboxLow(mailbox, value);
}

void DSPCore::WriteMailboxHigh(Mailbox mailbox, u16 value)
{
  m_dsp.WriteMailboxHigh(mailbox, value);
}

void DSPCore::LogIFXRead(u16 address, u16 read_value)
{
  m_dsp_cap->LogIFXRead(address, read_value);
}

void DSPCore::LogIFXWrite(u16 address, u16 written_value)
{
  m_dsp_cap->LogIFXWrite(address, written_value);
}

void DSPCore::LogDMA(u16 control, u32 gc_address, u16 dsp_address, u16 length, const u8* data)
{
  m_dsp_cap->LogDMA(control, gc_address, dsp_address, length, data);
}

bool DSPCore::IsJITCreated() const
{
  return m_dsp_jit != nullptr;
}

void DSPCore::DoState(PointerWrap& p)
{
  m_dsp.DoState(p);
  p.Do(m_init_hax);

  if (m_dsp_jit)
    m_dsp_jit->DoState(p);
}
}  // namespace DSP
