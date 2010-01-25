// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

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

#include "DSP.h"

#include "../CoreTiming.h"
#include "../Core.h"
#include "CPU.h"
#include "MemoryUtil.h"
#include "Memmap.h"
#include "ProcessorInterface.h"
#include "AudioInterface.h"
#include "../PowerPC/PowerPC.h"
#include "../PluginManager.h"
#include "../ConfigManager.h"

namespace DSP
{

// register offsets
enum
{
	DSP_MAIL_TO_DSP_HI		= 0x5000,
	DSP_MAIL_TO_DSP_LO		= 0x5002,
	DSP_MAIL_FROM_DSP_HI	= 0x5004,
	DSP_MAIL_FROM_DSP_LO	= 0x5006,
	DSP_CONTROL				= 0x500A,
	DSP_INTERRUPT_CONTROL   = 0x5010,
	AR_INFO					= 0x5012, // These names are a good guess at best
	AR_MODE					= 0x5016, //
	AR_REFRESH				= 0x501a,
	AR_DMA_MMADDR_H			= 0x5020,
	AR_DMA_MMADDR_L			= 0x5022,
	AR_DMA_ARADDR_H			= 0x5024,
	AR_DMA_ARADDR_L			= 0x5026,
	AR_DMA_CNT_H			= 0x5028,
	AR_DMA_CNT_L			= 0x502A,
	AUDIO_DMA_START_HI		= 0x5030,
	AUDIO_DMA_START_LO		= 0x5032,
	AUDIO_DMA_BLOCKS_LENGTH	= 0x5034, // Ever used?
	AUDIO_DMA_CONTROL_LEN	= 0x5036,
	AUDIO_DMA_BLOCKS_LEFT	= 0x503A,
};

// UARAMCount
union UARAMCount
{
	u32 Hex;
	struct
	{
		u32 count	: 31;
		u32 dir		: 1; // 0: MRAM -> ARAM 1: ARAM -> MRAM
	};
};

// UDSPControl
#define DSP_CONTROL_MASK 0x0C07
union UDSPControl
{
	u16 Hex;
	struct
	{
		// DSP Control
		u16 DSPReset		: 1;	// Write 1 to reset and waits for 0
		u16 DSPAssertInt	: 1;
		u16 DSPHalt			: 1;
		// Interrupt for DMA to the AI/speakers
		u16 AID				: 1;
		u16 AID_mask		: 1;
		// ARAM DMA interrupt
		u16 ARAM			: 1;
		u16 ARAM_mask		: 1;
		// DSP DMA interrupt
		u16 DSP				: 1;
		u16 DSP_mask		: 1;
		// Other ???
		u16 DMAState		: 1;	// DSPGetDMAStatus() uses this flag. __ARWaitForDMA() uses it too...maybe it's just general DMA flag
		u16 unk3			: 1;
		u16 DSPInit			: 1;	// DSPInit() writes to this flag
		u16 pad				: 4;
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
	u32 SourceAddress;
	u32 ReadAddress;
	UAudioDMAControl AudioDMAControl;
	int BlocksLeft;

