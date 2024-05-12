// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// AID / AUDIO_DMA controls pushing audio out to the SRC and then the speakers.
// The audio DMA pushes audio through a small FIFO 32 bytes at a time, as
// needed.

// The SRC behind the fifo eats stereo 16-bit data at a sample rate of 32khz,
// that is, 4 bytes at 32 khz, which is 32 bytes at 4 khz. We thereforce
// schedule an event that runs at 4khz, that eats audio from the fifo. Thus, we
// have homebrew audio.

// The AID interrupt is set when the fifo STARTS a transfer. It latches address
// and count into internal registers and starts copying. This means that the
// interrupt handler can simply set the registers to where the next buffer is,
// and start filling it. When the DMA is complete, it will automatically
// relatch and fire a new interrupt.

// Then there's the DSP... what likely happens is that the
// fifo-latched-interrupt handler kicks off the DSP, requesting it to fill up
// the just used buffer through the AXList (or whatever it might be called in
// Nintendo games).

#include "Core/HW/DSP.h"

#include <memory>

#include "AudioCommon/AudioCommon.h"

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/MemoryUtil.h"

#include "Core/CoreTiming.h"
#include "Core/DSPEmulator.h"
#include "Core/HW/HSP/HSP.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

namespace DSP
{
// register offsets
enum
{
  DSP_MAIL_TO_DSP_HI = 0x5000,
  DSP_MAIL_TO_DSP_LO = 0x5002,
  DSP_MAIL_FROM_DSP_HI = 0x5004,
  DSP_MAIL_FROM_DSP_LO = 0x5006,
  DSP_CONTROL = 0x500A,
  DSP_INTERRUPT_CONTROL = 0x5010,
  AR_INFO = 0x5012,  // These names are a good guess at best
  AR_MODE = 0x5016,  //
  AR_REFRESH = 0x501a,
  AR_DMA_MMADDR_H = 0x5020,
  AR_DMA_MMADDR_L = 0x5022,
  AR_DMA_ARADDR_H = 0x5024,
  AR_DMA_ARADDR_L = 0x5026,
  AR_DMA_CNT_H = 0x5028,
  AR_DMA_CNT_L = 0x502A,
  AUDIO_DMA_START_HI = 0x5030,
  AUDIO_DMA_START_LO = 0x5032,
  AUDIO_DMA_BLOCKS_LENGTH = 0x5034,  // Ever used?
  AUDIO_DMA_CONTROL_LEN = 0x5036,
  AUDIO_DMA_BLOCKS_LEFT = 0x503A,
};

DSPManager::DSPManager(Core::System& system) : m_system(system)
{
}

DSPManager::~DSPManager() = default;

// time given to LLE DSP on every read of the high bits in a mailbox
constexpr int DSP_MAIL_SLICE = 72;

void DSPManager::DoState(PointerWrap& p)
{
  if (!m_aram.wii_mode)
    p.DoArray(m_aram.ptr, m_aram.size);
  p.Do(m_dsp_control);
  p.Do(m_audio_dma);
  p.Do(m_aram_dma);
  p.Do(m_aram_info);
  p.Do(m_aram_mode);
  p.Do(m_aram_refresh);
  p.Do(m_dsp_slice);

  m_dsp_emulator->DoState(p);
}

void DSPManager::GlobalCompleteARAM(Core::System& system, u64 userdata, s64 cyclesLate)
{
  system.GetDSP().CompleteARAM(userdata, cyclesLate);
}

void DSPManager::CompleteARAM(u64 userdata, s64 cyclesLate)
{
  m_dsp_control.DMAState = 0;
  GenerateDSPInterrupt(INT_ARAM, 0);
}

DSPEmulator* DSPManager::GetDSPEmulator()
{
  return m_dsp_emulator.get();
}

void DSPManager::Init(bool hle)
{
  Reinit(hle);
  auto& core_timing = m_system.GetCoreTiming();
  m_event_type_generate_dsp_interrupt =
      core_timing.RegisterEvent("DSPint", GlobalGenerateDSPInterrupt);
  m_event_type_complete_aram = core_timing.RegisterEvent("ARAMint", GlobalCompleteARAM);
}

void DSPManager::Reinit(bool hle)
{
  m_dsp_emulator = CreateDSPEmulator(m_system, hle);
  m_is_lle = m_dsp_emulator->IsLLE();

  if (m_system.IsWii())
  {
    auto& memory = m_system.GetMemory();
    m_aram.wii_mode = true;
    m_aram.size = memory.GetExRamSizeReal();
    m_aram.mask = memory.GetExRamMask();
    m_aram.ptr = memory.GetEXRAM();
  }
  else
  {
    // On the GameCube, ARAM is accessible only through this interface.
    m_aram.wii_mode = false;
    m_aram.size = ARAM_SIZE;
    m_aram.mask = ARAM_MASK;
    m_aram.ptr = static_cast<u8*>(Common::AllocateMemoryPages(m_aram.size));
  }

  m_audio_dma = {};
  m_aram_dma = {};

  m_dsp_control.Hex = 0;
  m_dsp_control.DSPHalt = 1;

  m_aram_info.Hex = 0;
  m_aram_mode = 1;       // ARAM Controller has init'd
  m_aram_refresh = 156;  // 156MHz
}

void DSPManager::Shutdown()
{
  if (!m_aram.wii_mode)
  {
    Common::FreeMemoryPages(m_aram.ptr, m_aram.size);
    m_aram.ptr = nullptr;
  }

  m_dsp_emulator->Shutdown();
  m_dsp_emulator.reset();
}

void DSPManager::RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  static constexpr u16 WMASK_NONE = 0x0000;
  static constexpr u16 WMASK_AR_INFO = 0x007f;
  static constexpr u16 WMASK_AR_REFRESH = 0x07ff;
  static constexpr u16 WMASK_AR_HI_RESTRICT = 0x03ff;
  static constexpr u16 WMASK_AR_CNT_DIR_BIT = 0x8000;
  static constexpr u16 WMASK_AUDIO_HI_RESTRICT_GCN = 0x03ff;
  static constexpr u16 WMASK_AUDIO_HI_RESTRICT_WII = 0x1fff;
  static constexpr u16 WMASK_LO_ALIGN_32BIT = 0xffe0;

