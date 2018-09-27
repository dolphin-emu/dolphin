// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/DSPEmulator.h"

#include "Core/HW/MMIO.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/PowerPC.h"

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

// UARAMCount
union UARAMCount
{
  u32 Hex = 0;
  struct
  {
    u32 count : 31;
    u32 dir : 1;  // 0: MRAM -> ARAM 1: ARAM -> MRAM
  };
};

// Blocks are 32 bytes.
union UAudioDMAControl
{
  u16 Hex = 0;
  struct
  {
    u16 NumBlocks : 15;
    u16 Enable : 1;
  };
};

// AudioDMA
struct AudioDMA
{
  u32 current_source_address = 0;
  u16 remaining_blocks_count = 0;
  u32 SourceAddress = 0;
  UAudioDMAControl AudioDMAControl;
};

// ARAM_DMA
struct ARAM_DMA
{
  u32 MMAddr = 0;
  u32 ARAddr = 0;
  UARAMCount Cnt;
};

// So we may abstract GC/Wii differences a little
struct ARAMInfo
{
  bool wii_mode = false;  // Wii EXRAM is managed in Memory:: so we need to skip statesaving, etc
  u32 size = ARAM_SIZE;
  u32 mask = ARAM_MASK;
  u8* ptr = nullptr;  // aka audio ram, auxiliary ram, MEM2, EXRAM, etc...
};

union ARAM_Info
{
  u16 Hex = 0;
  struct
  {
    u16 size : 6;
    u16 unk : 1;
    u16 : 9;
  };
};

// STATE_TO_SAVE
static ARAMInfo s_ARAM;
static AudioDMA s_audioDMA;
static ARAM_DMA s_arDMA;
static UDSPControl s_dspState;
static ARAM_Info s_ARAM_Info;
// Contains bitfields for some stuff we don't care about (and nothing ever reads):
//  CAS latency/burst length/addressing mode/write mode
// We care about the LSB tho. It indicates that the ARAM controller has finished initializing
static u16 s_AR_MODE;
static u16 s_AR_REFRESH;
static int s_dsp_slice = 0;

static std::unique_ptr<DSPEmulator> s_dsp_emulator;

static bool s_dsp_is_lle = false;

// time given to LLE DSP on every read of the high bits in a mailbox
static const int DSP_MAIL_SLICE = 72;

void DoState(PointerWrap& p)
{
  if (!s_ARAM.wii_mode)
    p.DoArray(s_ARAM.ptr, s_ARAM.size);
  p.DoPOD(s_dspState);
  p.DoPOD(s_audioDMA);
  p.DoPOD(s_arDMA);
  p.Do(s_ARAM_Info);
  p.Do(s_AR_MODE);
  p.Do(s_AR_REFRESH);
  p.Do(s_dsp_slice);

  s_dsp_emulator->DoState(p);
}

static void UpdateInterrupts();
static void Do_ARAM_DMA();
static void GenerateDSPInterrupt(u64 DSPIntType, s64 cyclesLate = 0);

static CoreTiming::EventType* s_et_GenerateDSPInterrupt;
static CoreTiming::EventType* s_et_CompleteARAM;

static void CompleteARAM(u64 userdata, s64 cyclesLate)
{
  s_dspState.DMAState = 0;
  GenerateDSPInterrupt(INT_ARAM);
}

DSPEmulator* GetDSPEmulator()
{
  return s_dsp_emulator.get();
}

void Init(bool hle)
{
  Reinit(hle);
  s_et_GenerateDSPInterrupt = CoreTiming::RegisterEvent("DSPint", GenerateDSPInterrupt);
  s_et_CompleteARAM = CoreTiming::RegisterEvent("ARAMint", CompleteARAM);
}

void Reinit(bool hle)
{
  s_dsp_emulator = CreateDSPEmulator(hle);
  s_dsp_is_lle = s_dsp_emulator->IsLLE();

  if (SConfig::GetInstance().bWii)
  {
    s_ARAM.wii_mode = true;
    s_ARAM.size = Memory::EXRAM_SIZE;
    s_ARAM.mask = Memory::EXRAM_MASK;
    s_ARAM.ptr = Memory::m_pEXRAM;
  }
  else
  {
    // On the GameCube, ARAM is accessible only through this interface.
    s_ARAM.wii_mode = false;
    s_ARAM.size = ARAM_SIZE;
    s_ARAM.mask = ARAM_MASK;
    s_ARAM.ptr = static_cast<u8*>(Common::AllocateMemoryPages(s_ARAM.size));
  }

  s_audioDMA = {};
  s_arDMA = {};

  s_dspState.Hex = 0;
  s_dspState.DSPHalt = 1;

  s_ARAM_Info.Hex = 0;
  s_AR_MODE = 1;       // ARAM Controller has init'd
  s_AR_REFRESH = 156;  // 156MHz
}

