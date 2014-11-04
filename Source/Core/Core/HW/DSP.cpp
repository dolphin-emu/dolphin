// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
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

#include "AudioCommon/AudioCommon.h"

#include "Common/MemoryUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/DSPEmulator.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/CPU.h"
#include "Core/HW/DSP.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"

namespace DSP
{

// register offsets
enum
{
	DSP_MAIL_TO_DSP_HI      = 0x5000,
	DSP_MAIL_TO_DSP_LO      = 0x5002,
	DSP_MAIL_FROM_DSP_HI    = 0x5004,
	DSP_MAIL_FROM_DSP_LO    = 0x5006,
	DSP_CONTROL             = 0x500A,
	DSP_INTERRUPT_CONTROL   = 0x5010,
	AR_INFO                 = 0x5012,  // These names are a good guess at best
	AR_MODE                 = 0x5016,  //
	AR_REFRESH              = 0x501a,
	AR_DMA_MMADDR_H         = 0x5020,
	AR_DMA_MMADDR_L         = 0x5022,
	AR_DMA_ARADDR_H         = 0x5024,
	AR_DMA_ARADDR_L         = 0x5026,
	AR_DMA_CNT_H            = 0x5028,
	AR_DMA_CNT_L            = 0x502A,
	AUDIO_DMA_START_HI      = 0x5030,
	AUDIO_DMA_START_LO      = 0x5032,
	AUDIO_DMA_BLOCKS_LENGTH = 0x5034,  // Ever used?
	AUDIO_DMA_CONTROL_LEN   = 0x5036,
	AUDIO_DMA_BLOCKS_LEFT   = 0x503A,
};

// UARAMCount
union UARAMCount
{
	u32 Hex;
	struct
	{
		u32 count : 31;
		u32 dir   : 1; // 0: MRAM -> ARAM 1: ARAM -> MRAM
	};
};

// DSPState
struct DSPState
{
	UDSPControl DSPControl;
	DSPState()
	{
		DSPControl.Hex = 0;
	}
};

// Blocks are 32 bytes.
union UAudioDMAControl
{
	u16 Hex;
	struct
	{
		u16 NumBlocks  : 15;
		u16 Enable     : 1;
	};

	UAudioDMAControl(u16 _Hex = 0) : Hex(_Hex)
	{}
};

// AudioDMA
struct AudioDMA
{
	u32 current_source_address;
	u16 remaining_blocks_count;
	u32 SourceAddress;
	UAudioDMAControl AudioDMAControl;

	AudioDMA():
		current_source_address(0),
		remaining_blocks_count(0),
		SourceAddress(0),
		AudioDMAControl(0)
	{
	}
};

// ARAM_DMA
struct ARAM_DMA
{
	u32 MMAddr;
	u32 ARAddr;
	UARAMCount Cnt;

	ARAM_DMA()
	{
		MMAddr = 0;
		ARAddr = 0;
		Cnt.Hex = 0;
	}
};

// So we may abstract GC/Wii differences a little
struct ARAMInfo
{
	bool wii_mode; // Wii EXRAM is managed in Memory:: so we need to skip statesaving, etc
	u32 size;
	u32 mask;
	u8* ptr; // aka audio ram, auxiliary ram, MEM2, EXRAM, etc...