  // Declare all the boilerplate direct MMIOs.
  struct
  {
    u32 addr;
    u16* ptr;
    u16 wmask;
  } directly_mapped_vars[] = {
      // This register is read-only
      {AR_MODE, &m_aram_mode, WMASK_NONE},

      // For these registers, only some bits can be set
      {AR_INFO, &m_aram_info.Hex, WMASK_AR_INFO},
      {AR_REFRESH, &m_aram_refresh, WMASK_AR_REFRESH},

      // For AR_DMA_*_H registers, only bits 0x03ff can be set
      // For AR_DMA_*_L registers, only bits 0xffe0 can be set
      {AR_DMA_MMADDR_H, MMIO::Utils::HighPart(&m_aram_dma.MMAddr), WMASK_AR_HI_RESTRICT},
      {AR_DMA_MMADDR_L, MMIO::Utils::LowPart(&m_aram_dma.MMAddr), WMASK_LO_ALIGN_32BIT},
      {AR_DMA_ARADDR_H, MMIO::Utils::HighPart(&m_aram_dma.ARAddr), WMASK_AR_HI_RESTRICT},
      {AR_DMA_ARADDR_L, MMIO::Utils::LowPart(&m_aram_dma.ARAddr), WMASK_LO_ALIGN_32BIT},
      // For this register, the topmost (dir) bit can also be set
      {AR_DMA_CNT_H, MMIO::Utils::HighPart(&m_aram_dma.Cnt.Hex),
       WMASK_AR_HI_RESTRICT | WMASK_AR_CNT_DIR_BIT},
      // AR_DMA_CNT_L triggers DMA

      // For AUDIO_DMA_START_HI, only bits 0x03ff can be set on GCN and 0x1fff on Wii
      // For AUDIO_DMA_START_LO, only bits 0xffe0 can be set
      // AUDIO_DMA_START_HI requires a complex write handler
      {AUDIO_DMA_START_LO, MMIO::Utils::LowPart(&m_audio_dma.SourceAddress), WMASK_LO_ALIGN_32BIT},
  };
  for (auto& mapped_var : directly_mapped_vars)
  {
    mmio->Register(base | mapped_var.addr, MMIO::DirectRead<u16>(mapped_var.ptr),
                   mapped_var.wmask != WMASK_NONE ?
                       MMIO::DirectWrite<u16>(mapped_var.ptr, mapped_var.wmask) :
                       MMIO::InvalidWrite<u16>());
  }