	AudioDMA()
	{
		SourceAddress = 0;
		ReadAddress = 0;
		AudioDMAControl.Hex = 0;
		BlocksLeft = 0;
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

// So we may abstract gc/wii differences a little
struct ARAMInfo
{
	bool wii_mode; // wii EXRAM is managed in Memory:: so we need to skip statesaving, etc
	u32 size;
	u32 mask;
	u8* ptr; // aka audio ram, auxiliary ram, MEM2, EXRAM, etc...

	// Default to GC mode
	ARAMInfo() {
		wii_mode = false;
		size = ARAM_SIZE;
		mask = ARAM_MASK;
		ptr = NULL;
	}
};

// STATE_TO_SAVE
static ARAMInfo g_ARAM;
static DSPState g_dspState;
static AudioDMA g_audioDMA;
static ARAM_DMA g_arDMA;

union ARAM_Info
{
	u16 Hex;
	struct
	{
		u16 size	: 6;
		u16 unk		: 1;
		u16			: 9;
	};
};
static ARAM_Info g_ARAM_Info;
// Contains bitfields for some stuff we don't care about (and nothing ever reads):
//  CAS latency/burst length/addressing mode/write mode
// We care about the LSB tho. It indicates that the ARAM controller has finished initializing
static u16 g_AR_MODE;
static u16 g_AR_REFRESH;

Common::PluginDSP *dsp_plugin;


void DoState(PointerWrap &p)
{
	if (!g_ARAM.wii_mode)
		p.DoArray(g_ARAM.ptr, g_ARAM.size);
	p.Do(g_dspState);
	p.Do(g_audioDMA);
	p.Do(g_arDMA);
	p.Do(g_ARAM_Info);
	p.Do(g_AR_MODE);
	p.Do(g_AR_REFRESH);
}


void UpdateInterrupts();
void Do_ARAM_DMA();
void WriteARAM(u8 _iValue, u32 _iAddress);
bool Update_DSP_ReadRegister();
void Update_DSP_WriteRegister();

int et_GenerateDSPInterrupt;

void GenerateDSPInterrupt_Wrapper(u64 userdata, int cyclesLate)
{
	GenerateDSPInterrupt((DSPInterruptType)(userdata&0xFFFF), (bool)((userdata>>16) & 1));
}

void Init()
{
	dsp_plugin = CPluginManager::GetInstance().GetDSP();

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
	{
		// On the Wii, ARAM is simply mapped to EXRAM.
		g_ARAM.wii_mode = true;
		g_ARAM.size = Memory::EXRAM_SIZE;
		g_ARAM.mask = Memory::EXRAM_MASK;
		g_ARAM.ptr = Memory::GetPointer(0x10000000);
	}
	else
	{
		// On the GC, ARAM is accessible only through this interface (unless you're doing mmu tricks?...)
		g_ARAM.wii_mode = false;
		g_ARAM.size = ARAM_SIZE;
		g_ARAM.mask = ARAM_MASK;
		g_ARAM.ptr = (u8 *)AllocateMemoryPages(g_ARAM.size);
	}

	g_audioDMA.AudioDMAControl.Hex = 0;

	g_dspState.DSPControl.Hex = 0;
    g_dspState.DSPControl.DSPHalt = 1;
	
	g_ARAM_Info.Hex = 0;
	g_AR_MODE = 1; // ARAM Controller has init'd
	g_AR_REFRESH = 156; // 156MHz

	et_GenerateDSPInterrupt = CoreTiming::RegisterEvent("DSPint", GenerateDSPInterrupt_Wrapper);
}

void Shutdown()
{
	if (!g_ARAM.wii_mode)
		FreeMemoryPages(g_ARAM.ptr, g_ARAM.size);
	g_ARAM.ptr = NULL;

	dsp_plugin = NULL;
}

void Read16(u16& _uReturnValue, const u32 _iAddress)
{
	switch (_iAddress & 0xFFFF)
	{
		// DSP
	case DSP_MAIL_TO_DSP_HI:
		_uReturnValue = dsp_plugin->DSP_ReadMailboxHigh(true);
		break;

	case DSP_MAIL_TO_DSP_LO:
		_uReturnValue = dsp_plugin->DSP_ReadMailboxLow(true);
		break;

	case DSP_MAIL_FROM_DSP_HI:
		_uReturnValue = dsp_plugin->DSP_ReadMailboxHigh(false);
		break;

	case DSP_MAIL_FROM_DSP_LO:
		_uReturnValue = dsp_plugin->DSP_ReadMailboxLow(false);
		break;

	case DSP_CONTROL:
		_uReturnValue = (g_dspState.DSPControl.Hex & ~DSP_CONTROL_MASK) |
			(dsp_plugin->DSP_ReadControlRegister() & DSP_CONTROL_MASK);
		break;

		// ARAM
	case AR_INFO:
		PanicAlert("read %x %x", g_ARAM_Info.Hex,PowerPC::ppcState.pc);
		_uReturnValue = g_ARAM_Info.Hex;
		break;

	case AR_MODE:
		_uReturnValue = g_AR_MODE;
		break;

	case AR_REFRESH:
		_uReturnValue = g_AR_REFRESH;
		break;

	case AR_DMA_MMADDR_H: _uReturnValue = g_arDMA.MMAddr >> 16; return;
	case AR_DMA_MMADDR_L: _uReturnValue = g_arDMA.MMAddr & 0xFFFF; return;
	case AR_DMA_ARADDR_H: _uReturnValue = g_arDMA.ARAddr >> 16; return;
	case AR_DMA_ARADDR_L: _uReturnValue = g_arDMA.ARAddr & 0xFFFF; return;
	case AR_DMA_CNT_H:    _uReturnValue = g_arDMA.Cnt.Hex >> 16; return;
	case AR_DMA_CNT_L:    _uReturnValue = g_arDMA.Cnt.Hex & 0xFFFF; return;

		// AI
	case AUDIO_DMA_BLOCKS_LEFT:
		_uReturnValue = g_audioDMA.BlocksLeft;
		break;

	case AUDIO_DMA_START_LO:
		_uReturnValue = g_audioDMA.SourceAddress & 0xFFFF;
		break;

	case AUDIO_DMA_START_HI:
		_uReturnValue = g_audioDMA.SourceAddress >> 16;
		break;

	case AUDIO_DMA_CONTROL_LEN:
		_uReturnValue = g_audioDMA.AudioDMAControl.Hex;
		break;

	default:
		_dbg_assert_(DSPINTERFACE,0);
		break;
	}

	if (_iAddress != (0xCC000000 + DSP_MAIL_FROM_DSP_HI))
	{
		DEBUG_LOG(DSPINTERFACE, "DSPInterface(r16) 0x%08x (%02x)  (%08x)", _iAddress, _uReturnValue, PowerPC::ppcState.pc);
	}
}

void Write16(const u16 _Value, const u32 _Address)
{
	DEBUG_LOG(DSPINTERFACE, "DSPInterface(w16) 0x%04x 0x%08x", _Value, _Address);

	switch (_Address & 0xFFFF)
	{
	// DSP
	case DSP_MAIL_TO_DSP_HI:
		dsp_plugin->DSP_WriteMailboxHigh(true, _Value);
		break;

	case DSP_MAIL_TO_DSP_LO:
		dsp_plugin->DSP_WriteMailboxLow(true, _Value);
		break;

	case DSP_MAIL_FROM_DSP_HI:
		_dbg_assert_msg_(DSPINTERFACE, 0, "W16: DSP_MAIL_FROM_DSP_HI");
		break;

	case DSP_MAIL_FROM_DSP_LO:
		_dbg_assert_msg_(DSPINTERFACE, 0, "W16: DSP_MAIL_FROM_DSP_LO");
		break;

	// Control Register
	case DSP_CONTROL:
		{
			UDSPControl tmpControl;
			tmpControl.Hex = (_Value & ~DSP_CONTROL_MASK) |
							(dsp_plugin->DSP_WriteControlRegister(_Value) & DSP_CONTROL_MASK);

			// Update DSP related flags
			g_dspState.DSPControl.DSPReset		= tmpControl.DSPReset;
			g_dspState.DSPControl.DSPAssertInt	= tmpControl.DSPAssertInt;
			g_dspState.DSPControl.DSPHalt		= tmpControl.DSPHalt;
			g_dspState.DSPControl.DSPInit       = tmpControl.DSPInit;

			// Interrupt (mask)
			g_dspState.DSPControl.AID_mask	= tmpControl.AID_mask;
			g_dspState.DSPControl.ARAM_mask	= tmpControl.ARAM_mask;
			g_dspState.DSPControl.DSP_mask	= tmpControl.DSP_mask;

			// Interrupt
			if (tmpControl.AID)  g_dspState.DSPControl.AID  = 0;
			if (tmpControl.ARAM) g_dspState.DSPControl.ARAM = 0;
			if (tmpControl.DSP)  g_dspState.DSPControl.DSP  = 0;

			// g_ARAM
			g_dspState.DSPControl.DMAState = 0;	// keep g_ARAM DMA State zero

			// unknown					
			g_dspState.DSPControl.unk3	= tmpControl.unk3;
			g_dspState.DSPControl.pad   = tmpControl.pad;
			if (g_dspState.DSPControl.pad != 0)
			{
				PanicAlert("DSPInterface (w) g_dspState.DSPControl (CC00500A) gets a value with junk in the padding %08x", _Value);
			}

			UpdateInterrupts();
		}			
		break;

	// ARAM
	// DMA back and forth between ARAM and RAM
	case AR_INFO:
		PanicAlert("write %x %x", _Value,PowerPC::ppcState.pc);
		g_ARAM_Info.Hex = _Value;
		// __OSInitAudioSystem sets to 0x43 -> expects 16bit adressing and mapping to dsp iram?
		// __OSCheckSize sets = 0x20 | 3 (keeps upper bits)
		// 0x23 -> Zelda standard mode (standard ARAM access ??)
		// 0x63 -> ARCheckSize Mode (access AR-registers ??) or no exception ??
		break;

	case AR_MODE:
		g_AR_MODE = _Value;
		break;

	case AR_REFRESH:
		g_AR_REFRESH = _Value;
		break;

	case AR_DMA_MMADDR_H:
		g_arDMA.MMAddr = (g_arDMA.MMAddr & 0xFFFF) | (_Value<<16); break;
	case AR_DMA_MMADDR_L:
		g_arDMA.MMAddr = (g_arDMA.MMAddr & 0xFFFF0000) | (_Value); break;

	case AR_DMA_ARADDR_H:
		g_arDMA.ARAddr = (g_arDMA.ARAddr & 0xFFFF) | (_Value<<16); break;
	case AR_DMA_ARADDR_L:
		g_arDMA.ARAddr = (g_arDMA.ARAddr & 0xFFFF0000) | (_Value); break;

	case AR_DMA_CNT_H:
		g_arDMA.Cnt.Hex = (g_arDMA.Cnt.Hex & 0xFFFF) | (_Value<<16);
		break;

	case AR_DMA_CNT_L:
		g_arDMA.Cnt.Hex = (g_arDMA.Cnt.Hex & 0xFFFF0000) | (_Value);
		Do_ARAM_DMA();
		break;

	// AI
	// This is the DMA that goes straight out the speaker.
	case AUDIO_DMA_START_HI:
		g_audioDMA.SourceAddress = (g_audioDMA.SourceAddress & 0xFFFF) | (_Value<<16);
		break;

	case AUDIO_DMA_START_LO:
		g_audioDMA.SourceAddress = (g_audioDMA.SourceAddress & 0xFFFF0000) | (_Value);
		break;

	case AUDIO_DMA_CONTROL_LEN:			// called by AIStartDMA()
		g_audioDMA.AudioDMAControl.Hex = _Value;
		g_audioDMA.ReadAddress = g_audioDMA.SourceAddress;
		g_audioDMA.BlocksLeft = g_audioDMA.AudioDMAControl.NumBlocks;
		INFO_LOG(DSPINTERFACE, "AID DMA started - source address %08x, length %i blocks", g_audioDMA.SourceAddress, g_audioDMA.AudioDMAControl.NumBlocks);
		break;

	case AUDIO_DMA_BLOCKS_LEFT:
		_dbg_assert_(DSPINTERFACE,0);
		break;

	default:
		_dbg_assert_(DSPINTERFACE,0);
		break;
	}
}

void Read32(u32& _uReturnValue, const u32 _iAddress)
{
	INFO_LOG(DSPINTERFACE, "DSPInterface(r32) 0x%08x", _iAddress);
	switch (_iAddress & 0xFFFF)
	{
		// DSP
	case DSP_MAIL_TO_DSP_HI:
		_uReturnValue = (dsp_plugin->DSP_ReadMailboxHigh(true) << 16) | dsp_plugin->DSP_ReadMailboxLow(true);
		break;

	default:
		_dbg_assert_(DSPINTERFACE,0);
		break;
	}
	_uReturnValue = 0;
}

void Write32(const u32 _iValue, const u32 _iAddress)
{
	INFO_LOG(DSPINTERFACE, "DSPInterface(w32) 0x%08x 0x%08x", _iValue, _iAddress);

	switch (_iAddress & 0xFFFF)
	{
		// DSP
	case DSP_MAIL_TO_DSP_HI:
		dsp_plugin->DSP_WriteMailboxHigh(true, _iValue >> 16);
		dsp_plugin->DSP_WriteMailboxLow(true, (u16)_iValue);
		break;

		// AI
	case AUDIO_DMA_START_HI:
		g_audioDMA.SourceAddress = _iValue;
		break;

		// ARAM
	case AR_DMA_MMADDR_H:
		g_arDMA.MMAddr = _iValue;
		break;

	case AR_DMA_ARADDR_H:
		g_arDMA.ARAddr = _iValue;
		break;

	case AR_DMA_CNT_H:   
		g_arDMA.Cnt.Hex = _iValue;
		Do_ARAM_DMA();
		break;

	default:
		_dbg_assert_(DSPINTERFACE,0);
		break;
	}
}


// UpdateInterrupts
void UpdateInterrupts()
{
	if ((g_dspState.DSPControl.AID  & g_dspState.DSPControl.AID_mask) ||
		(g_dspState.DSPControl.ARAM & g_dspState.DSPControl.ARAM_mask) ||
		(g_dspState.DSPControl.DSP  & g_dspState.DSPControl.DSP_mask))
	{
		ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_DSP, true);
	}
	else
	{
		ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_DSP, false);
	}
}

void GenerateDSPInterrupt(DSPInterruptType type, bool _bSet)
{
	switch (type)
	{
	case INT_DSP:	g_dspState.DSPControl.DSP		= _bSet ? 1 : 0; break;
	case INT_ARAM:	g_dspState.DSPControl.ARAM	    = _bSet ? 1 : 0; break;
	case INT_AID:	g_dspState.DSPControl.AID		= _bSet ? 1 : 0; break;
	}

	UpdateInterrupts();
}

// CALLED FROM DSP PLUGIN, POSSIBLY THREADED
void GenerateDSPInterruptFromPlugin(DSPInterruptType type, bool _bSet)
{
	CoreTiming::ScheduleEvent_Threadsafe(
		0, et_GenerateDSPInterrupt, type | (_bSet<<16));
}

// This happens at 4 khz, since 32 bytes at 4khz = 4 bytes at 32 khz (16bit stereo pcm)
void UpdateAudioDMA()
{
	if (g_audioDMA.AudioDMAControl.Enable && g_audioDMA.BlocksLeft)
	{
		// Read audio at g_audioDMA.ReadAddress in RAM and push onto an
		// external audio fifo in the emulator, to be mixed with the disc
		// streaming output. If that audio queue fills up, we delay the
		// emulator.

		// AyuanX: let's do it in a bundle to speed up
		if (g_audioDMA.BlocksLeft == g_audioDMA.AudioDMAControl.NumBlocks)
			dsp_plugin->DSP_SendAIBuffer(g_audioDMA.SourceAddress, g_audioDMA.AudioDMAControl.NumBlocks * 8);

		//g_audioDMA.ReadAddress += 32;
		g_audioDMA.BlocksLeft--;

		if (g_audioDMA.BlocksLeft == 0)
		{
			GenerateDSPInterrupt(DSP::INT_AID);
			//g_audioDMA.ReadAddress = g_audioDMA.SourceAddress;
			g_audioDMA.BlocksLeft = g_audioDMA.AudioDMAControl.NumBlocks;
			//DEBUG_LOG(DSPLLE, "ADMA read addresses: %08x", g_audioDMA.ReadAddress);
		}
	}
	else
	{
		// Send silence. Yeah, it's a bit of a waste to sample rate convert
		// silence.  or hm. Maybe we shouldn't do this :)
		// dsp->DSP_SendAIBuffer(0, AudioInterface::GetDSPSampleRate());
	}
}

void Do_ARAM_DMA()
{
	// Real hardware DMAs in 32byte chunks, but we can get by with 8byte chunks
	if (g_arDMA.Cnt.dir)
	{
		// ARAM -> MRAM
		INFO_LOG(DSPINTERFACE, "DMA %08x bytes from ARAM %08x to MRAM %08x",
			g_arDMA.Cnt.count, g_arDMA.ARAddr, g_arDMA.MMAddr);

		while (g_arDMA.Cnt.count)
		{
			if (g_arDMA.ARAddr < g_ARAM.size)
			{
				Memory::Write_U64_Swap(*(u64*)&g_ARAM.ptr[g_arDMA.ARAddr], g_arDMA.MMAddr);
				g_arDMA.MMAddr += 8;
				g_arDMA.ARAddr += 8;
			}
			g_arDMA.Cnt.count -= 8;
		}
	}
	else
	{
		// MRAM -> ARAM
		INFO_LOG(DSPINTERFACE, "DMA %08x bytes from MRAM %08x to ARAM %08x",
			g_arDMA.Cnt.count, g_arDMA.MMAddr, g_arDMA.ARAddr);

		while (g_arDMA.Cnt.count)
		{
			if (g_arDMA.ARAddr < g_ARAM.size)
			{
				*(u64*)&g_ARAM.ptr[g_arDMA.ARAddr] = Common::swap64(Memory::Read_U64(g_arDMA.MMAddr));
				g_arDMA.MMAddr += 8;
				g_arDMA.ARAddr += 8;
			}
			g_arDMA.Cnt.count -= 8;
		}
	}

	GenerateDSPInterrupt(INT_ARAM);
}


// (shuffle2) I still don't believe that this hack is actually needed... :(
// Maybe the wii sports ucode is processed incorrectly?
u8 ReadARAM(u32 _iAddress)
{
	//NOTICE_LOG(DSPINTERFACE, "ReadARAM 0x%08x (0x%08x)", _iAddress, _iAddress & g_ARAM.mask);
	if (g_ARAM.wii_mode && _iAddress < Memory::REALRAM_SIZE)
		return Memory::Read_U8(_iAddress);
	else
		return g_ARAM.ptr[_iAddress & g_ARAM.mask];
}

void WriteARAM(u8 value, u32 _uAddress)
{
	//NOTICE_LOG(DSPINTERFACE, "WriteARAM 0x%08x (0x%08x)", _uAddress, _uAddress & g_ARAM.mask);
	g_ARAM.ptr[_uAddress & g_ARAM.mask] = value;
}

u8 *GetARAMPtr()
{
	return g_ARAM.ptr;
}

} // end of namespace DSP