	// Default to GC mode
	ARAMInfo()
	{
		wii_mode = false;
		size = ARAM_SIZE;
		mask = ARAM_MASK;
		ptr = nullptr;
	}
};

// STATE_TO_SAVE
static ARAMInfo g_ARAM;
static DSPState g_dspState;
static AudioDMA g_audioDMA;
static ARAM_DMA g_arDMA;
static u32 last_mmaddr;
static u32 last_aram_dma_count;
static bool instant_dma;

union ARAM_Info
{
	u16 Hex;
	struct
	{
		u16 size : 6;
		u16 unk  : 1;
		u16      : 9;
	};
};
static ARAM_Info g_ARAM_Info;
// Contains bitfields for some stuff we don't care about (and nothing ever reads):
//  CAS latency/burst length/addressing mode/write mode
// We care about the LSB tho. It indicates that the ARAM controller has finished initializing
static u16 g_AR_MODE;
static u16 g_AR_REFRESH;

static DSPEmulator *dsp_emulator;

static int dsp_slice = 0;
static bool dsp_is_lle = false;

//time given to lle dsp on every read of the high bits in a mailbox
static const int DSP_MAIL_SLICE=72;

void DoState(PointerWrap &p)
{
	if (!g_ARAM.wii_mode)
		p.DoArray(g_ARAM.ptr, g_ARAM.size);
	p.DoPOD(g_dspState);
	p.DoPOD(g_audioDMA);
	p.DoPOD(g_arDMA);
	p.Do(g_ARAM_Info);
	p.Do(g_AR_MODE);
	p.Do(g_AR_REFRESH);
	p.Do(dsp_slice);
	p.Do(last_mmaddr);
	p.Do(last_aram_dma_count);
	p.Do(instant_dma);

	dsp_emulator->DoState(p);
}


static void UpdateInterrupts();
static void Do_ARAM_DMA();
static void GenerateDSPInterrupt(u64 DSPIntType, int cyclesLate = 0);

static int et_GenerateDSPInterrupt;
static int et_CompleteARAM;

static void CompleteARAM(u64 userdata, int cyclesLate)
{
	g_dspState.DSPControl.DMAState = 0;
	GenerateDSPInterrupt(INT_ARAM);
}

void EnableInstantDMA()
{
	CoreTiming::RemoveEvent(et_CompleteARAM);
	CompleteARAM(0, 0);
	instant_dma = true;
}

DSPEmulator *GetDSPEmulator()
{
	return dsp_emulator;
}

void Init(bool hle)
{
	dsp_emulator = CreateDSPEmulator(hle);
	dsp_is_lle = dsp_emulator->IsLLE();

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
	{
		g_ARAM.wii_mode = true;
		g_ARAM.size = Memory::EXRAM_SIZE;
		g_ARAM.mask = Memory::EXRAM_MASK;
		g_ARAM.ptr = Memory::m_pEXRAM;
	}
	else
	{
		// On the GC, ARAM is accessible only through this interface.
		g_ARAM.wii_mode = false;
		g_ARAM.size = ARAM_SIZE;
		g_ARAM.mask = ARAM_MASK;
		g_ARAM.ptr = (u8 *)AllocateMemoryPages(g_ARAM.size);
	}

	memset(&g_audioDMA, 0, sizeof(g_audioDMA));
	memset(&g_arDMA, 0, sizeof(g_arDMA));

	g_dspState.DSPControl.Hex = 0;
	g_dspState.DSPControl.DSPHalt = 1;

	g_ARAM_Info.Hex = 0;
	g_AR_MODE = 1; // ARAM Controller has init'd
	g_AR_REFRESH = 156; // 156MHz

	instant_dma = false;

	last_aram_dma_count = 0;
	last_mmaddr = 0;

	et_GenerateDSPInterrupt = CoreTiming::RegisterEvent("DSPint", GenerateDSPInterrupt);
	et_CompleteARAM = CoreTiming::RegisterEvent("ARAMint", CompleteARAM);
}

void Shutdown()
{
	if (!g_ARAM.wii_mode)
	{
		FreeMemoryPages(g_ARAM.ptr, g_ARAM.size);
		g_ARAM.ptr = nullptr;
	}

	dsp_emulator->Shutdown();
	delete dsp_emulator;
	dsp_emulator = nullptr;
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
		{ AR_INFO, &g_ARAM_Info.Hex },
		{ AR_MODE, &g_AR_MODE },
		{ AR_REFRESH, &g_AR_REFRESH },
		{ AR_DMA_MMADDR_H, MMIO::Utils::HighPart(&g_arDMA.MMAddr) },
		{ AR_DMA_MMADDR_L, MMIO::Utils::LowPart(&g_arDMA.MMAddr), true },
		{ AR_DMA_ARADDR_H, MMIO::Utils::HighPart(&g_arDMA.ARAddr) },
		{ AR_DMA_ARADDR_L, MMIO::Utils::LowPart(&g_arDMA.ARAddr), true },
		{ AR_DMA_CNT_H, MMIO::Utils::HighPart(&g_arDMA.Cnt.Hex) },
		// AR_DMA_CNT_L triggers DMA
		{ AUDIO_DMA_START_HI, MMIO::Utils::HighPart(&g_audioDMA.SourceAddress) },
		{ AUDIO_DMA_START_LO, MMIO::Utils::LowPart(&g_audioDMA.SourceAddress) },
	};
	for (auto& mapped_var : directly_mapped_vars)
	{
		u16 write_mask = mapped_var.align_writes_on_32_bytes ? 0xFFE0 : 0xFFFF;
		mmio->Register(base | mapped_var.addr,
			MMIO::DirectRead<u16>(mapped_var.ptr),
			MMIO::DirectWrite<u16>(mapped_var.ptr, write_mask)
		);
	}