void Shutdown()
{
  if (!s_ARAM.wii_mode)
  {
    Common::FreeMemoryPages(s_ARAM.ptr, s_ARAM.size);
    s_ARAM.ptr = nullptr;
  }

  s_dsp_emulator->Shutdown();
  s_dsp_emulator.reset();
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  // Declare all the boilerplate direct MMIOs.
  struct
  {
    u32 addr;
    u16* ptr;
    bool align_writes_on_32_bytes;
  } directly_mapped_vars[] = {
      {AR_INFO, &s_ARAM_Info.Hex},
      {AR_MODE, &s_AR_MODE},
      {AR_REFRESH, &s_AR_REFRESH},
      {AR_DMA_MMADDR_H, MMIO::Utils::HighPart(&s_arDMA.MMAddr)},
      {AR_DMA_MMADDR_L, MMIO::Utils::LowPart(&s_arDMA.MMAddr), true},
      {AR_DMA_ARADDR_H, MMIO::Utils::HighPart(&s_arDMA.ARAddr)},
      {AR_DMA_ARADDR_L, MMIO::Utils::LowPart(&s_arDMA.ARAddr), true},
      {AR_DMA_CNT_H, MMIO::Utils::HighPart(&s_arDMA.Cnt.Hex)},
      // AR_DMA_CNT_L triggers DMA
      {AUDIO_DMA_START_HI, MMIO::Utils::HighPart(&s_audioDMA.SourceAddress)},
      {AUDIO_DMA_START_LO, MMIO::Utils::LowPart(&s_audioDMA.SourceAddress)},
  };
  for (auto& mapped_var : directly_mapped_vars)
  {
    u16 write_mask = mapped_var.align_writes_on_32_bytes ? 0xFFE0 : 0xFFFF;
    mmio->Register(base | mapped_var.addr, MMIO::DirectRead<u16>(mapped_var.ptr),
                   MMIO::DirectWrite<u16>(mapped_var.ptr, write_mask));
  }

  // DSP mail MMIOs call DSP emulator functions to get results or write data.
  mmio->Register(base | DSP_MAIL_TO_DSP_HI, MMIO::ComplexRead<u16>([](u32) {
                   if (s_dsp_slice > DSP_MAIL_SLICE && s_dsp_is_lle)
                   {
                     s_dsp_emulator->DSP_Update(DSP_MAIL_SLICE);
                     s_dsp_slice -= DSP_MAIL_SLICE;
                   }
                   return s_dsp_emulator->DSP_ReadMailBoxHigh(true);
                 }),
                 MMIO::ComplexWrite<u16>(
                     [](u32, u16 val) { s_dsp_emulator->DSP_WriteMailBoxHigh(true, val); }));
  mmio->Register(base | DSP_MAIL_TO_DSP_LO, MMIO::ComplexRead<u16>([](u32) {
                   return s_dsp_emulator->DSP_ReadMailBoxLow(true);
                 }),
                 MMIO::ComplexWrite<u16>(
                     [](u32, u16 val) { s_dsp_emulator->DSP_WriteMailBoxLow(true, val); }));
  mmio->Register(base | DSP_MAIL_FROM_DSP_HI, MMIO::ComplexRead<u16>([](u32) {
                   if (s_dsp_slice > DSP_MAIL_SLICE && s_dsp_is_lle)
                   {
                     s_dsp_emulator->DSP_Update(DSP_MAIL_SLICE);
                     s_dsp_slice -= DSP_MAIL_SLICE;
                   }
                   return s_dsp_emulator->DSP_ReadMailBoxHigh(false);
                 }),
                 MMIO::InvalidWrite<u16>());
  mmio->Register(base | DSP_MAIL_FROM_DSP_LO, MMIO::ComplexRead<u16>([](u32) {
                   return s_dsp_emulator->DSP_ReadMailBoxLow(false);
                 }),
                 MMIO::InvalidWrite<u16>());

  mmio->Register(
      base | DSP_CONTROL, MMIO::ComplexRead<u16>([](u32) {
        return (s_dspState.Hex & ~DSP_CONTROL_MASK) |
               (s_dsp_emulator->DSP_ReadControlRegister() & DSP_CONTROL_MASK);
      }),
      MMIO::ComplexWrite<u16>([](u32, u16 val) {
        UDSPControl tmpControl;
        tmpControl.Hex = (val & ~DSP_CONTROL_MASK) |
                         (s_dsp_emulator->DSP_WriteControlRegister(val) & DSP_CONTROL_MASK);

        // Not really sure if this is correct, but it works...
        // Kind of a hack because DSP_CONTROL_MASK should make this bit
        // only viewable to DSP emulator
        if (val & 1 /*DSPReset*/)
        {
          s_audioDMA.AudioDMAControl.Hex = 0;
        }

        // Update DSP related flags
        s_dspState.DSPReset = tmpControl.DSPReset;
        s_dspState.DSPAssertInt = tmpControl.DSPAssertInt;
        s_dspState.DSPHalt = tmpControl.DSPHalt;
        s_dspState.DSPInit = tmpControl.DSPInit;

        // Interrupt (mask)
        s_dspState.AID_mask = tmpControl.AID_mask;
        s_dspState.ARAM_mask = tmpControl.ARAM_mask;
        s_dspState.DSP_mask = tmpControl.DSP_mask;

        // Interrupt
        if (tmpControl.AID)
          s_dspState.AID = 0;
        if (tmpControl.ARAM)
          s_dspState.ARAM = 0;
        if (tmpControl.DSP)
          s_dspState.DSP = 0;

        // unknown
        s_dspState.DSPInitCode = tmpControl.DSPInitCode;
        s_dspState.pad = tmpControl.pad;
        if (s_dspState.pad != 0)
        {
          PanicAlert(
              "DSPInterface (w) DSP state (CC00500A) gets a value with junk in the padding %08x",
              val);
        }

        UpdateInterrupts();
      }));

  // ARAM MMIO controlling the DMA start.
  mmio->Register(base | AR_DMA_CNT_L, MMIO::DirectRead<u16>(MMIO::Utils::LowPart(&s_arDMA.Cnt.Hex)),
                 MMIO::ComplexWrite<u16>([](u32, u16 val) {
                   s_arDMA.Cnt.Hex = (s_arDMA.Cnt.Hex & 0xFFFF0000) | (val & ~31);
                   Do_ARAM_DMA();
                 }));

  // Audio DMA MMIO controlling the DMA start.
  mmio->Register(
      base | AUDIO_DMA_CONTROL_LEN, MMIO::DirectRead<u16>(&s_audioDMA.AudioDMAControl.Hex),
      MMIO::ComplexWrite<u16>([](u32, u16 val) {
        bool already_enabled = s_audioDMA.AudioDMAControl.Enable;
        s_audioDMA.AudioDMAControl.Hex = val;

        // Only load new values if were not already doing a DMA transfer,
        // otherwise just let the new values be autoloaded in when the
        // current transfer ends.
        if (!already_enabled && s_audioDMA.AudioDMAControl.Enable)
        {
          s_audioDMA.current_source_address = s_audioDMA.SourceAddress;
          s_audioDMA.remaining_blocks_count = s_audioDMA.AudioDMAControl.NumBlocks;

          INFO_LOG(AUDIO_INTERFACE, "Audio DMA configured: %i blocks from 0x%08x",
                   s_audioDMA.AudioDMAControl.NumBlocks, s_audioDMA.SourceAddress);

          // We make the samples ready as soon as possible
          void* address = Memory::GetPointer(s_audioDMA.SourceAddress);
          AudioCommon::SendAIBuffer((short*)address, s_audioDMA.AudioDMAControl.NumBlocks * 8);

          // TODO: need hardware tests for the timing of this interrupt.
          // Sky Crawlers crashes at boot if this is scheduled less than 87 cycles in the future.
          // Other Namco games crash too, see issue 9509. For now we will just push it to 200 cycles
          CoreTiming::ScheduleEvent(200, s_et_GenerateDSPInterrupt, INT_AID);
        }
      }));

  // Audio DMA blocks remaining is invalid to write to, and requires logic on
  // the read side.
  mmio->Register(
      base | AUDIO_DMA_BLOCKS_LEFT, MMIO::ComplexRead<u16>([](u32) {
        // remaining_blocks_count is zero-based.  DreamMix World Fighters will hang if it never
        // reaches zero.
        return (s_audioDMA.remaining_blocks_count > 0 ? s_audioDMA.remaining_blocks_count - 1 : 0);
      }),
      MMIO::InvalidWrite<u16>());

  // 32 bit reads/writes are a combination of two 16 bit accesses.
  for (int i = 0; i < 0x1000; i += 4)
  {
    mmio->Register(base | i, MMIO::ReadToSmaller<u32>(mmio, base | i, base | (i + 2)),
                   MMIO::WriteToSmaller<u32>(mmio, base | i, base | (i + 2)));
  }
}