  // DSP mail MMIOs call DSP emulator functions to get results or write data.
  mmio->Register(base | DSP_MAIL_TO_DSP_HI, MMIO::ComplexRead<u16>([](Core::System& system, u32) {
                   auto& dsp = system.GetDSP();
                   if (dsp.m_dsp_slice > DSP_MAIL_SLICE && dsp.m_is_lle)
                   {
                     dsp.m_dsp_emulator->DSP_Update(DSP_MAIL_SLICE);
                     dsp.m_dsp_slice -= DSP_MAIL_SLICE;
                   }
                   return dsp.m_dsp_emulator->DSP_ReadMailBoxHigh(true);
                 }),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& dsp = system.GetDSP();
                   dsp.m_dsp_emulator->DSP_WriteMailBoxHigh(true, val);
                 }));
  mmio->Register(base | DSP_MAIL_TO_DSP_LO, MMIO::ComplexRead<u16>([](Core::System& system, u32) {
                   auto& dsp = system.GetDSP();
                   return dsp.m_dsp_emulator->DSP_ReadMailBoxLow(true);
                 }),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& dsp = system.GetDSP();
                   dsp.m_dsp_emulator->DSP_WriteMailBoxLow(true, val);
                 }));
  mmio->Register(base | DSP_MAIL_FROM_DSP_HI, MMIO::ComplexRead<u16>([](Core::System& system, u32) {
                   auto& dsp = system.GetDSP();
                   if (dsp.m_dsp_slice > DSP_MAIL_SLICE && dsp.m_is_lle)
                   {
                     dsp.m_dsp_emulator->DSP_Update(DSP_MAIL_SLICE);
                     dsp.m_dsp_slice -= DSP_MAIL_SLICE;
                   }
                   return dsp.m_dsp_emulator->DSP_ReadMailBoxHigh(false);
                 }),
                 MMIO::InvalidWrite<u16>());
  mmio->Register(base | DSP_MAIL_FROM_DSP_LO, MMIO::ComplexRead<u16>([](Core::System& system, u32) {
                   auto& dsp = system.GetDSP();
                   return dsp.m_dsp_emulator->DSP_ReadMailBoxLow(false);
                 }),
                 MMIO::InvalidWrite<u16>());

  mmio->Register(
      base | DSP_CONTROL, MMIO::ComplexRead<u16>([](Core::System& system, u32) {
        auto& dsp = system.GetDSP();
        return (dsp.m_dsp_control.Hex & ~DSP_CONTROL_MASK) |
               (dsp.m_dsp_emulator->DSP_ReadControlRegister() & DSP_CONTROL_MASK);
      }),
      MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
        auto& dsp = system.GetDSP();

        UDSPControl tmpControl;
        tmpControl.Hex = (val & ~DSP_CONTROL_MASK) |
                         (dsp.m_dsp_emulator->DSP_WriteControlRegister(val) & DSP_CONTROL_MASK);

        // Not really sure if this is correct, but it works...
        // Kind of a hack because DSP_CONTROL_MASK should make this bit
        // only viewable to DSP emulator
        if (val & 1 /*DSPReset*/)
        {
          dsp.m_audio_dma.AudioDMAControl.Hex = 0;
        }

        // Update DSP related flags
        dsp.m_dsp_control.DSPReset = tmpControl.DSPReset;
        dsp.m_dsp_control.DSPAssertInt = tmpControl.DSPAssertInt;
        dsp.m_dsp_control.DSPHalt = tmpControl.DSPHalt;
        dsp.m_dsp_control.DSPInitCode = tmpControl.DSPInitCode;
        dsp.m_dsp_control.DSPInit = tmpControl.DSPInit;

        // Interrupt (mask)
        dsp.m_dsp_control.AID_mask = tmpControl.AID_mask;
        dsp.m_dsp_control.ARAM_mask = tmpControl.ARAM_mask;
        dsp.m_dsp_control.DSP_mask = tmpControl.DSP_mask;

        // Interrupt
        if (tmpControl.AID)
          dsp.m_dsp_control.AID = 0;
        if (tmpControl.ARAM)
          dsp.m_dsp_control.ARAM = 0;
        if (tmpControl.DSP)
          dsp.m_dsp_control.DSP = 0;

        // unknown
        dsp.m_dsp_control.pad = tmpControl.pad;
        if (dsp.m_dsp_control.pad != 0)
        {
          PanicAlertFmt(
              "DSPInterface (w) DSP state (CC00500A) gets a value with junk in the padding {:08x}",
              val);
        }

        dsp.UpdateInterrupts();
      }));

  // ARAM MMIO controlling the DMA start.
  mmio->Register(base | AR_DMA_CNT_L,
                 MMIO::DirectRead<u16>(MMIO::Utils::LowPart(&m_aram_dma.Cnt.Hex)),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& dsp = system.GetDSP();
                   dsp.m_aram_dma.Cnt.Hex =
                       (dsp.m_aram_dma.Cnt.Hex & 0xFFFF0000) | (val & WMASK_LO_ALIGN_32BIT);
                   dsp.Do_ARAM_DMA();
                 }));

  mmio->Register(base | AUDIO_DMA_START_HI,
                 MMIO::DirectRead<u16>(MMIO::Utils::HighPart(&m_audio_dma.SourceAddress)),
                 MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
                   auto& dsp = system.GetDSP();
                   *MMIO::Utils::HighPart(&dsp.m_audio_dma.SourceAddress) =
                       val &
                       (system.IsWii() ? WMASK_AUDIO_HI_RESTRICT_WII : WMASK_AUDIO_HI_RESTRICT_GCN);
                 }));

  // Audio DMA MMIO controlling the DMA start.
  mmio->Register(
      base | AUDIO_DMA_CONTROL_LEN, MMIO::DirectRead<u16>(&m_audio_dma.AudioDMAControl.Hex),
      MMIO::ComplexWrite<u16>([](Core::System& system, u32, u16 val) {
        auto& dsp = system.GetDSP();
        bool already_enabled = dsp.m_audio_dma.AudioDMAControl.Enable;
        dsp.m_audio_dma.AudioDMAControl.Hex = val;

        // Only load new values if we're not already doing a DMA transfer,
        // otherwise just let the new values be autoloaded in when the
        // current transfer ends.
        if (!already_enabled && dsp.m_audio_dma.AudioDMAControl.Enable)
        {
          dsp.m_audio_dma.current_source_address = dsp.m_audio_dma.SourceAddress;
          dsp.m_audio_dma.remaining_blocks_count = dsp.m_audio_dma.AudioDMAControl.NumBlocks;

          INFO_LOG_FMT(AUDIO_INTERFACE, "Audio DMA configured: {} blocks from {:#010x}",
                       dsp.m_audio_dma.AudioDMAControl.NumBlocks, dsp.m_audio_dma.SourceAddress);

          // TODO: need hardware tests for the timing of this interrupt.
          // Sky Crawlers crashes at boot if this is scheduled less than 87 cycles in the future.
          // Other Namco games crash too, see issue 9509. For now we will just push it to 200 cycles
          system.GetCoreTiming().ScheduleEvent(200, dsp.m_event_type_generate_dsp_interrupt,
                                               INT_AID);
        }
      }));

  // Audio DMA blocks remaining is invalid to write to, and requires logic on
  // the read side.
  mmio->Register(base | AUDIO_DMA_BLOCKS_LEFT,
                 MMIO::ComplexRead<u16>([](Core::System& system, u32) {
                   // remaining_blocks_count is zero-based.  DreamMix World Fighters will hang if it
                   // never reaches zero.
                   auto& dsp = system.GetDSP();
                   return (dsp.m_audio_dma.remaining_blocks_count > 0 ?
                               dsp.m_audio_dma.remaining_blocks_count - 1 :
                               0);
                 }),
                 MMIO::InvalidWrite<u16>());

  // 32 bit reads/writes are a combination of two 16 bit accesses.
  for (u32 i = 0; i < 0x1000; i += 4)
  {
    mmio->Register(base | i, MMIO::ReadToSmaller<u32>(mmio, base | i, base | (i + 2)),
                   MMIO::WriteToSmaller<u32>(mmio, base | i, base | (i + 2)));
  }
}