	// DSP mail MMIOs call DSP emulator functions to get results or write data.
	mmio->Register(base | DSP_MAIL_TO_DSP_HI,
		MMIO::ComplexRead<u16>([](u32) {
			if (dsp_slice > DSP_MAIL_SLICE && dsp_is_lle)
			{
				dsp_emulator->DSP_Update(DSP_MAIL_SLICE);
				dsp_slice -= DSP_MAIL_SLICE;
			}
			return dsp_emulator->DSP_ReadMailBoxHigh(true);
		}),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			dsp_emulator->DSP_WriteMailBoxHigh(true, val);
		})
	);
	mmio->Register(base | DSP_MAIL_TO_DSP_LO,
		MMIO::ComplexRead<u16>([](u32) {
			return dsp_emulator->DSP_ReadMailBoxLow(true);
		}),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			dsp_emulator->DSP_WriteMailBoxLow(true, val);
		})
	);
	mmio->Register(base | DSP_MAIL_FROM_DSP_HI,
		MMIO::ComplexRead<u16>([](u32) {
			if (dsp_slice > DSP_MAIL_SLICE && dsp_is_lle)
			{
				dsp_emulator->DSP_Update(DSP_MAIL_SLICE);
				dsp_slice -= DSP_MAIL_SLICE;
			}
			return dsp_emulator->DSP_ReadMailBoxHigh(false);
		}),
		MMIO::InvalidWrite<u16>()
	);
	mmio->Register(base | DSP_MAIL_FROM_DSP_LO,
		MMIO::ComplexRead<u16>([](u32) {
			return dsp_emulator->DSP_ReadMailBoxLow(false);
		}),
		MMIO::InvalidWrite<u16>()
	);

	mmio->Register(base | DSP_CONTROL,
		MMIO::ComplexRead<u16>([](u32) {
			return (g_dspState.DSPControl.Hex & ~DSP_CONTROL_MASK) |
			       (dsp_emulator->DSP_ReadControlRegister() & DSP_CONTROL_MASK);
		}),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			UDSPControl tmpControl;
			tmpControl.Hex = (val & ~DSP_CONTROL_MASK) |
				(dsp_emulator->DSP_WriteControlRegister(val) & DSP_CONTROL_MASK);

			// Not really sure if this is correct, but it works...
			// Kind of a hack because DSP_CONTROL_MASK should make this bit
			// only viewable to dsp emulator
			if (val & 1 /*DSPReset*/)
			{
				g_audioDMA.AudioDMAControl.Hex = 0;
			}

			// Update DSP related flags
			g_dspState.DSPControl.DSPReset     = tmpControl.DSPReset;
			g_dspState.DSPControl.DSPAssertInt = tmpControl.DSPAssertInt;
			g_dspState.DSPControl.DSPHalt      = tmpControl.DSPHalt;
			g_dspState.DSPControl.DSPInit      = tmpControl.DSPInit;

			// Interrupt (mask)
			g_dspState.DSPControl.AID_mask  = tmpControl.AID_mask;
			g_dspState.DSPControl.ARAM_mask = tmpControl.ARAM_mask;
			g_dspState.DSPControl.DSP_mask  = tmpControl.DSP_mask;

			// Interrupt
			if (tmpControl.AID)  g_dspState.DSPControl.AID  = 0;
			if (tmpControl.ARAM) g_dspState.DSPControl.ARAM = 0;
			if (tmpControl.DSP)  g_dspState.DSPControl.DSP  = 0;

			// unknown
			g_dspState.DSPControl.DSPInitCode = tmpControl.DSPInitCode;
			g_dspState.DSPControl.pad  = tmpControl.pad;
			if (g_dspState.DSPControl.pad != 0)
			{
				PanicAlert("DSPInterface (w) g_dspState.DSPControl (CC00500A) gets a value with junk in the padding %08x", val);
			}

			UpdateInterrupts();
		})
	);

	// ARAM MMIO controlling the DMA start.
	mmio->Register(base | AR_DMA_CNT_L,
		MMIO::DirectRead<u16>(MMIO::Utils::LowPart(&g_arDMA.Cnt.Hex)),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			g_arDMA.Cnt.Hex = (g_arDMA.Cnt.Hex & 0xFFFF0000) | (val & ~31);
			Do_ARAM_DMA();
		})
	);

	// Audio DMA MMIO controlling the DMA start.
	mmio->Register(base | AUDIO_DMA_CONTROL_LEN,
		MMIO::DirectRead<u16>(&g_audioDMA.AudioDMAControl.Hex),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			bool already_enabled = g_audioDMA.AudioDMAControl.Enable;
			g_audioDMA.AudioDMAControl.Hex = val;

			// Only load new values if were not already doing a DMA transfer,
			// otherwise just let the new values be autoloaded in when the
			// current transfer ends.
			if (!already_enabled && g_audioDMA.AudioDMAControl.Enable)
			{
				g_audioDMA.current_source_address = g_audioDMA.SourceAddress;
				g_audioDMA.remaining_blocks_count = g_audioDMA.AudioDMAControl.NumBlocks;

				// We make the samples ready as soon as possible
				void *address = Memory::GetPointer(g_audioDMA.SourceAddress);
				AudioCommon::SendAIBuffer((short*)address, g_audioDMA.AudioDMAControl.NumBlocks * 8);
				CoreTiming::ScheduleEvent_Threadsafe(80, et_GenerateDSPInterrupt, INT_AID);
			}
		})
	);

	// Audio DMA blocks remaining is invalid to write to, and requires logic on
	// the read side.
	mmio->Register(base | AUDIO_DMA_BLOCKS_LEFT,
		MMIO::ComplexRead<u16>([](u32) {
			// remaining_blocks_count is zero-based.  DreamMix World Fighters will hang if it never reaches zero.
			return (g_audioDMA.remaining_blocks_count > 0 ? g_audioDMA.remaining_blocks_count - 1 : 0);
		}),
		MMIO::InvalidWrite<u16>()
	);

	// 32 bit reads/writes are a combination of two 16 bit accesses.
	for (int i = 0; i < 0x1000; i += 4)
	{
		mmio->Register(base | i,
			MMIO::ReadToSmaller<u32>(mmio, base | i, base | (i + 2)),
			MMIO::WriteToSmaller<u32>(mmio, base | i, base | (i + 2))
		);
	}
}