// UpdateInterrupts
static void UpdateInterrupts()
{
  // For each interrupt bit in DSP_CONTROL, the interrupt enablemask is the bit directly
  // to the left of it. By doing:
  // (DSP_CONTROL>>1) & DSP_CONTROL & MASK_OF_ALL_INTERRUPT_BITS
  // We can check if any of the interrupts are enabled and active, all at once.
  bool ints_set = (((s_dspState.Hex >> 1) & s_dspState.Hex & (INT_DSP | INT_ARAM | INT_AID)) != 0);

  ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_DSP, ints_set);
}

static void GenerateDSPInterrupt(u64 DSPIntType, s64 cyclesLate)
{
  // The INT_* enumeration members have values that reflect their bit positions in
  // DSP_CONTROL - we mask by (INT_DSP | INT_ARAM | INT_AID) just to ensure people
  // don't call this with bogus values.
  s_dspState.Hex |= (DSPIntType & (INT_DSP | INT_ARAM | INT_AID));
  UpdateInterrupts();
}

// CALLED FROM DSP EMULATOR, POSSIBLY THREADED
void GenerateDSPInterruptFromDSPEmu(DSPInterruptType type, int cycles_into_future)
{
  CoreTiming::ScheduleEvent(cycles_into_future, s_et_GenerateDSPInterrupt, type,
                            CoreTiming::FromThread::ANY);
}