// UpdateInterrupts
void DSPManager::UpdateInterrupts()
{
  // For each interrupt bit in DSP_CONTROL, the interrupt enablemask is the bit directly
  // to the left of it. By doing:
  // (DSP_CONTROL>>1) & DSP_CONTROL & MASK_OF_ALL_INTERRUPT_BITS
  // We can check if any of the interrupts are enabled and active, all at once.
  bool ints_set =
      (((m_dsp_control.Hex >> 1) & m_dsp_control.Hex & (INT_DSP | INT_ARAM | INT_AID)) != 0);

  m_system.GetProcessorInterface().SetInterrupt(ProcessorInterface::INT_CAUSE_DSP, ints_set);
}

void DSPManager::GlobalGenerateDSPInterrupt(Core::System& system, u64 DSPIntType, s64 cyclesLate)
{
  system.GetDSP().GenerateDSPInterrupt(DSPIntType, cyclesLate);
}

void DSPManager::GenerateDSPInterrupt(u64 DSPIntType, s64 cyclesLate)
{
  // The INT_* enumeration members have values that reflect their bit positions in
  // DSP_CONTROL - we mask by (INT_DSP | INT_ARAM | INT_AID) just to ensure people
  // don't call this with bogus values.
  m_dsp_control.Hex |= (DSPIntType & (INT_DSP | INT_ARAM | INT_AID));
  UpdateInterrupts();
}