// UpdateInterrupts
static void UpdateInterrupts()
{
	// For each interrupt bit in DSP_CONTROL, the interrupt enablemask is the bit directly
	// to the left of it. By doing:
	// (DSP_CONTROL>>1) & DSP_CONTROL & MASK_OF_ALL_INTERRUPT_BITS
	// We can check if any of the interrupts are enabled and active, all at once.
	bool ints_set = (((g_dspState.DSPControl.Hex >> 1) & g_dspState.DSPControl.Hex & (INT_DSP | INT_ARAM | INT_AID)) != 0);

	ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_DSP, ints_set);
}

static void GenerateDSPInterrupt(u64 DSPIntType, int cyclesLate)
{
	// The INT_* enumeration members have values that reflect their bit positions in
	// DSP_CONTROL - we mask by (INT_DSP | INT_ARAM | INT_AID) just to ensure people
	// don't call this with bogus values.
	g_dspState.DSPControl.Hex |= (DSPIntType & (INT_DSP | INT_ARAM | INT_AID));

	UpdateInterrupts();
}

// CALLED FROM DSP EMULATOR, POSSIBLY THREADED
void GenerateDSPInterruptFromDSPEmu(DSPInterruptType type)
{
	CoreTiming::ScheduleEvent_Threadsafe_Immediate(et_GenerateDSPInterrupt, type);
}