// called whenever SystemTimers thinks the DSP deserves a few more cycles
void UpdateDSPSlice(int cycles)
{
  if (s_dsp_is_lle)
  {
    // use up the rest of the slice(if any)
    s_dsp_emulator->DSP_Update(s_dsp_slice);
    s_dsp_slice %= 6;
    // note the new budget
    s_dsp_slice += cycles;
  }
  else
  {
    s_dsp_emulator->DSP_Update(cycles);
  }
}

// This happens at 4 khz, since 32 bytes at 4khz = 4 bytes at 32 khz (16bit stereo pcm)
void UpdateAudioDMA()
{
  static short zero_samples[8 * 2] = {0};
  if (s_audioDMA.AudioDMAControl.Enable)
  {
    // Read audio at g_audioDMA.current_source_address in RAM and push onto an
    // external audio fifo in the emulator, to be mixed with the disc
    // streaming output.

    if (s_audioDMA.remaining_blocks_count != 0)
    {
      s_audioDMA.remaining_blocks_count--;
      s_audioDMA.current_source_address += 32;
    }

    if (s_audioDMA.remaining_blocks_count == 0)
    {
      s_audioDMA.current_source_address = s_audioDMA.SourceAddress;
      s_audioDMA.remaining_blocks_count = s_audioDMA.AudioDMAControl.NumBlocks;

      if (s_audioDMA.remaining_blocks_count != 0)
      {
        // We make the samples ready as soon as possible
        void* address = Memory::GetPointer(s_audioDMA.SourceAddress);
        AudioCommon::SendAIBuffer((short*)address, s_audioDMA.AudioDMAControl.NumBlocks * 8);
      }
      GenerateDSPInterrupt(DSP::INT_AID);
    }
  }
  else
  {
    AudioCommon::SendAIBuffer(&zero_samples[0], 8);
  }
}