// CALLED FROM DSP EMULATOR, POSSIBLY THREADED
void DSPManager::GenerateDSPInterruptFromDSPEmu(DSPInterruptType type, int cycles_into_future)
{
  auto& core_timing = m_system.GetCoreTiming();
  core_timing.ScheduleEvent(cycles_into_future, m_event_type_generate_dsp_interrupt, type,
                            CoreTiming::FromThread::ANY);
}

// called whenever SystemTimers thinks the DSP deserves a few more cycles
void DSPManager::UpdateDSPSlice(int cycles)
{
  if (m_is_lle)
  {
    // use up the rest of the slice(if any)
    m_dsp_emulator->DSP_Update(m_dsp_slice);
    m_dsp_slice %= 6;
    // note the new budget
    m_dsp_slice += cycles;
  }
  else
  {
    m_dsp_emulator->DSP_Update(cycles);
  }
}

// This happens at 4 khz, since 32 bytes at 4khz = 4 bytes at 32 khz (16bit stereo pcm)
void DSPManager::UpdateAudioDMA()
{
  static short zero_samples[8 * 2] = {0};
  if (m_audio_dma.AudioDMAControl.Enable)
  {
    // Read audio at g_audioDMA.current_source_address in RAM and push onto an
    // external audio fifo in the emulator, to be mixed with the disc
    // streaming output.
    auto& memory = m_system.GetMemory();
    void* address = memory.GetPointerForRange(m_audio_dma.current_source_address, 32);
    AudioCommon::SendAIBuffer(m_system, reinterpret_cast<short*>(address), 8);

    if (m_audio_dma.remaining_blocks_count != 0)
    {
      m_audio_dma.remaining_blocks_count--;
      m_audio_dma.current_source_address += 32;
    }

    if (m_audio_dma.remaining_blocks_count == 0)
    {
      m_audio_dma.current_source_address = m_audio_dma.SourceAddress;
      m_audio_dma.remaining_blocks_count = m_audio_dma.AudioDMAControl.NumBlocks;

      GenerateDSPInterrupt(DSP::INT_AID, 0);
    }
  }
  else
  {
    AudioCommon::SendAIBuffer(m_system, &zero_samples[0], 8);
  }
}