// called whenever SystemTimers thinks the dsp deserves a few more cycles
void UpdateDSPSlice(int cycles)
{
	if (dsp_is_lle)
	{
		//use up the rest of the slice(if any)
		dsp_emulator->DSP_Update(dsp_slice);
		dsp_slice %= 6;
		//note the new budget
		dsp_slice += cycles;
	}
	else
	{
		dsp_emulator->DSP_Update(cycles);
	}
}

// This happens at 4 khz, since 32 bytes at 4khz = 4 bytes at 32 khz (16bit stereo pcm)
void UpdateAudioDMA()
{
	static short zero_samples[8*2] = { 0 };
	if (g_audioDMA.AudioDMAControl.Enable)
	{
		// Read audio at g_audioDMA.current_source_address in RAM and push onto an
		// external audio fifo in the emulator, to be mixed with the disc
		// streaming output.

		if (g_audioDMA.remaining_blocks_count != 0)
		{
			g_audioDMA.remaining_blocks_count--;
			g_audioDMA.current_source_address += 32;
		}

		if (g_audioDMA.remaining_blocks_count == 0)
		{
			g_audioDMA.current_source_address = g_audioDMA.SourceAddress;
			g_audioDMA.remaining_blocks_count = g_audioDMA.AudioDMAControl.NumBlocks;

			if (g_audioDMA.remaining_blocks_count != 0)
			{
				// We make the samples ready as soon as possible
				void *address = Memory::GetPointer(g_audioDMA.SourceAddress);
				AudioCommon::SendAIBuffer((short*)address, g_audioDMA.AudioDMAControl.NumBlocks * 8);
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
	g_dspState.DSPControl.DMAState = 1;

	// ARAM DMA transfer rate has been measured on real hw
	int ticksToTransfer = (g_arDMA.Cnt.count / 32) * 246;

	if (instant_dma)
		ticksToTransfer = 0;

	CoreTiming::ScheduleEvent_Threadsafe(ticksToTransfer, et_CompleteARAM);

	if (instant_dma)
		CoreTiming::ForceExceptionCheck(100);

	last_mmaddr = g_arDMA.MMAddr;
	last_aram_dma_count = g_arDMA.Cnt.count;

	// Real hardware DMAs in 32byte chunks, but we can get by with 8byte chunks
	if (g_arDMA.Cnt.dir)
	{
		// ARAM -> MRAM
		INFO_LOG(DSPINTERFACE, "DMA %08x bytes from ARAM %08x to MRAM %08x PC: %08x",
			g_arDMA.Cnt.count, g_arDMA.ARAddr, g_arDMA.MMAddr, PC);

		// Outgoing data from ARAM is mirrored every 64MB (verified on real HW)
		g_arDMA.ARAddr &= 0x3ffffff;
		g_arDMA.MMAddr &= 0x3ffffff;

		if (g_arDMA.ARAddr < g_ARAM.size)
		{
			while (g_arDMA.Cnt.count)
			{
				// These are logically separated in code to show that a memory map has been set up
				// See below in the write section for more information
				if ((g_ARAM_Info.Hex & 0xf) == 3)
				{
					Memory::Write_U64_Swap(*(u64*)&g_ARAM.ptr[g_arDMA.ARAddr & g_ARAM.mask], g_arDMA.MMAddr);
				}
				else if ((g_ARAM_Info.Hex & 0xf) == 4)
				{
					Memory::Write_U64_Swap(*(u64*)&g_ARAM.ptr[g_arDMA.ARAddr & g_ARAM.mask], g_arDMA.MMAddr);
				}
				else
				{
					Memory::Write_U64_Swap(*(u64*)&g_ARAM.ptr[g_arDMA.ARAddr & g_ARAM.mask], g_arDMA.MMAddr);
				}

				g_arDMA.MMAddr += 8;
				g_arDMA.ARAddr += 8;
				g_arDMA.Cnt.count -= 8;
			}
		}
		else
		{
			// Assuming no external ARAM installed; returns zeroes on out of bounds reads (verified on real HW)
			while (g_arDMA.Cnt.count)
			{
				Memory::Write_U64(0, g_arDMA.MMAddr);
				g_arDMA.MMAddr += 8;
				g_arDMA.ARAddr += 8;
				g_arDMA.Cnt.count -= 8;
			}
		}
	}
	else
	{
		// MRAM -> ARAM
		INFO_LOG(DSPINTERFACE, "DMA %08x bytes from MRAM %08x to ARAM %08x PC: %08x",
			g_arDMA.Cnt.count, g_arDMA.MMAddr, g_arDMA.ARAddr, PC);

		// Incoming data into ARAM is mirrored every 64MB (verified on real HW)
		g_arDMA.ARAddr &= 0x3ffffff;
		g_arDMA.MMAddr &= 0x3ffffff;

		if (g_arDMA.ARAddr < g_ARAM.size)
		{
			while (g_arDMA.Cnt.count)
			{
				if ((g_ARAM_Info.Hex & 0xf) == 3)
				{
					*(u64*)&g_ARAM.ptr[g_arDMA.ARAddr & g_ARAM.mask] = Common::swap64(Memory::Read_U64(g_arDMA.MMAddr));
				}
				else if ((g_ARAM_Info.Hex & 0xf) == 4)
				{
					if (g_arDMA.ARAddr < 0x400000)
					{
						*(u64*)&g_ARAM.ptr[(g_arDMA.ARAddr + 0x400000) & g_ARAM.mask] = Common::swap64(Memory::Read_U64(g_arDMA.MMAddr));
					}
					*(u64*)&g_ARAM.ptr[g_arDMA.ARAddr & g_ARAM.mask] = Common::swap64(Memory::Read_U64(g_arDMA.MMAddr));
				}
				else
				{
					*(u64*)&g_ARAM.ptr[g_arDMA.ARAddr & g_ARAM.mask] = Common::swap64(Memory::Read_U64(g_arDMA.MMAddr));
				}

				g_arDMA.MMAddr += 8;
				g_arDMA.ARAddr += 8;
				g_arDMA.Cnt.count -= 8;
			}
		}
		else
		{
			// Assuming no external ARAM installed; writes nothing to ARAM when out of bounds (verified on real HW)
			g_arDMA.MMAddr += g_arDMA.Cnt.count;
			g_arDMA.ARAddr += g_arDMA.Cnt.count;
			g_arDMA.Cnt.count = 0;
		}
	}
}

// (shuffle2) I still don't believe that this hack is actually needed... :(
// Maybe the Wii Sports ucode is processed incorrectly?
// (LM) It just means that dsp reads via '0xffdd' on WII can end up in EXRAM or main RAM
u8 ReadARAM(u32 _iAddress)
{
	//NOTICE_LOG(DSPINTERFACE, "ReadARAM 0x%08x", _iAddress);
	if (g_ARAM.wii_mode)
	{
		if (_iAddress & 0x10000000)
			return g_ARAM.ptr[_iAddress & g_ARAM.mask];
		else
			return Memory::Read_U8(_iAddress & Memory::RAM_MASK);
	}
	else
	{
		return g_ARAM.ptr[_iAddress & g_ARAM.mask];
	}
}

void WriteARAM(u8 value, u32 _uAddress)
{
	//NOTICE_LOG(DSPINTERFACE, "WriteARAM 0x%08x", _uAddress);
	//TODO: verify this on WII
	g_ARAM.ptr[_uAddress & g_ARAM.mask] = value;
}

u8 *GetARAMPtr()
{
	return g_ARAM.ptr;
}

u64 DMAInProgress()
{
	if (g_dspState.DSPControl.DMAState == 1)
	{
		return ((u64)last_mmaddr << 32 | (last_mmaddr + last_aram_dma_count));
	}
	return 0;
}

} // end of namespace DSP