static void Do_ARAM_DMA()
{
  s_dspState.DMAState = 1;

  if (SConfig::GetInstance().m_DSPInterruptHack)
    GenerateDSPInterrupt(INT_ARAM);

  // ARAM DMA transfer rate has been measured on real hw
  int ticksToTransfer = (s_arDMA.Cnt.count / 32) * 246;
  CoreTiming::ScheduleEvent(ticksToTransfer, s_et_CompleteARAM);

  // Real hardware DMAs in 32byte chunks, but we can get by with 8byte chunks
  if (s_arDMA.Cnt.dir)
  {
    // ARAM -> MRAM
    DEBUG_LOG(DSPINTERFACE, "DMA %08x bytes from ARAM %08x to MRAM %08x PC: %08x",
              s_arDMA.Cnt.count, s_arDMA.ARAddr, s_arDMA.MMAddr, PC);

    // Outgoing data from ARAM is mirrored every 64MB (verified on real HW)
    s_arDMA.ARAddr &= 0x3ffffff;
    s_arDMA.MMAddr &= 0x3ffffff;

    if (s_arDMA.ARAddr < s_ARAM.size)
    {
      while (s_arDMA.Cnt.count)
      {
        // These are logically separated in code to show that a memory map has been set up
        // See below in the write section for more information
        if ((s_ARAM_Info.Hex & 0xf) == 3)
        {
          Memory::Write_U64_Swap(*(u64*)&s_ARAM.ptr[s_arDMA.ARAddr & s_ARAM.mask], s_arDMA.MMAddr);
        }
        else if ((s_ARAM_Info.Hex & 0xf) == 4)
        {
          Memory::Write_U64_Swap(*(u64*)&s_ARAM.ptr[s_arDMA.ARAddr & s_ARAM.mask], s_arDMA.MMAddr);
        }
        else
        {
          Memory::Write_U64_Swap(*(u64*)&s_ARAM.ptr[s_arDMA.ARAddr & s_ARAM.mask], s_arDMA.MMAddr);
        }

        s_arDMA.MMAddr += 8;
        s_arDMA.ARAddr += 8;
        s_arDMA.Cnt.count -= 8;
      }
    }
    else
    {
      // Assuming no external ARAM installed; returns zeros on out of bounds reads (verified on real
      // HW)
      while (s_arDMA.Cnt.count)
      {
        Memory::Write_U64(0, s_arDMA.MMAddr);
        s_arDMA.MMAddr += 8;
        s_arDMA.ARAddr += 8;
        s_arDMA.Cnt.count -= 8;
      }
    }
  }
  else
  {
    // MRAM -> ARAM
    DEBUG_LOG(DSPINTERFACE, "DMA %08x bytes from MRAM %08x to ARAM %08x PC: %08x",
              s_arDMA.Cnt.count, s_arDMA.MMAddr, s_arDMA.ARAddr, PC);

    // Incoming data into ARAM is mirrored every 64MB (verified on real HW)
    s_arDMA.ARAddr &= 0x3ffffff;
    s_arDMA.MMAddr &= 0x3ffffff;

    if (s_arDMA.ARAddr < s_ARAM.size)
    {
      while (s_arDMA.Cnt.count)
      {
        if ((s_ARAM_Info.Hex & 0xf) == 3)
        {
          *(u64*)&s_ARAM.ptr[s_arDMA.ARAddr & s_ARAM.mask] =
              Common::swap64(Memory::Read_U64(s_arDMA.MMAddr));
        }
        else if ((s_ARAM_Info.Hex & 0xf) == 4)
        {
          if (s_arDMA.ARAddr < 0x400000)
          {
            *(u64*)&s_ARAM.ptr[(s_arDMA.ARAddr + 0x400000) & s_ARAM.mask] =
                Common::swap64(Memory::Read_U64(s_arDMA.MMAddr));
          }
          *(u64*)&s_ARAM.ptr[s_arDMA.ARAddr & s_ARAM.mask] =
              Common::swap64(Memory::Read_U64(s_arDMA.MMAddr));
        }
        else
        {
          *(u64*)&s_ARAM.ptr[s_arDMA.ARAddr & s_ARAM.mask] =
              Common::swap64(Memory::Read_U64(s_arDMA.MMAddr));
        }

        s_arDMA.MMAddr += 8;
        s_arDMA.ARAddr += 8;
        s_arDMA.Cnt.count -= 8;
      }
    }
    else
    {
      // Assuming no external ARAM installed; writes nothing to ARAM when out of bounds (verified on
      // real HW)
      s_arDMA.MMAddr += s_arDMA.Cnt.count;
      s_arDMA.ARAddr += s_arDMA.Cnt.count;
      s_arDMA.Cnt.count = 0;
    }
  }
}

// (shuffle2) I still don't believe that this hack is actually needed... :(
// Maybe the Wii Sports ucode is processed incorrectly?
// (LM) It just means that DSP reads via '0xffdd' on Wii can end up in EXRAM or main RAM
u8 ReadARAM(u32 address)
{
  if (s_ARAM.wii_mode)
  {
    if (address & 0x10000000)
      return s_ARAM.ptr[address & s_ARAM.mask];
    else
      return Memory::Read_U8(address & Memory::RAM_MASK);
  }
  else
  {
    return s_ARAM.ptr[address & s_ARAM.mask];
  }
}

void WriteARAM(u8 value, u32 address)
{
  // TODO: verify this on Wii
  s_ARAM.ptr[address & s_ARAM.mask] = value;
}

u8* GetARAMPtr()
{
  return s_ARAM.ptr;
}

}  // end of namespace DSP