void DSPManager::Do_ARAM_DMA()
{
  auto& core_timing = m_system.GetCoreTiming();
  auto& memory = m_system.GetMemory();

  m_dsp_control.DMAState = 1;

  // ARAM DMA transfer rate has been measured on real hw
  int ticksToTransfer = (m_aram_dma.Cnt.count / 32) * 246;
  core_timing.ScheduleEvent(ticksToTransfer, m_event_type_complete_aram);

  // Real hardware DMAs in 32byte chunks, but we can get by with 8byte chunks
  if (m_aram_dma.Cnt.dir)
  {
    // ARAM -> MRAM
    DEBUG_LOG_FMT(DSPINTERFACE, "DMA {:08x} bytes from ARAM {:08x} to MRAM {:08x} PC: {:08x}",
                  m_aram_dma.Cnt.count, m_aram_dma.ARAddr, m_aram_dma.MMAddr,
                  m_system.GetPPCState().pc);

    // Outgoing data from ARAM is mirrored every 64MB (verified on real HW)
    m_aram_dma.ARAddr &= 0x3ffffff;
    m_aram_dma.MMAddr &= 0x3ffffff;

    if (m_aram_dma.ARAddr < m_aram.size)
    {
      while (m_aram_dma.Cnt.count)
      {
        // These are logically separated in code to show that a memory map has been set up
        // See below in the write section for more information
        if ((m_aram_info.Hex & 0xf) == 3)
        {
          memory.Write_U64_Swap(*(u64*)&m_aram.ptr[m_aram_dma.ARAddr & m_aram.mask],
                                m_aram_dma.MMAddr);
        }
        else if ((m_aram_info.Hex & 0xf) == 4)
        {
          memory.Write_U64_Swap(*(u64*)&m_aram.ptr[m_aram_dma.ARAddr & m_aram.mask],
                                m_aram_dma.MMAddr);
        }
        else
        {
          memory.Write_U64_Swap(*(u64*)&m_aram.ptr[m_aram_dma.ARAddr & m_aram.mask],
                                m_aram_dma.MMAddr);
        }

        m_aram_dma.MMAddr += 8;
        m_aram_dma.ARAddr += 8;
        m_aram_dma.Cnt.count -= 8;
      }
    }
    else if (!m_aram.wii_mode)
    {
      while (m_aram_dma.Cnt.count)
      {
        memory.Write_U64(m_system.GetHSP().Read(m_aram_dma.ARAddr), m_aram_dma.MMAddr);
        m_aram_dma.MMAddr += 8;
        m_aram_dma.ARAddr += 8;
        m_aram_dma.Cnt.count -= 8;
      }
    }
  }
  else
  {
    // MRAM -> ARAM
    DEBUG_LOG_FMT(DSPINTERFACE, "DMA {:08x} bytes from MRAM {:08x} to ARAM {:08x} PC: {:08x}",
                  m_aram_dma.Cnt.count, m_aram_dma.MMAddr, m_aram_dma.ARAddr,
                  m_system.GetPPCState().pc);

    // Incoming data into ARAM is mirrored every 64MB (verified on real HW)
    m_aram_dma.ARAddr &= 0x3ffffff;
    m_aram_dma.MMAddr &= 0x3ffffff;

    if (m_aram_dma.ARAddr < m_aram.size)
    {
      while (m_aram_dma.Cnt.count)
      {
        if ((m_aram_info.Hex & 0xf) == 3)
        {
          *(u64*)&m_aram.ptr[m_aram_dma.ARAddr & m_aram.mask] =
              Common::swap64(memory.Read_U64(m_aram_dma.MMAddr));
        }
        else if ((m_aram_info.Hex & 0xf) == 4)
        {
          if (m_aram_dma.ARAddr < 0x400000)
          {
            *(u64*)&m_aram.ptr[(m_aram_dma.ARAddr + 0x400000) & m_aram.mask] =
                Common::swap64(memory.Read_U64(m_aram_dma.MMAddr));
          }
          *(u64*)&m_aram.ptr[m_aram_dma.ARAddr & m_aram.mask] =
              Common::swap64(memory.Read_U64(m_aram_dma.MMAddr));
        }
        else
        {
          *(u64*)&m_aram.ptr[m_aram_dma.ARAddr & m_aram.mask] =
              Common::swap64(memory.Read_U64(m_aram_dma.MMAddr));
        }

        m_aram_dma.MMAddr += 8;
        m_aram_dma.ARAddr += 8;
        m_aram_dma.Cnt.count -= 8;
      }
    }
    else if (!m_aram.wii_mode)
    {
      while (m_aram_dma.Cnt.count)
      {
        m_system.GetHSP().Write(m_aram_dma.ARAddr, memory.Read_U64(m_aram_dma.MMAddr));

        m_aram_dma.MMAddr += 8;
        m_aram_dma.ARAddr += 8;
        m_aram_dma.Cnt.count -= 8;
      }
    }
  }
}

// (shuffle2) I still don't believe that this hack is actually needed... :(
// Maybe the Wii Sports ucode is processed incorrectly?
// (LM) It just means that DSP reads via '0xffdd' on Wii can end up in EXRAM or main RAM
u8 DSPManager::ReadARAM(u32 address) const
{
  if (m_aram.wii_mode)
  {
    if (address & 0x10000000)
    {
      return m_aram.ptr[address & m_aram.mask];
    }
    else
    {
      auto& memory = m_system.GetMemory();
      return memory.Read_U8(address & memory.GetRamMask());
    }
  }
  else
  {
    return m_aram.ptr[address & m_aram.mask];
  }
}

void DSPManager::WriteARAM(u8 value, u32 address)
{
  // TODO: verify this on Wii
  m_aram.ptr[address & m_aram.mask] = value;
}

u8* DSPManager::GetARAMPtr() const
{
  return m_aram.ptr;
}

}  // end of namespace DSP
