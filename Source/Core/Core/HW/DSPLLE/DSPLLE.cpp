// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <mutex>
#include <thread>

#include "Common/Atomic.h"
#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"
#include "Common/Event.h"
#include "Common/IniFile.h"
#include "Common/Logging/LogManager.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/DSP/DSPCaptureLogger.h"
#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPDisassembler.h"
#include "Core/DSP/DSPHost.h"
#include "Core/DSP/DSPHWInterface.h"
#include "Core/DSP/DSPInterpreter.h"
#include "Core/DSP/DSPTables.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/Memmap.h"

#include "Core/HW/DSPLLE/DSPLLE.h"
#include "Core/HW/DSPLLE/DSPLLEGlobals.h"
#include "Core/HW/DSPLLE/DSPSymbols.h"


DSPLLE::DSPLLE()
{
	m_bIsRunning = false;
	m_cycle_count = 0;
}

static Common::Event dspEvent;
static Common::Event ppcEvent;

void DSPLLE::DoState(PointerWrap &p)
{
	bool isHLE = false;
	p.Do(isHLE);
	if (isHLE != false && p.GetMode() == PointerWrap::MODE_READ)
	{
		Core::DisplayMessage("State is incompatible with current DSP engine. Aborting load state.", 3000);
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
	p.Do(g_dsp.ifx_regs);
	p.Do(g_dsp.mbox[0]);
	p.Do(g_dsp.mbox[1]);
	UnWriteProtectMemory(g_dsp.iram, DSP_IRAM_BYTE_SIZE, false);
	p.DoArray(g_dsp.iram, DSP_IRAM_SIZE);
	WriteProtectMemory(g_dsp.iram, DSP_IRAM_BYTE_SIZE, false);
	if (p.GetMode() == PointerWrap::MODE_READ)
		DSPHost::CodeLoaded((const u8*)g_dsp.iram, DSP_IRAM_BYTE_SIZE);
	p.DoArray(g_dsp.dram, DSP_DRAM_SIZE);
	p.Do(cyclesLeft);
	p.Do(init_hax);
	p.Do(m_cycle_count);
}

// Regular thread
void DSPLLE::dsp_thread(DSPLLE *dsp_lle)
{
	Common::SetCurrentThreadName("DSP thread");

	while (dsp_lle->m_bIsRunning)
	{
		int cycles = (int)dsp_lle->m_cycle_count;
		if (cycles > 0)
		{
			std::lock_guard<std::mutex> lk(dsp_lle->m_csDSPThreadActive);
			if (dspjit)
			{
				DSPCore_RunCycles(cycles);
			}
			else
			{
				DSPInterpreter::RunCyclesThread(cycles);
			}
			Common::AtomicStore(dsp_lle->m_cycle_count, 0);
		}
		else
		{
			ppcEvent.Set();
			dspEvent.Wait();
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
		ERROR_LOG(DSPLLE, "%s has a wrong size (%u, expected %u)",
		          filename.c_str(), (u32)bytes.size(), size_in_bytes);
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

	opts->core_type = SConfig::GetInstance().m_DSPEnableJIT ?
		DSPInitOptions::CORE_JIT : DSPInitOptions::CORE_INTERPRETER;

	if (SConfig::GetInstance().m_DSPCaptureLog)
	{
		const std::string pcap_path = File::GetUserPath(D_DUMPDSP_IDX) + "dsp.pcap";
		opts->capture_logger = new PCAPDSPCaptureLogger(pcap_path);
	}

	return true;
}

bool DSPLLE::Initialize(bool bWii, bool bDSPThread)
{
	m_bWii = bWii;
	m_bDSPThread = bDSPThread;

	DSPInitOptions opts;
	if (!FillDSPInitOptions(&opts))
		return false;
	if (!DSPCore_Init(opts))
		return false;

	g_dsp.cpu_ram = Memory::m_pRAM;
	DSPCore_Reset();

	m_bIsRunning = true;

	InitInstructionTable();

	if (m_bDSPThread)
		m_hDSPThread = std::thread(dsp_thread, this);

	Host_RefreshDSPDebuggerWindow();

	return true;
}

void DSPLLE::DSP_StopSoundStream()
{
	DSPInterpreter::Stop();
	m_bIsRunning = false;
	if (m_bDSPThread)
	{
		ppcEvent.Set();
		dspEvent.Set();
		m_hDSPThread.join();
	}
}

void DSPLLE::Shutdown()
{
	DSPCore_Shutdown();
}

u16 DSPLLE::DSP_WriteControlRegister(u16 _uFlag)
{
	DSPInterpreter::WriteCR(_uFlag);

	// Check if the CPU has set an external interrupt (CR_EXTERNAL_INT)
	// and immediately process it, if it has.
	if (_uFlag & 2)
	{
		if (!m_bDSPThread)
		{
			DSPCore_CheckExternalInterrupt();
			DSPCore_CheckExceptions();
		}
		else
		{
			DSPCore_SetExternalInterrupt(true);
		}

	}

	return DSPInterpreter::ReadCR();
}

u16 DSPLLE::DSP_ReadControlRegister()
{
	return DSPInterpreter::ReadCR();
}

u16 DSPLLE::DSP_ReadMailBoxHigh(bool _CPUMailbox)
{
	if (_CPUMailbox)
		return gdsp_mbox_read_h(GDSP_MBOX_CPU);
	else
		return gdsp_mbox_read_h(GDSP_MBOX_DSP);
}

u16 DSPLLE::DSP_ReadMailBoxLow(bool _CPUMailbox)
{
	if (_CPUMailbox)
		return gdsp_mbox_read_l(GDSP_MBOX_CPU);
	else
		return gdsp_mbox_read_l(GDSP_MBOX_DSP);
}

void DSPLLE::DSP_WriteMailBoxHigh(bool _CPUMailbox, u16 _uHighMail)
{
	if (_CPUMailbox)
	{
		if (gdsp_mbox_peek(GDSP_MBOX_CPU) & 0x80000000)
		{
			ERROR_LOG(DSPLLE, "Mailbox isnt empty ... strange");
		}

#if PROFILE
		if ((_uHighMail) == 0xBABE)
		{
			ProfilerStart();
		}
#endif

		gdsp_mbox_write_h(GDSP_MBOX_CPU, _uHighMail);
	}
	else
	{
		ERROR_LOG(DSPLLE, "CPU can't write to DSP mailbox");
	}
}

void DSPLLE::DSP_WriteMailBoxLow(bool _CPUMailbox, u16 _uLowMail)
{
	if (_CPUMailbox)
	{
		gdsp_mbox_write_l(GDSP_MBOX_CPU, _uLowMail);
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
// Sound stream update job has been handled by AudioDMA routine, which is more efficient
/*
	// This gets called VERY OFTEN. The soundstream update might be expensive so only do it 200 times per second or something.
	int cycles_between_ss_update;

	if (g_dspInitialize.bWii)
		cycles_between_ss_update = 121500000 / 200;
	else
		cycles_between_ss_update = 81000000 / 200;

	m_cycle_count += cycles;
	if (m_cycle_count > cycles_between_ss_update)
	{
		while (m_cycle_count > cycles_between_ss_update)
			m_cycle_count -= cycles_between_ss_update;
		soundStream->Update();
	}
*/
	// If we're not on a thread, run cycles here.
	if (!m_bDSPThread)
	{
		// ~1/6th as many cycles as the period PPC-side.
		DSPCore_RunCycles(dsp_cycles);
	}
	else
	{
		// Wait for dsp thread to complete its cycle. Note: this logic should be thought through.
		ppcEvent.Wait();
		Common::AtomicStore(m_cycle_count, dsp_cycles);
		dspEvent.Set();

	}
}

u32 DSPLLE::DSP_UpdateRate()
{
	return 12600; // TO BE TWEAKED
}

void DSPLLE::PauseAndLock(bool doLock, bool unpauseOnUnlock)
{
	if (doLock)
		m_csDSPThreadActive.lock();
	else
		m_csDSPThreadActive.unlock();
}

